// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/in_process_command_buffer.h"

#include <queue>
#include <utility>

#include <GLES2/gl2.h>
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequence_checker.h"
#include "base/threading/thread.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gpu_control_service.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_share_group.h"

#if defined(OS_ANDROID)
#include "gpu/command_buffer/service/stream_texture_manager_in_process_android.h"
#include "ui/gl/android/surface_texture_bridge.h"
#endif

namespace gpu {

namespace {

static base::LazyInstance<std::set<InProcessCommandBuffer*> >
    g_all_shared_contexts = LAZY_INSTANCE_INITIALIZER;

static bool g_use_virtualized_gl_context = false;
static bool g_uses_explicit_scheduling = false;
static GpuMemoryBufferFactory* g_gpu_memory_buffer_factory = NULL;

template <typename T>
static void RunTaskWithResult(base::Callback<T(void)> task,
                              T* result,
                              base::WaitableEvent* completion) {
  *result = task.Run();
  completion->Signal();
}

class GpuInProcessThread
    : public base::Thread,
      public base::RefCountedThreadSafe<GpuInProcessThread> {
 public:
  GpuInProcessThread();

 private:
  friend class base::RefCountedThreadSafe<GpuInProcessThread>;
  virtual ~GpuInProcessThread();

  DISALLOW_COPY_AND_ASSIGN(GpuInProcessThread);
};

GpuInProcessThread::GpuInProcessThread() : base::Thread("GpuThread") {
  Start();
}

GpuInProcessThread::~GpuInProcessThread() {
  Stop();
}

// Used with explicit scheduling when there is no dedicated GPU thread.
class GpuCommandQueue {
 public:
  GpuCommandQueue();
  ~GpuCommandQueue();

  void QueueTask(const base::Closure& task);
  void RunTasks();
  void SetScheduleCallback(const base::Closure& callback);

 private:
  base::Lock tasks_lock_;
  std::queue<base::Closure> tasks_;
  base::Closure schedule_callback_;

  DISALLOW_COPY_AND_ASSIGN(GpuCommandQueue);
};

GpuCommandQueue::GpuCommandQueue() {}

GpuCommandQueue::~GpuCommandQueue() {
  base::AutoLock lock(tasks_lock_);
  DCHECK(tasks_.empty());
}

void GpuCommandQueue::QueueTask(const base::Closure& task) {
  {
    base::AutoLock lock(tasks_lock_);
    tasks_.push(task);
  }

  DCHECK(!schedule_callback_.is_null());
  schedule_callback_.Run();
}

void GpuCommandQueue::RunTasks() {
  size_t num_tasks;
  {
    base::AutoLock lock(tasks_lock_);
    num_tasks = tasks_.size();
  }

  while (num_tasks) {
    base::Closure task;
    {
      base::AutoLock lock(tasks_lock_);
      task = tasks_.front();
      tasks_.pop();
      num_tasks = tasks_.size();
    }

    task.Run();
  }
}

void GpuCommandQueue::SetScheduleCallback(const base::Closure& callback) {
  DCHECK(schedule_callback_.is_null());
  schedule_callback_ = callback;
}

static base::LazyInstance<GpuCommandQueue> g_gpu_queue =
    LAZY_INSTANCE_INITIALIZER;

class SchedulerClientBase : public InProcessCommandBuffer::SchedulerClient {
 public:
  explicit SchedulerClientBase(bool need_thread);
  virtual ~SchedulerClientBase();

  static bool HasClients();

 protected:
  scoped_refptr<GpuInProcessThread> thread_;

