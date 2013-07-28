// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gl_in_process_context.h"

#include <utility>
#include <vector>

#include <GLES2/gl2.h>
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gpu_memory_buffer.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_factory.h"
#include "gpu/command_buffer/client/image_factory.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/id_allocator.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"

namespace gpu {

using gles2::ImageManager;

namespace {

const int32 kCommandBufferSize = 1024 * 1024;
// TODO(kbr): make the transfer buffer size configurable via context
// creation attributes.
const size_t kStartTransferBufferSize = 4 * 1024 * 1024;
const size_t kMinTransferBufferSize = 1 * 256 * 1024;
const size_t kMaxTransferBufferSize = 16 * 1024 * 1024;

// In the normal command buffer implementation, all commands are passed over IPC
// to the gpu process where they are fed to the GLES2Decoder from a single
// thread. In layout tests, any thread could call this function. GLES2Decoder,
// and in particular the GL implementations behind it, are not generally
// threadsafe, so we guard entry points with a mutex.
static base::LazyInstance<base::Lock> g_decoder_lock =
    LAZY_INSTANCE_INITIALIZER;

class GLInProcessContextImpl;

static base::LazyInstance<
    std::set<GLInProcessContextImpl*> >
        g_all_shared_contexts = LAZY_INSTANCE_INITIALIZER;

static bool g_use_virtualized_gl_context = false;

static GpuMemoryBufferFactory* g_gpu_memory_buffer_factory = NULL;

// Also calls DetachFromThreadHack on all GLES2Decoders before the lock is
// released to maintain the invariant that all decoders are unbound while the
// lock is not held. This is to workaround DumpRenderTree using WGC3DIPCBI with
// shared resources on different threads.
// Remove this as part of crbug.com/234964.
class AutoLockAndDecoderDetachThread {
 public:
  AutoLockAndDecoderDetachThread(
      base::Lock& lock,
      const std::set<GLInProcessContextImpl*>& contexts);
  ~AutoLockAndDecoderDetachThread();

