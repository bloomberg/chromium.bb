 // Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_GPU)

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/shared_memory.h"
#include "base/time.h"
#include "build/build_config.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/gpu_watchdog.h"
#include "content/common/gpu/image_transport_surface.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_WIN)
#include "content/public/common/sandbox_init.h"
#endif

GpuCommandBufferStub::SurfaceState::SurfaceState(int32 surface_id,
                                                 bool visible,
                                                 base::TimeTicks last_used_time)
    : surface_id(surface_id),
      visible(visible),
      last_used_time(last_used_time) {
}

GpuCommandBufferStub::GpuCommandBufferStub(
    GpuChannel* channel,
    GpuCommandBufferStub* share_group,
    const gfx::GLSurfaceHandle& handle,
    gpu::gles2::MailboxManager* mailbox_manager,
    const gfx::Size& size,
    const gpu::gles2::DisallowedFeatures& disallowed_features,
    const std::string& allowed_extensions,
    const std::vector<int32>& attribs,
    gfx::GpuPreference gpu_preference,
    int32 route_id,
    int32 surface_id,
    GpuWatchdog* watchdog,
    bool software)
    : channel_(channel),
      handle_(handle),
      initial_size_(size),
      disallowed_features_(disallowed_features),
      allowed_extensions_(allowed_extensions),
      requested_attribs_(attribs),
      gpu_preference_(gpu_preference),
      route_id_(route_id),
      software_(software),
      client_has_memory_allocation_changed_callback_(false),
      last_flush_count_(0),
      parent_stub_for_initialization_(),
      parent_texture_for_initialization_(0),
      watchdog_(watchdog) {
  if (share_group) {
    context_group_ = share_group->context_group_;
  } else {
    context_group_ = new gpu::gles2::ContextGroup(mailbox_manager, true);
  }
  if (surface_id != 0)
    surface_state_.reset(new GpuCommandBufferStubBase::SurfaceState(
        surface_id, true, base::TimeTicks::Now()));
}

GpuCommandBufferStub::~GpuCommandBufferStub() {
  Destroy();

  GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
  gpu_channel_manager->Send(new GpuHostMsg_DestroyCommandBuffer(surface_id()));
}

bool GpuCommandBufferStub::OnMessageReceived(const IPC::Message& message) {
  // Ensure the appropriate GL context is current before handling any IPC
  // messages directed at the command buffer. This ensures that the message
  // handler can assume that the context is current.
  if (decoder_.get()) {
    if (!decoder_->MakeCurrent()) {
      DLOG(ERROR) << "Context lost because MakeCurrent failed.";
      command_buffer_->SetContextLostReason(decoder_->GetContextLostReason());
      command_buffer_->SetParseError(gpu::error::kLostContext);
      if (gfx::GLContext::LosesAllContextsOnContextLost())
        channel_->LoseAllContexts();
      return false;
    }
  }

  // Always use IPC_MESSAGE_HANDLER_DELAY_REPLY for synchronous message handlers
  // here. This is so the reply can be delayed if the scheduler is unscheduled.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuCommandBufferStub, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_Initialize,
                                    OnInitialize);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_SetGetBuffer,
                                    OnSetGetBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_SetSharedStateBuffer,
                                    OnSetSharedStateBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_SetParent,
                                    OnSetParent);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_Echo, OnEcho);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_GetState, OnGetState);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_GetStateFast,
                                    OnGetStateFast);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_AsyncFlush, OnAsyncFlush);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_Rescheduled, OnRescheduled);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_CreateTransferBuffer,
                                    OnCreateTransferBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_RegisterTransferBuffer,
                                    OnRegisterTransferBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_DestroyTransferBuffer,
                                    OnDestroyTransferBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_GetTransferBuffer,
                                    OnGetTransferBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_CreateVideoDecoder,
                                    OnCreateVideoDecoder)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_DestroyVideoDecoder,
                        OnDestroyVideoDecoder)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SetSurfaceVisible,
                        OnSetSurfaceVisible)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_DiscardBackbuffer,
                        OnDiscardBackbuffer)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_EnsureBackbuffer,
                        OnEnsureBackbuffer)
    IPC_MESSAGE_HANDLER(
        GpuCommandBufferMsg_SetClientHasMemoryAllocationChangedCallback,
        OnSetClientHasMemoryAllocationChangedCallback)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  DCHECK(handled);
  return handled;
}