 private:
  static base::LazyInstance<std::set<SchedulerClientBase*> > all_clients_;
  static base::LazyInstance<base::Lock> all_clients_lock_;
};

base::LazyInstance<std::set<SchedulerClientBase*> >
    SchedulerClientBase::all_clients_ = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<base::Lock> SchedulerClientBase::all_clients_lock_ =
    LAZY_INSTANCE_INITIALIZER;

SchedulerClientBase::SchedulerClientBase(bool need_thread) {
  base::AutoLock(all_clients_lock_.Get());
  if (need_thread) {
    if (!all_clients_.Get().empty()) {
      SchedulerClientBase* other = *all_clients_.Get().begin();
      thread_ = other->thread_;
      DCHECK(thread_.get());
    } else {
      thread_ = new GpuInProcessThread;
    }
  }
  all_clients_.Get().insert(this);
}

SchedulerClientBase::~SchedulerClientBase() {
  base::AutoLock(all_clients_lock_.Get());
  all_clients_.Get().erase(this);
}

bool SchedulerClientBase::HasClients() {
  base::AutoLock(all_clients_lock_.Get());
  return !all_clients_.Get().empty();
}

// A client that talks to the GPU thread
class ThreadClient : public SchedulerClientBase {
 public:
  ThreadClient();
  virtual void QueueTask(const base::Closure& task) OVERRIDE;
};

ThreadClient::ThreadClient() : SchedulerClientBase(true) {
  DCHECK(thread_.get());
}

void ThreadClient::QueueTask(const base::Closure& task) {
  thread_->message_loop()->PostTask(FROM_HERE, task);
}

// A client that talks to the GpuCommandQueue
class QueueClient : public SchedulerClientBase {
 public:
  QueueClient();
  virtual void QueueTask(const base::Closure& task) OVERRIDE;
};

QueueClient::QueueClient() : SchedulerClientBase(false) {
  DCHECK(!thread_.get());
}

void QueueClient::QueueTask(const base::Closure& task) {
  g_gpu_queue.Get().QueueTask(task);
}

static scoped_ptr<InProcessCommandBuffer::SchedulerClient>
CreateSchedulerClient() {
  scoped_ptr<InProcessCommandBuffer::SchedulerClient> client;
  if (g_uses_explicit_scheduling)
    client.reset(new QueueClient);
  else
    client.reset(new ThreadClient);

  return client.Pass();
}

class ScopedEvent {
 public:
  ScopedEvent(base::WaitableEvent* event) : event_(event) {}
  ~ScopedEvent() { event_->Signal(); }