 private:
  base::AutoLock auto_lock_;
  const std::set<GLInProcessContextImpl*>& contexts_;
};

size_t SharedContextCount() {
  AutoLockAndDecoderDetachThread lock(g_decoder_lock.Get(),
                                      g_all_shared_contexts.Get());
  return g_all_shared_contexts.Get().size();
}

class GLInProcessContextImpl
    : public GLInProcessContext,
      public gles2::ImageFactory,
      public base::SupportsWeakPtr<GLInProcessContextImpl> {
 public:
  explicit GLInProcessContextImpl(bool share_resources);
  virtual ~GLInProcessContextImpl();

  bool Initialize(bool is_offscreen,
                  gfx::AcceleratedWidget window,
                  const gfx::Size& size,
                  const char* allowed_extensions,
                  const int32* attrib_list,
                  gfx::GpuPreference gpu_preference,
                  const base::Closure& context_lost_callback);

  // GLInProcessContext implementation:
  virtual void SignalSyncPoint(unsigned sync_point,
                               const base::Closure& callback) OVERRIDE;
  virtual void SignalQuery(unsigned query, const base::Closure& callback)
      OVERRIDE;
  virtual gles2::GLES2Implementation* GetImplementation() OVERRIDE;

  // ImageFactory implementation:
  virtual scoped_ptr<GpuMemoryBuffer> CreateGpuMemoryBuffer(
      int width, int height, GLenum internalformat,
      unsigned* image_id) OVERRIDE;
  virtual void DeleteGpuMemoryBuffer(unsigned image_id) OVERRIDE;

  // Other methods:
  gles2::GLES2Decoder* GetDecoder();
  bool GetBufferChanged(int32 transfer_buffer_id);
  void PumpCommands();
  void OnResizeView(gfx::Size size, float scale_factor);
  void OnContextLost();

 private:
  void Destroy();
  bool IsCommandBufferContextLost();
  void PollQueryCallbacks();
  void CallQueryCallback(size_t index);
  bool MakeCurrent();

  gles2::ImageManager* GetImageManager();

  base::Closure context_lost_callback_;
  scoped_ptr<TransferBufferManagerInterface> transfer_buffer_manager_;
  scoped_ptr<CommandBuffer> command_buffer_;
  scoped_ptr<GpuScheduler> gpu_scheduler_;
  scoped_ptr<gles2::GLES2Decoder> decoder_;
  scoped_refptr<gfx::GLContext> context_;
  scoped_refptr<gfx::GLSurface> surface_;
  scoped_ptr<gles2::GLES2CmdHelper> gles2_helper_;
  scoped_ptr<TransferBuffer> transfer_buffer_;
  scoped_ptr<gles2::GLES2Implementation> gles2_implementation_;
  bool share_resources_;
  bool context_lost_;

  typedef std::pair<unsigned, base::Closure> QueryCallback;
  std::vector<QueryCallback> query_callbacks_;

  std::vector<base::Closure> signal_sync_point_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(GLInProcessContextImpl);
};

AutoLockAndDecoderDetachThread::AutoLockAndDecoderDetachThread(
    base::Lock& lock,
    const std::set<GLInProcessContextImpl*>& contexts)
    : auto_lock_(lock),
      contexts_(contexts) {
}

void DetachThread(GLInProcessContextImpl* context) {
  if (context->GetDecoder())
    context->GetDecoder()->DetachFromThreadHack();
}

AutoLockAndDecoderDetachThread::~AutoLockAndDecoderDetachThread() {
  std::for_each(contexts_.begin(),
                contexts_.end(),
                &DetachThread);
}

scoped_ptr<GpuMemoryBuffer> GLInProcessContextImpl::CreateGpuMemoryBuffer(
    int width, int height, GLenum internalformat, unsigned int* image_id) {
  // We're taking the lock here because we're accessing the ContextGroup's
  // shared IdManager.
  AutoLockAndDecoderDetachThread lock(g_decoder_lock.Get(),
                                      g_all_shared_contexts.Get());
  scoped_ptr<GpuMemoryBuffer> buffer(
      g_gpu_memory_buffer_factory->CreateGpuMemoryBuffer(width,
                                                         height,
                                                         internalformat));
  if (!buffer)
    return scoped_ptr<GpuMemoryBuffer>();

  scoped_refptr<gfx::GLImage> gl_image =
      gfx::GLImage::CreateGLImageForGpuMemoryBuffer(buffer->GetNativeBuffer(),
                                                    gfx::Size(width, height));
  *image_id = decoder_->GetContextGroup()
      ->GetIdAllocator(gles2::id_namespaces::kImages)->AllocateID();
  GetImageManager()->AddImage(gl_image.get(), *image_id);
  return buffer.Pass();
}

void GLInProcessContextImpl::DeleteGpuMemoryBuffer(unsigned int image_id) {
  // We're taking the lock here because we're accessing the ContextGroup's
  // shared ImageManager.
  AutoLockAndDecoderDetachThread lock(g_decoder_lock.Get(),
                                      g_all_shared_contexts.Get());
  GetImageManager()->RemoveImage(image_id);
  decoder_->GetContextGroup()->GetIdAllocator(gles2::id_namespaces::kImages)
      ->FreeID(image_id);
}

GLInProcessContextImpl::GLInProcessContextImpl(bool share_resources)
    : share_resources_(share_resources),
      context_lost_(false) {
}

GLInProcessContextImpl::~GLInProcessContextImpl() {
  Destroy();
}

bool GLInProcessContextImpl::MakeCurrent() {
  if (decoder_->MakeCurrent())
    return true;
  DLOG(ERROR) << "Context lost because MakeCurrent failed.";
  command_buffer_->SetContextLostReason(decoder_->GetContextLostReason());
  command_buffer_->SetParseError(gpu::error::kLostContext);
  return false;
}

void GLInProcessContextImpl::PumpCommands() {
  {
    AutoLockAndDecoderDetachThread lock(g_decoder_lock.Get(),
                                        g_all_shared_contexts.Get());
    if (!MakeCurrent())
      return;
    gpu_scheduler_->PutChanged();
    CommandBuffer::State state = command_buffer_->GetState();
    DCHECK((!error::IsError(state.error) && !context_lost_) ||
           (error::IsError(state.error) && context_lost_));
  }

  if (!context_lost_ && signal_sync_point_callbacks_.size()) {
    for (size_t n = 0; n < signal_sync_point_callbacks_.size(); n++) {
      base::MessageLoop::current()->PostTask(FROM_HERE,
                                             signal_sync_point_callbacks_[n]);
    }
  }
  signal_sync_point_callbacks_.clear();
}

bool GLInProcessContextImpl::GetBufferChanged(int32 transfer_buffer_id) {
  return gpu_scheduler_->SetGetBuffer(transfer_buffer_id);
}

void GLInProcessContextImpl::SignalSyncPoint(unsigned sync_point,
                                             const base::Closure& callback) {
  DCHECK(!callback.is_null());
  signal_sync_point_callbacks_.push_back(callback);
}

bool GLInProcessContextImpl::IsCommandBufferContextLost() {
  if (context_lost_ || !command_buffer_) {
    return true;
  }
  CommandBuffer::State state = command_buffer_->GetState();
  return error::IsError(state.error);
}

gles2::GLES2Decoder* GLInProcessContextImpl::GetDecoder() {
  return decoder_.get();
}

void GLInProcessContextImpl::OnResizeView(gfx::Size size, float scale_factor) {
  DCHECK(!surface_->IsOffscreen());
  surface_->Resize(size);
}

gles2::GLES2Implementation* GLInProcessContextImpl::GetImplementation() {
  return gles2_implementation_.get();
}

gles2::ImageManager* GLInProcessContextImpl::GetImageManager() {
  return decoder_->GetContextGroup()->image_manager();
}

bool GLInProcessContextImpl::Initialize(
    bool is_offscreen,
    gfx::AcceleratedWidget window,
    const gfx::Size& size,
    const char* allowed_extensions,
    const int32* attrib_list,
    gfx::GpuPreference gpu_preference,
    const base::Closure& context_lost_callback) {
  // Use one share group for all contexts.
  CR_DEFINE_STATIC_LOCAL(scoped_refptr<gfx::GLShareGroup>, share_group,
                         (new gfx::GLShareGroup));

  DCHECK(size.width() >= 0 && size.height() >= 0);

  std::vector<int32> attribs;
  while (attrib_list) {
    int32 attrib = *attrib_list++;
    switch (attrib) {
      // Known attributes
      case ALPHA_SIZE:
      case BLUE_SIZE:
      case GREEN_SIZE:
      case RED_SIZE:
      case DEPTH_SIZE:
      case STENCIL_SIZE:
      case SAMPLES:
      case SAMPLE_BUFFERS:
        attribs.push_back(attrib);
        attribs.push_back(*attrib_list++);
        break;
      case NONE:
        attribs.push_back(attrib);
        attrib_list = NULL;
        break;
      default:
        attribs.push_back(NONE);
        attrib_list = NULL;
        break;
    }
  }

  {
    TransferBufferManager* manager = new TransferBufferManager();
    transfer_buffer_manager_.reset(manager);
    manager->Initialize();
  }

  scoped_ptr<CommandBufferService> command_buffer(
      new CommandBufferService(transfer_buffer_manager_.get()));
  command_buffer->SetPutOffsetChangeCallback(base::Bind(
      &GLInProcessContextImpl::PumpCommands, base::Unretained(this)));
  command_buffer->SetGetBufferChangeCallback(base::Bind(
      &GLInProcessContextImpl::GetBufferChanged, base::Unretained(this)));
  command_buffer->SetParseErrorCallback(base::Bind(
      &GLInProcessContextImpl::OnContextLost, base::Unretained(this)));

  command_buffer_ = command_buffer.Pass();
  if (!command_buffer_->Initialize()) {
    LOG(ERROR) << "Could not initialize command buffer.";
    Destroy();
    return false;
  }

  GLInProcessContextImpl* context_group = NULL;

  {
    AutoLockAndDecoderDetachThread lock(g_decoder_lock.Get(),
                                        g_all_shared_contexts.Get());
    if (share_resources_ && !g_all_shared_contexts.Get().empty()) {
      for (std::set<GLInProcessContextImpl*>::iterator it =
               g_all_shared_contexts.Get().begin();
           it != g_all_shared_contexts.Get().end();
           ++it) {
        if (!(*it)->IsCommandBufferContextLost()) {
          context_group = *it;
          break;
        }
      }
      if (!context_group)
        share_group = new gfx::GLShareGroup;
    }

    // TODO(gman): This needs to be true if this is Pepper.
    bool bind_generates_resource = false;
    decoder_.reset(gles2::GLES2Decoder::Create(
        context_group ? context_group->decoder_->GetContextGroup()
                      : new gles2::ContextGroup(
                            NULL, NULL, NULL, NULL, bind_generates_resource)));

    gpu_scheduler_.reset(new GpuScheduler(command_buffer_.get(),
                                          decoder_.get(),
                                          decoder_.get()));

    decoder_->set_engine(gpu_scheduler_.get());

    if (is_offscreen)
      surface_ = gfx::GLSurface::CreateOffscreenGLSurface(size);
    else
      surface_ = gfx::GLSurface::CreateViewGLSurface(window);

    if (!surface_.get()) {
      LOG(ERROR) << "Could not create GLSurface.";
      Destroy();
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
      context_ = gfx::GLContext::CreateGLContext(share_group.get(),
                                                 surface_.get(),
                                                 gpu_preference);
    }

    if (!context_.get()) {
      LOG(ERROR) << "Could not create GLContext.";
      Destroy();
      return false;
    }

    if (!context_->MakeCurrent(surface_.get())) {
      LOG(ERROR) << "Could not make context current.";
      Destroy();
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
      Destroy();
      return false;
    }

    if (!is_offscreen) {
      decoder_->SetResizeCallback(base::Bind(
          &GLInProcessContextImpl::OnResizeView, base::Unretained(this)));
    }
  }

  // Create the GLES2 helper, which writes the command buffer protocol.
  gles2_helper_.reset(new gles2::GLES2CmdHelper(command_buffer_.get()));
  if (!gles2_helper_->Initialize(kCommandBufferSize)) {
    Destroy();
    return false;
  }

  // Create a transfer buffer.
  transfer_buffer_.reset(new TransferBuffer(gles2_helper_.get()));

  // Create the object exposing the OpenGL API.
  gles2_implementation_.reset(new gles2::GLES2Implementation(
      gles2_helper_.get(),
      context_group ? context_group->GetImplementation()->share_group() : NULL,
      transfer_buffer_.get(),
      true,
      false,
      this));

  if (!gles2_implementation_->Initialize(
      kStartTransferBufferSize,
      kMinTransferBufferSize,
      kMaxTransferBufferSize)) {
    return false;
  }

  if (share_resources_) {
    AutoLockAndDecoderDetachThread lock(g_decoder_lock.Get(),
                                        g_all_shared_contexts.Get());
    g_all_shared_contexts.Pointer()->insert(this);
  }

  context_lost_callback_ = context_lost_callback;
  return true;
}

void GLInProcessContextImpl::Destroy() {
  while (!query_callbacks_.empty()) {
    CallQueryCallback(0);
  }

  bool context_lost = IsCommandBufferContextLost();

  if (gles2_implementation_) {
    // First flush the context to ensure that any pending frees of resources
    // are completed. Otherwise, if this context is part of a share group,
    // those resources might leak. Also, any remaining side effects of commands
    // issued on this context might not be visible to other contexts in the
    // share group.
    gles2_implementation_->Flush();

    gles2_implementation_.reset();
  }

  transfer_buffer_.reset();
  gles2_helper_.reset();
  command_buffer_.reset();

  AutoLockAndDecoderDetachThread lock(g_decoder_lock.Get(),
                                      g_all_shared_contexts.Get());
  if (decoder_) {
    decoder_->Destroy(!context_lost);
  }

  g_all_shared_contexts.Pointer()->erase(this);
}

void GLInProcessContextImpl::OnContextLost() {
  if (!context_lost_callback_.is_null())
    context_lost_callback_.Run();

  context_lost_ = true;
  if (share_resources_) {
      for (std::set<GLInProcessContextImpl*>::iterator it =
               g_all_shared_contexts.Get().begin();
           it != g_all_shared_contexts.Get().end();
           ++it)
        (*it)->context_lost_ = true;
  }
}

void GLInProcessContextImpl::CallQueryCallback(size_t index) {
  DCHECK_LT(index, query_callbacks_.size());
  QueryCallback query_callback = query_callbacks_[index];
  query_callbacks_[index] = query_callbacks_.back();
  query_callbacks_.pop_back();
  query_callback.second.Run();
}

void GLInProcessContextImpl::PollQueryCallbacks() {
  for (size_t i = 0; i < query_callbacks_.size();) {
    unsigned query = query_callbacks_[i].first;
    GLuint param = 0;
    gles2::GLES2Implementation* gl = GetImplementation();
    if (gl->IsQueryEXT(query)) {
      gl->GetQueryObjectuivEXT(query, GL_QUERY_RESULT_AVAILABLE_EXT, &param);
    } else {
      param = 1;
    }
    if (param) {
      CallQueryCallback(i);
    } else {
      i++;
    }
  }
  if (!query_callbacks_.empty()) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&GLInProcessContextImpl::PollQueryCallbacks,
                   this->AsWeakPtr()),
        base::TimeDelta::FromMilliseconds(5));
  }
}