bool GpuCommandBufferStub::Send(IPC::Message* message) {
  return channel_->Send(message);
}

bool GpuCommandBufferStub::IsScheduled() {
  return !scheduler_.get() || scheduler_->IsScheduled();
}

bool GpuCommandBufferStub::HasMoreWork() {
  return scheduler_.get() && scheduler_->HasMoreWork();
}

void GpuCommandBufferStub::PollWork() {
  if (decoder_.get()) {
    if (!decoder_->MakeCurrent()) {
      DLOG(ERROR) << "Context lost because MakeCurrent failed.";
      command_buffer_->SetContextLostReason(decoder_->GetContextLostReason());
      command_buffer_->SetParseError(gpu::error::kLostContext);
      if (gfx::GLContext::LosesAllContextsOnContextLost())
        channel_->LoseAllContexts();
      return;
    }
  }
  if (scheduler_.get())
    scheduler_->PollUnscheduleFences();
}

bool GpuCommandBufferStub::HasUnprocessedCommands() {
  if (command_buffer_.get()) {
    gpu::CommandBuffer::State state = command_buffer_->GetLastState();
    return state.put_offset != state.get_offset &&
        !gpu::error::IsError(state.error);
  }
  return false;
}

void GpuCommandBufferStub::OnEcho(const IPC::Message& message) {
  // For latency_tests.cc:
  UNSHIPPED_TRACE_EVENT_INSTANT0("test_gpu", "CompositorSwapBuffersComplete");
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnEcho");
  Send(new IPC::Message(message));
}

void GpuCommandBufferStub::DelayEcho(IPC::Message* message) {
  delayed_echos_.push_back(message);
}

void GpuCommandBufferStub::OnReschedule() {
  while (!delayed_echos_.empty()) {
    scoped_ptr<IPC::Message> message(delayed_echos_.front());
    delayed_echos_.pop_front();

    OnMessageReceived(*message);
  }

  channel_->OnScheduled();
}

void GpuCommandBufferStub::Destroy() {
  // The scheduler has raw references to the decoder and the command buffer so
  // destroy it before those.
  scheduler_.reset();

  while (!delayed_echos_.empty()) {
    delete delayed_echos_.front();
    delayed_echos_.pop_front();
  }

  if (decoder_.get())
    decoder_->MakeCurrent();
  FOR_EACH_OBSERVER(DestructionObserver,
                    destruction_observers_,
                    OnWillDestroyStub(this));

  if (decoder_.get()) {
    decoder_->Destroy(true);
    decoder_.reset();
  }

  command_buffer_.reset();

  context_ = NULL;
  surface_ = NULL;

  channel_->gpu_channel_manager()->gpu_memory_manager()->ScheduleManage();
}

void GpuCommandBufferStub::OnInitializeFailed(IPC::Message* reply_message) {
  Destroy();
  GpuCommandBufferMsg_Initialize::WriteReplyParams(reply_message, false);
  Send(reply_message);
}