 private:
  base::WaitableEvent* event_;
};

}  // anonyous namespace

InProcessCommandBuffer::InProcessCommandBuffer()
    : context_lost_(false),
      share_group_id_(0),
      last_put_offset_(-1),
      flush_event_(false, false),
      queue_(CreateSchedulerClient()) {}

InProcessCommandBuffer::~InProcessCommandBuffer() {
  Destroy();
}

bool InProcessCommandBuffer::IsContextLost() {
  CheckSequencedThread();
  if (context_lost_ || !command_buffer_) {
    return true;
  }
  CommandBuffer::State state = GetState();
  return error::IsError(state.error);
}

void InProcessCommandBuffer::OnResizeView(gfx::Size size, float scale_factor) {
  CheckSequencedThread();
  DCHECK(!surface_->IsOffscreen());
  surface_->Resize(size);
}

bool InProcessCommandBuffer::MakeCurrent() {
  CheckSequencedThread();
  command_buffer_lock_.AssertAcquired();

  if (!context_lost_ && decoder_->MakeCurrent())
    return true;
  DLOG(ERROR) << "Context lost because MakeCurrent failed.";
  command_buffer_->SetContextLostReason(decoder_->GetContextLostReason());
  command_buffer_->SetParseError(gpu::error::kLostContext);
  return false;
}

void InProcessCommandBuffer::PumpCommands() {
  CheckSequencedThread();
  command_buffer_lock_.AssertAcquired();

  if (!MakeCurrent())
    return;

  gpu_scheduler_->PutChanged();
}

bool InProcessCommandBuffer::GetBufferChanged(int32 transfer_buffer_id) {
  CheckSequencedThread();
  command_buffer_lock_.AssertAcquired();
  command_buffer_->SetGetBuffer(transfer_buffer_id);
  return true;
}

bool InProcessCommandBuffer::Initialize(
    scoped_refptr<gfx::GLSurface> surface,
    bool is_offscreen,
    bool share_resources,
    gfx::AcceleratedWidget window,
    const gfx::Size& size,
    const char* allowed_extensions,
    const std::vector<int32>& attribs,
    gfx::GpuPreference gpu_preference,
    const base::Closure& context_lost_callback,
    unsigned int share_group_id) {

  share_resources_ = share_resources;
  context_lost_callback_ = WrapCallback(context_lost_callback);
  share_group_id_ = share_group_id;

  if (surface) {
    // GPU thread must be the same as client thread due to GLSurface not being
    // thread safe.
    sequence_checker_.reset(new base::SequenceChecker);
    surface_ = surface;
  }

  base::Callback<bool(void)> init_task =
      base::Bind(&InProcessCommandBuffer::InitializeOnGpuThread,
                 base::Unretained(this),
                 is_offscreen,
                 window,
                 size,
                 allowed_extensions,
                 attribs,
                 gpu_preference);

  base::WaitableEvent completion(true, false);
  bool result = false;
  QueueTask(
      base::Bind(&RunTaskWithResult<bool>, init_task, &result, &completion));
  completion.Wait();
  return result;
}

bool InProcessCommandBuffer::InitializeOnGpuThread(
    bool is_offscreen,
    gfx::AcceleratedWidget window,
    const gfx::Size& size,
    const char* allowed_extensions,
    const std::vector<int32>& attribs,
    gfx::GpuPreference gpu_preference) {
  CheckSequencedThread();
  // Use one share group for all contexts.
  CR_DEFINE_STATIC_LOCAL(scoped_refptr<gfx::GLShareGroup>, share_group,
                         (new gfx::GLShareGroup));

  DCHECK(size.width() >= 0 && size.height() >= 0);

  TransferBufferManager* manager = new TransferBufferManager();
  transfer_buffer_manager_.reset(manager);
  manager->Initialize();

  scoped_ptr<CommandBufferService> command_buffer(
      new CommandBufferService(transfer_buffer_manager_.get()));
  command_buffer->SetPutOffsetChangeCallback(base::Bind(
      &InProcessCommandBuffer::PumpCommands, base::Unretained(this)));
  command_buffer->SetParseErrorCallback(base::Bind(
      &InProcessCommandBuffer::OnContextLost, base::Unretained(this)));

  if (!command_buffer->Initialize()) {
    LOG(ERROR) << "Could not initialize command buffer.";
    DestroyOnGpuThread();
    return false;
  }

  InProcessCommandBuffer* context_group = NULL;

  if (share_resources_ && !g_all_shared_contexts.Get().empty()) {
    DCHECK(share_group_id_);
    for (std::set<InProcessCommandBuffer*>::iterator it =
             g_all_shared_contexts.Get().begin();
         it != g_all_shared_contexts.Get().end();
         ++it) {
      if ((*it)->share_group_id_ == share_group_id_) {
        context_group = *it;
        DCHECK(context_group->share_resources_);
        context_lost_ = context_group->IsContextLost();
        break;
      }
    }
    if (!context_group)
      share_group = new gfx::GLShareGroup;
  }

  StreamTextureManager* stream_texture_manager = NULL;
#if defined(OS_ANDROID)
  stream_texture_manager = stream_texture_manager_ =
      context_group ? context_group->stream_texture_manager_.get()
                    : new StreamTextureManagerInProcess;
#endif

  bool bind_generates_resource = false;
  decoder_.reset(gles2::GLES2Decoder::Create(
      context_group ? context_group->decoder_->GetContextGroup()
                    : new gles2::ContextGroup(NULL,
                                              NULL,
                                              NULL,
                                              stream_texture_manager,
                                              bind_generates_resource)));

  gpu_scheduler_.reset(
      new GpuScheduler(command_buffer.get(), decoder_.get(), decoder_.get()));
  command_buffer->SetGetBufferChangeCallback(base::Bind(
      &GpuScheduler::SetGetBuffer, base::Unretained(gpu_scheduler_.get())));
  command_buffer_ = command_buffer.Pass();

  gpu_control_.reset(
      new GpuControlService(decoder_->GetContextGroup()->image_manager(),
                            g_gpu_memory_buffer_factory));

  decoder_->set_engine(gpu_scheduler_.get());

  if (!surface_) {
    if (is_offscreen)
      surface_ = gfx::GLSurface::CreateOffscreenGLSurface(size);
    else
      surface_ = gfx::GLSurface::CreateViewGLSurface(window);
  }

  if (!surface_.get()) {
    LOG(ERROR) << "Could not create GLSurface.";
    DestroyOnGpuThread();
    return false;
  }

  if (g_use_virtualized_gl_context) {
    context_ = share_group->GetSharedContext();
    if (!context_.get()) {
      context_ = gfx::GLContext::CreateGLContext(
          share_group.get(), surface_.get(), gpu_preference);
      share_group->SetSharedContext(context_.get());
    }

    context_ = new GLContextVirtual(
        share_group.get(), context_.get(), decoder_->AsWeakPtr());
    if (context_->Initialize(surface_.get(), gpu_preference)) {
      VLOG(1) << "Created virtual GL context.";
    } else {
      context_ = NULL;
    }
  } else {
    context_ = gfx::GLContext::CreateGLContext(
        share_group.get(), surface_.get(), gpu_preference);
  }

  if (!context_.get()) {
    LOG(ERROR) << "Could not create GLContext.";
    DestroyOnGpuThread();
    return false;
  }

  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "Could not make context current.";
    DestroyOnGpuThread();
    return false;
  }