void GLInProcessContextImpl::SignalQuery(
    unsigned query,
    const base::Closure& callback) {
  query_callbacks_.push_back(std::make_pair(query, callback));
  // If size > 1, there is already a poll callback pending.
  if (query_callbacks_.size() == 1) {
    PollQueryCallbacks();
  }
}

}  // anonymous namespace

// static
GLInProcessContext* GLInProcessContext::CreateContext(
    bool is_offscreen,
    gfx::AcceleratedWidget window,
    const gfx::Size& size,
    bool share_resources,
    const char* allowed_extensions,
    const int32* attrib_list,
    gfx::GpuPreference gpu_preference,
    const base::Closure& callback) {
  scoped_ptr<GLInProcessContextImpl> context(
      new GLInProcessContextImpl(share_resources));
  if (!context->Initialize(
      is_offscreen,
      window,
      size,
      allowed_extensions,
      attrib_list,
      gpu_preference,
      callback))
    return NULL;

  return context.release();
}

// static
void GLInProcessContext::SetGpuMemoryBufferFactory(
    GpuMemoryBufferFactory* factory) {
  DCHECK_EQ(0u, SharedContextCount());
  g_gpu_memory_buffer_factory = factory;
}

// static
void GLInProcessContext::EnableVirtualizedContext() {
  DCHECK_EQ(0u, SharedContextCount());
  g_use_virtualized_gl_context = true;
}

}  // namespace gpu