void GpuCommandBufferStub::OnInitialize(
    IPC::Message* reply_message) {
  DCHECK(!command_buffer_.get());

  command_buffer_.reset(new gpu::CommandBufferService);

  if (!command_buffer_->Initialize()) {
    DLOG(ERROR) << "CommandBufferService failed to initialize.\n";
    OnInitializeFailed(reply_message);
    return;
  }

  decoder_.reset(::gpu::gles2::GLES2Decoder::Create(context_group_.get()));

  scheduler_.reset(new gpu::GpuScheduler(command_buffer_.get(),
                                         decoder_.get(),
                                         decoder_.get()));

  decoder_->set_engine(scheduler_.get());

  if (!handle_.is_null()) {
#if defined(OS_MACOSX) || defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
    if (software_) {
      DLOG(ERROR) << "No software support.\n";
      OnInitializeFailed(reply_message);
      return;
    }
#endif

    surface_ = ImageTransportSurface::CreateSurface(
        channel_->gpu_channel_manager(),
        this,
        handle_);
  } else {
    surface_ = gfx::GLSurface::CreateOffscreenGLSurface(software_,
                                                        gfx::Size(1, 1));
  }

  if (!surface_.get()) {
    // Ensure the decoder is not destroyed if it is not initialized.
    decoder_.reset();

    DLOG(ERROR) << "Failed to create surface.\n";
    OnInitializeFailed(reply_message);
    return;
  }

  gfx::GpuPreference gpu_preference =
      channel_->ShouldPreferDiscreteGpu() ?
          gfx::PreferDiscreteGpu : gpu_preference_;

  context_ = gfx::GLContext::CreateGLContext(
      channel_->share_group(),
      surface_.get(),
      gpu_preference);
  if (!context_.get()) {
    // Ensure the decoder is not destroyed if it is not initialized.
    decoder_.reset();

    DLOG(ERROR) << "Failed to create context.\n";
    OnInitializeFailed(reply_message);
    return;
  }

  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "Failed to make context current.";
    OnInitializeFailed(reply_message);
    return;
  }

  // Initialize the decoder with either the view or pbuffer GLContext.
  if (!decoder_->Initialize(surface_,
                            context_,
                            !surface_id(),
                            initial_size_,
                            disallowed_features_,
                            allowed_extensions_.c_str(),
                            requested_attribs_)) {
    DLOG(ERROR) << "Failed to initialize decoder.";
    OnInitializeFailed(reply_message);
    return;
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableGPUServiceLogging)) {
    decoder_->set_log_commands(true);
  }

  decoder_->SetMsgCallback(
      base::Bind(&GpuCommandBufferStub::SendConsoleMessage,
                 base::Unretained(this)));

  command_buffer_->SetPutOffsetChangeCallback(
      base::Bind(&gpu::GpuScheduler::PutChanged,
                 base::Unretained(scheduler_.get())));
  command_buffer_->SetGetBufferChangeCallback(
      base::Bind(&gpu::GpuScheduler::SetGetBuffer,
                 base::Unretained(scheduler_.get())));
  command_buffer_->SetParseErrorCallback(
      base::Bind(&GpuCommandBufferStub::OnParseError, base::Unretained(this)));
  scheduler_->SetScheduledCallback(
      base::Bind(&GpuCommandBufferStub::OnReschedule, base::Unretained(this)));

  if (watchdog_) {
    scheduler_->SetCommandProcessedCallback(
        base::Bind(&GpuCommandBufferStub::OnCommandProcessed,
                   base::Unretained(this)));
  }

  if (parent_stub_for_initialization_) {
    decoder_->SetParent(parent_stub_for_initialization_->decoder_.get(),
                        parent_texture_for_initialization_);
    parent_stub_for_initialization_.reset();
    parent_texture_for_initialization_ = 0;
  }

  GpuCommandBufferMsg_Initialize::WriteReplyParams(reply_message, true);
  Send(reply_message);

  channel_->gpu_channel_manager()->gpu_memory_manager()->ScheduleManage();
}

void GpuCommandBufferStub::OnSetGetBuffer(
    int32 shm_id, IPC::Message* reply_message) {
  if (command_buffer_.get()) {
    command_buffer_->SetGetBuffer(shm_id);
  } else {
    DLOG(ERROR) << "no command_buffer.";
    reply_message->set_reply_error();
  }
  Send(reply_message);
}

void GpuCommandBufferStub::OnSetSharedStateBuffer(
    int32 shm_id, IPC::Message* reply_message) {
  if (command_buffer_.get()) {
    command_buffer_->SetSharedStateBuffer(shm_id);
  } else {
    DLOG(ERROR) << "no command_buffer.";
    reply_message->set_reply_error();
  }
  Send(reply_message);
}

void GpuCommandBufferStub::OnSetParent(int32 parent_route_id,
                                       uint32 parent_texture_id,
                                       IPC::Message* reply_message) {
  GpuCommandBufferStub* parent_stub = NULL;
  if (parent_route_id != MSG_ROUTING_NONE) {
    parent_stub = channel_->LookupCommandBuffer(parent_route_id);
  }

  bool result = true;
  if (scheduler_.get()) {
    gpu::gles2::GLES2Decoder* parent_decoder =
        parent_stub ? parent_stub->decoder_.get() : NULL;
    result = decoder_->SetParent(parent_decoder, parent_texture_id);
  } else {
    // If we don't have a scheduler, it means that Initialize hasn't been called
    // yet. Keep around the requested parent stub and texture so that we can set
    // it in Initialize().
    parent_stub_for_initialization_ = parent_stub ?
        parent_stub->AsWeakPtr() : base::WeakPtr<GpuCommandBufferStub>();
    parent_texture_for_initialization_ = parent_texture_id;
  }
  GpuCommandBufferMsg_SetParent::WriteReplyParams(reply_message, result);
  Send(reply_message);
}