  gles2::DisallowedFeatures disallowed_features;
  disallowed_features.swap_buffer_complete_callback = true;
  disallowed_features.gpu_memory_manager = true;
  if (!decoder_->Initialize(surface_,
                            context_,
                            is_offscreen,
                            size,
                            disallowed_features,
                            allowed_extensions,
                            attribs)) {
    LOG(ERROR) << "Could not initialize decoder.";
    DestroyOnGpuThread();
    return false;
  }

  if (!is_offscreen) {
    decoder_->SetResizeCallback(base::Bind(
        &InProcessCommandBuffer::OnResizeView, base::Unretained(this)));
  }

  if (share_resources_) {
    g_all_shared_contexts.Pointer()->insert(this);
  }

  return true;
}

void InProcessCommandBuffer::Destroy() {
  CheckSequencedThread();
  base::WaitableEvent completion(true, false);
  bool result = false;
  base::Callback<bool(void)> destroy_task = base::Bind(
      &InProcessCommandBuffer::DestroyOnGpuThread, base::Unretained(this));
  QueueTask(
      base::Bind(&RunTaskWithResult<bool>, destroy_task, &result, &completion));
  completion.Wait();
}

bool InProcessCommandBuffer::DestroyOnGpuThread() {
  CheckSequencedThread();
  command_buffer_.reset();
  // Clean up GL resources if possible.
  bool have_context = context_ && context_->MakeCurrent(surface_);
  if (decoder_) {
    decoder_->Destroy(have_context);
    decoder_.reset();
  }
  context_ = NULL;
  surface_ = NULL;

  g_all_shared_contexts.Pointer()->erase(this);
  return true;
}

void InProcessCommandBuffer::CheckSequencedThread() {
  DCHECK(!sequence_checker_ ||
         sequence_checker_->CalledOnValidSequencedThread());
}