void GpuCommandBufferStub::OnGetState(IPC::Message* reply_message) {
  if (command_buffer_.get()) {
    gpu::CommandBuffer::State state = command_buffer_->GetState();
    if (state.error == gpu::error::kLostContext &&
        gfx::GLContext::LosesAllContextsOnContextLost())
      channel_->LoseAllContexts();

    GpuCommandBufferMsg_GetState::WriteReplyParams(reply_message, state);
  } else {
    DLOG(ERROR) << "no command_buffer.";
    reply_message->set_reply_error();
  }
  Send(reply_message);
}

void GpuCommandBufferStub::OnParseError() {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnParseError");
  DCHECK(command_buffer_.get());
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  IPC::Message* msg = new GpuCommandBufferMsg_Destroyed(
      route_id_, state.context_lost_reason);
  msg->set_unblock(true);
  Send(msg);
}

void GpuCommandBufferStub::OnGetStateFast(IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnGetStateFast");
  DCHECK(command_buffer_.get());
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  if (state.error == gpu::error::kLostContext &&
      gfx::GLContext::LosesAllContextsOnContextLost())
    channel_->LoseAllContexts();

  GpuCommandBufferMsg_GetStateFast::WriteReplyParams(reply_message, state);
  Send(reply_message);
}

void GpuCommandBufferStub::OnAsyncFlush(int32 put_offset,
                                        uint32 flush_count) {
  TRACE_EVENT1("gpu", "GpuCommandBufferStub::OnAsyncFlush",
               "put_offset", put_offset);
  DCHECK(command_buffer_.get());
  if (flush_count - last_flush_count_ < 0x8000000U) {
    last_flush_count_ = flush_count;
    command_buffer_->Flush(put_offset);
  } else {
    // We received this message out-of-order. This should not happen but is here
    // to catch regressions. Ignore the message.
    NOTREACHED() << "Received a Flush message out-of-order";
  }

  ReportState();
}

void GpuCommandBufferStub::OnRescheduled() {
  gpu::CommandBuffer::State pre_state = command_buffer_->GetLastState();
  command_buffer_->Flush(pre_state.put_offset);
  gpu::CommandBuffer::State post_state = command_buffer_->GetLastState();

  if (pre_state.get_offset != post_state.get_offset)
    ReportState();
}

void GpuCommandBufferStub::OnCreateTransferBuffer(int32 size,
                                                  int32 id_request,
                                                  IPC::Message* reply_message) {
  if (command_buffer_.get()) {
    int32 id = command_buffer_->CreateTransferBuffer(size, id_request);
    GpuCommandBufferMsg_CreateTransferBuffer::WriteReplyParams(
        reply_message, id);
  } else {
    reply_message->set_reply_error();
  }
  Send(reply_message);
}

void GpuCommandBufferStub::OnRegisterTransferBuffer(
    base::SharedMemoryHandle transfer_buffer,
    size_t size,
    int32 id_request,
    IPC::Message* reply_message) {
  base::SharedMemory shared_memory(transfer_buffer, false);

  if (command_buffer_.get()) {
    int32 id = command_buffer_->RegisterTransferBuffer(&shared_memory,
                                                       size,
                                                       id_request);
    GpuCommandBufferMsg_RegisterTransferBuffer::WriteReplyParams(reply_message,
                                                                 id);
  } else {
    reply_message->set_reply_error();
  }

  Send(reply_message);
}

void GpuCommandBufferStub::OnDestroyTransferBuffer(
    int32 id,
    IPC::Message* reply_message) {
  if (command_buffer_.get()) {
    command_buffer_->DestroyTransferBuffer(id);
  } else {
    reply_message->set_reply_error();
  }
  Send(reply_message);
}

void GpuCommandBufferStub::OnGetTransferBuffer(
    int32 id,
    IPC::Message* reply_message) {
  if (command_buffer_.get()) {
    base::SharedMemoryHandle transfer_buffer = base::SharedMemoryHandle();
    uint32 size = 0;

    gpu::Buffer buffer = command_buffer_->GetTransferBuffer(id);
    if (buffer.shared_memory) {
#if defined(OS_WIN)
      transfer_buffer = NULL;
      content::BrokerDuplicateHandle(buffer.shared_memory->handle(),
          channel_->renderer_pid(), &transfer_buffer, FILE_MAP_READ |
          FILE_MAP_WRITE, 0);
      DCHECK(transfer_buffer != NULL);
#else
      buffer.shared_memory->ShareToProcess(channel_->renderer_pid(),
                                           &transfer_buffer);
#endif
      size = buffer.size;
    }

    GpuCommandBufferMsg_GetTransferBuffer::WriteReplyParams(reply_message,
                                                            transfer_buffer,
                                                            size);
  } else {
    reply_message->set_reply_error();
  }
  Send(reply_message);
}

void GpuCommandBufferStub::OnCommandProcessed() {
  if (watchdog_)
    watchdog_->CheckArmed();
}

void GpuCommandBufferStub::ReportState() {
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  if (state.error == gpu::error::kLostContext &&
      gfx::GLContext::LosesAllContextsOnContextLost()) {
    channel_->LoseAllContexts();
  } else {
    command_buffer_->UpdateState();
  }
}

void GpuCommandBufferStub::OnCreateVideoDecoder(
    media::VideoCodecProfile profile,
    IPC::Message* reply_message) {
  int decoder_route_id = channel_->GenerateRouteID();
  GpuCommandBufferMsg_CreateVideoDecoder::WriteReplyParams(
      reply_message, decoder_route_id);
  GpuVideoDecodeAccelerator* decoder =
      new GpuVideoDecodeAccelerator(this, decoder_route_id, this);
  video_decoders_.AddWithID(decoder, decoder_route_id);
  channel_->AddRoute(decoder_route_id, decoder);
  decoder->Initialize(profile, reply_message);
}

void GpuCommandBufferStub::OnDestroyVideoDecoder(int decoder_route_id) {
  channel_->RemoveRoute(decoder_route_id);
  video_decoders_.Remove(decoder_route_id);
}

void GpuCommandBufferStub::OnSetSurfaceVisible(bool visible) {
  DCHECK(surface_state_.get());
  surface_state_->visible = visible;
  surface_state_->last_used_time = base::TimeTicks::Now();
  channel_->gpu_channel_manager()->gpu_memory_manager()->ScheduleManage();
}

void GpuCommandBufferStub::OnDiscardBackbuffer() {
  if (!surface_)
    return;
  surface_->SetBackbufferAllocation(false);
}

void GpuCommandBufferStub::OnEnsureBackbuffer() {
  if (!surface_)
    return;
  surface_->SetBackbufferAllocation(true);
}

void GpuCommandBufferStub::OnSetClientHasMemoryAllocationChangedCallback(
    bool has_callback) {
  client_has_memory_allocation_changed_callback_ = has_callback;
  channel_->gpu_channel_manager()->gpu_memory_manager()->ScheduleManage();
}

void GpuCommandBufferStub::SendConsoleMessage(
    int32 id,
    const std::string& message) {
  GPUCommandBufferConsoleMessage console_message;
  console_message.id = id;
  console_message.message = message;
  IPC::Message* msg = new GpuCommandBufferMsg_ConsoleMsg(
      route_id_, console_message);
  msg->set_unblock(true);
  Send(msg);
}

void GpuCommandBufferStub::AddDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.AddObserver(observer);
}

void GpuCommandBufferStub::RemoveDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.RemoveObserver(observer);
}

gfx::Size GpuCommandBufferStub::GetSurfaceSize() const {
  if (!surface_)
    return gfx::Size();
  return surface_->GetSize();
}

bool GpuCommandBufferStub::IsInSameContextShareGroup(
    const GpuCommandBufferStubBase& other) const {
  return context_group_ ==
      static_cast<const GpuCommandBufferStub&>(other).context_group_;
}

bool GpuCommandBufferStub::
    client_has_memory_allocation_changed_callback() const {
  return client_has_memory_allocation_changed_callback_;
}

bool GpuCommandBufferStub::has_surface_state() const {
  return surface_state_ != NULL;
}

const GpuCommandBufferStubBase::SurfaceState&
    GpuCommandBufferStub::surface_state() const {
  DCHECK(has_surface_state());
  return *surface_state_.get();
}

void GpuCommandBufferStub::SetMemoryAllocation(
    const GpuMemoryAllocation& allocation) {
  Send(new GpuCommandBufferMsg_SetMemoryAllocation(route_id_, allocation));
  if (!surface_)
    return;
  surface_->SetFrontbufferAllocation(allocation.suggest_have_frontbuffer);
}

#endif  // defined(ENABLE_GPU)