void InProcessCommandBuffer::OnContextLost() {
  CheckSequencedThread();
  if (!context_lost_callback_.is_null()) {
    context_lost_callback_.Run();
    context_lost_callback_.Reset();
  }

  context_lost_ = true;
  if (share_resources_) {
    for (std::set<InProcessCommandBuffer*>::iterator it =
             g_all_shared_contexts.Get().begin();
         it != g_all_shared_contexts.Get().end();
         ++it) {
      (*it)->context_lost_ = true;
    }
  }
}

CommandBuffer::State InProcessCommandBuffer::GetStateFast() {
  CheckSequencedThread();
  base::AutoLock lock(state_after_last_flush_lock_);
  if (state_after_last_flush_.generation - last_state_.generation < 0x80000000U)
    last_state_ = state_after_last_flush_;
  return last_state_;
}

CommandBuffer::State InProcessCommandBuffer::GetState() {
  CheckSequencedThread();
  return GetStateFast();
}

CommandBuffer::State InProcessCommandBuffer::GetLastState() {
  CheckSequencedThread();
  return last_state_;
}

int32 InProcessCommandBuffer::GetLastToken() {
  CheckSequencedThread();
  GetStateFast();
  return last_state_.token;
}

void InProcessCommandBuffer::FlushOnGpuThread(int32 put_offset) {
  CheckSequencedThread();
  ScopedEvent handle_flush(&flush_event_);
  base::AutoLock lock(command_buffer_lock_);
  command_buffer_->Flush(put_offset);
  {
    // Update state before signaling the flush event.
    base::AutoLock lock(state_after_last_flush_lock_);
    state_after_last_flush_ = command_buffer_->GetState();
  }
  DCHECK((!error::IsError(state_after_last_flush_.error) && !context_lost_) ||
         (error::IsError(state_after_last_flush_.error) && context_lost_));
}

void InProcessCommandBuffer::Flush(int32 put_offset) {
  CheckSequencedThread();
  if (last_state_.error != gpu::error::kNoError)
    return;

  if (last_put_offset_ == put_offset)
    return;

  last_put_offset_ = put_offset;
  base::Closure task = base::Bind(&InProcessCommandBuffer::FlushOnGpuThread,
                                  base::Unretained(this),
                                  put_offset);
  QueueTask(task);
}

CommandBuffer::State InProcessCommandBuffer::FlushSync(int32 put_offset,
                                                       int32 last_known_get) {
  CheckSequencedThread();
  if (put_offset == last_known_get || last_state_.error != gpu::error::kNoError)
    return last_state_;

  Flush(put_offset);
  GetStateFast();
  while (last_known_get == last_state_.get_offset &&
         last_state_.error == gpu::error::kNoError) {
    flush_event_.Wait();
    GetStateFast();
  }

  return last_state_;
}

void InProcessCommandBuffer::SetGetBuffer(int32 shm_id) {
  CheckSequencedThread();
  if (last_state_.error != gpu::error::kNoError)
    return;

  {
    base::AutoLock lock(command_buffer_lock_);
    command_buffer_->SetGetBuffer(shm_id);
    last_put_offset_ = 0;
  }
  {
    base::AutoLock lock(state_after_last_flush_lock_);
    state_after_last_flush_ = command_buffer_->GetState();
  }
}

gpu::Buffer InProcessCommandBuffer::CreateTransferBuffer(size_t size,
                                                         int32* id) {
  CheckSequencedThread();
  base::AutoLock lock(command_buffer_lock_);
  return command_buffer_->CreateTransferBuffer(size, id);
}

void InProcessCommandBuffer::DestroyTransferBuffer(int32 id) {
  CheckSequencedThread();
  base::Closure task = base::Bind(&CommandBuffer::DestroyTransferBuffer,
                                  base::Unretained(command_buffer_.get()),
                                  id);

  QueueTask(task);
}

gpu::Buffer InProcessCommandBuffer::GetTransferBuffer(int32 id) {
  NOTREACHED();
  return gpu::Buffer();
}

uint32 InProcessCommandBuffer::InsertSyncPoint() {
  NOTREACHED();
  return 0;
}
void InProcessCommandBuffer::SignalSyncPoint(unsigned sync_point,
                                             const base::Closure& callback) {
  CheckSequencedThread();
  QueueTask(WrapCallback(callback));
}

gfx::GpuMemoryBuffer* InProcessCommandBuffer::CreateGpuMemoryBuffer(
    size_t width,
    size_t height,
    unsigned internalformat,
    int32* id) {
  CheckSequencedThread();
  base::AutoLock lock(command_buffer_lock_);
  return gpu_control_->CreateGpuMemoryBuffer(width,
                                             height,
                                             internalformat,
                                             id);
}

void InProcessCommandBuffer::DestroyGpuMemoryBuffer(int32 id) {
  CheckSequencedThread();
  base::Closure task = base::Bind(&GpuControl::DestroyGpuMemoryBuffer,
                                  base::Unretained(gpu_control_.get()),
                                  id);

  QueueTask(task);
}

gpu::error::Error InProcessCommandBuffer::GetLastError() {
  CheckSequencedThread();
  return last_state_.error;
}

bool InProcessCommandBuffer::Initialize() {
  NOTREACHED();
  return false;
}

void InProcessCommandBuffer::SetGetOffset(int32 get_offset) { NOTREACHED(); }

void InProcessCommandBuffer::SetToken(int32 token) { NOTREACHED(); }

void InProcessCommandBuffer::SetParseError(gpu::error::Error error) {
  NOTREACHED();
}

void InProcessCommandBuffer::SetContextLostReason(
    gpu::error::ContextLostReason reason) {
  NOTREACHED();
}

namespace {

void PostCallback(const scoped_refptr<base::MessageLoopProxy>& loop,
                         const base::Closure& callback) {
  if (!loop->BelongsToCurrentThread()) {
    loop->PostTask(FROM_HERE, callback);
  } else {
    callback.Run();
  }
}

void RunOnTargetThread(scoped_ptr<base::Closure> callback) {
  DCHECK(callback.get());
  callback->Run();
}

}  // anonymous namespace

base::Closure InProcessCommandBuffer::WrapCallback(
    const base::Closure& callback) {
  // Make sure the callback gets deleted on the target thread by passing
  // ownership.
  scoped_ptr<base::Closure> scoped_callback(new base::Closure(callback));
  base::Closure callback_on_client_thread =
      base::Bind(&RunOnTargetThread, base::Passed(&scoped_callback));
  base::Closure wrapped_callback =
      base::Bind(&PostCallback, base::MessageLoopProxy::current(),
                 callback_on_client_thread);
  return wrapped_callback;
}

#if defined(OS_ANDROID)
scoped_refptr<gfx::SurfaceTextureBridge>
InProcessCommandBuffer::GetSurfaceTexture(uint32 stream_id) {
  DCHECK(stream_texture_manager_);
  return stream_texture_manager_->GetSurfaceTexture(stream_id);
}
#endif

// static
void InProcessCommandBuffer::EnableVirtualizedContext() {
  g_use_virtualized_gl_context = true;
}

// static
void InProcessCommandBuffer::SetScheduleCallback(
    const base::Closure& callback) {
  DCHECK(!g_uses_explicit_scheduling);
  DCHECK(!SchedulerClientBase::HasClients());
  g_uses_explicit_scheduling = true;
  g_gpu_queue.Get().SetScheduleCallback(callback);
}

// static
void InProcessCommandBuffer::ProcessGpuWorkOnCurrentThread() {
  g_gpu_queue.Get().RunTasks();
}

// static
void InProcessCommandBuffer::SetGpuMemoryBufferFactory(
    GpuMemoryBufferFactory* factory) {
  g_gpu_memory_buffer_factory = factory;
}

}  // namespace gpu
