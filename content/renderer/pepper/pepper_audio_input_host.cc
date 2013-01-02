// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_audio_input_host.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ipc/ipc_message.h"
#include "media/audio/shared_memory_util.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_structs.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

namespace content {

namespace {

base::PlatformFile ConvertSyncSocketHandle(const base::SyncSocket& socket) {
  return socket.handle();
}

base::PlatformFile ConvertSharedMemoryHandle(
    const base::SharedMemory& shared_memory) {
#if defined(OS_POSIX)
  return shared_memory.handle().fd;
#elif defined(OS_WIN)
  return shared_memory.handle();
#else
#error "Platform not supported."
#endif
}

}  // namespace

PepperAudioInputHost::PepperAudioInputHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      audio_input_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          enumeration_helper_(this, this, PP_DEVICETYPE_DEV_AUDIOCAPTURE)) {
}

PepperAudioInputHost::~PepperAudioInputHost() {
  Close();
}

int32_t PepperAudioInputHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  int32_t result = PP_ERROR_FAILED;
  if (enumeration_helper_.HandleResourceMessage(msg, context, &result))
    return result;

  IPC_BEGIN_MESSAGE_MAP(PepperAudioInputHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_AudioInput_Open, OnOpen)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_AudioInput_StartOrStop,
                                      OnStartOrStop);
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_AudioInput_Close,
                                        OnClose);
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

void PepperAudioInputHost::StreamCreated(
    base::SharedMemoryHandle shared_memory_handle,
    size_t shared_memory_size,
    base::SyncSocket::Handle socket) {
  OnOpenComplete(PP_OK, shared_memory_handle, shared_memory_size, socket);
}

void PepperAudioInputHost::StreamCreationFailed() {
  OnOpenComplete(PP_ERROR_FAILED, base::SharedMemory::NULLHandle(), 0,
                 base::SyncSocket::kInvalidHandle);
}

webkit::ppapi::PluginDelegate* PepperAudioInputHost::GetPluginDelegate() {
  webkit::ppapi::PluginInstance* instance =
      renderer_ppapi_host_->GetPluginInstance(pp_instance());
  if (instance)
    return instance->delegate();
  return NULL;
}

int32_t PepperAudioInputHost::OnOpen(
    ppapi::host::HostMessageContext* context,
    const std::string& device_id,
    PP_AudioSampleRate sample_rate,
    uint32_t sample_frame_count) {
  if (open_context_.get())
    return PP_ERROR_INPROGRESS;
  if (audio_input_)
    return PP_ERROR_FAILED;

  webkit::ppapi::PluginDelegate* plugin_delegate = GetPluginDelegate();
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  // When it is done, we'll get called back on StreamCreated() or
  // StreamCreationFailed().
  audio_input_ = plugin_delegate->CreateAudioInput(
      device_id, sample_rate, sample_frame_count, this);
  if (audio_input_) {
    open_context_.reset(new ppapi::host::ReplyMessageContext(
        context->MakeReplyMessageContext()));
    return PP_OK_COMPLETIONPENDING;
  } else {
    return PP_ERROR_FAILED;
  }
}

int32_t PepperAudioInputHost::OnStartOrStop(
    ppapi::host::HostMessageContext* /* context */,
    bool capture) {
  if (!audio_input_)
    return PP_ERROR_FAILED;
  if (capture)
    audio_input_->StartCapture();
  else
    audio_input_->StopCapture();
  return PP_OK;
}

int32_t PepperAudioInputHost::OnClose(
    ppapi::host::HostMessageContext* /* context */) {
  Close();
  return PP_OK;
}

void PepperAudioInputHost::OnOpenComplete(
    int32_t result,
    base::SharedMemoryHandle shared_memory_handle,
    size_t shared_memory_size,
    base::SyncSocket::Handle socket_handle) {
  // Make sure the handles are cleaned up.
  base::SyncSocket scoped_socket(socket_handle);
  base::SharedMemory scoped_shared_memory(shared_memory_handle, false);

  if (!open_context_.get()) {
    NOTREACHED();
    return;
  }

  ppapi::proxy::SerializedHandle serialized_socket_handle(
      ppapi::proxy::SerializedHandle::SOCKET);
  ppapi::proxy::SerializedHandle serialized_shared_memory_handle(
      ppapi::proxy::SerializedHandle::SHARED_MEMORY);

  if (result == PP_OK) {
    IPC::PlatformFileForTransit temp_socket =
        IPC::InvalidPlatformFileForTransit();
    base::SharedMemoryHandle temp_shmem = base::SharedMemory::NULLHandle();
    result = GetRemoteHandles(
        scoped_socket, scoped_shared_memory, &temp_socket, &temp_shmem);

    serialized_socket_handle.set_socket(temp_socket);
    // Note that we must call TotalSharedMemorySizeInBytes() because extra space
    // in shared memory is allocated for book-keeping, so the actual size of the
    // shared memory buffer is larger than |shared_memory_size|. When sending to
    // NaCl, NaClIPCAdapter expects this size to match the size of the full
    // shared memory buffer.
    serialized_shared_memory_handle.set_shmem(
        temp_shmem, media::TotalSharedMemorySizeInBytes(shared_memory_size));
  }

  // Send all the values, even on error. This simplifies some of our cleanup
  // code since the handles will be in the other process and could be
  // inconvenient to clean up. Our IPC code will automatically handle this for
  // us, as long as the remote side always closes the handles it receives, even
  // in the failure case.
  open_context_->params.set_result(result);
  open_context_->params.AppendHandle(serialized_socket_handle);
  open_context_->params.AppendHandle(serialized_shared_memory_handle);

  host()->SendReply(*open_context_, PpapiPluginMsg_AudioInput_OpenReply());
  open_context_.reset();
}

int32_t PepperAudioInputHost::GetRemoteHandles(
    const base::SyncSocket& socket,
    const base::SharedMemory& shared_memory,
    IPC::PlatformFileForTransit* remote_socket_handle,
    base::SharedMemoryHandle* remote_shared_memory_handle) {
  *remote_socket_handle = renderer_ppapi_host_->ShareHandleWithRemote(
      ConvertSyncSocketHandle(socket), false);
  if (*remote_socket_handle == IPC::InvalidPlatformFileForTransit())
    return PP_ERROR_FAILED;

  *remote_shared_memory_handle = renderer_ppapi_host_->ShareHandleWithRemote(
      ConvertSharedMemoryHandle(shared_memory), false);
  if (*remote_shared_memory_handle == IPC::InvalidPlatformFileForTransit())
    return PP_ERROR_FAILED;

  return PP_OK;
}

void PepperAudioInputHost::Close() {
  if (!audio_input_)
    return;

  audio_input_->ShutDown();
  audio_input_ = NULL;

  if (open_context_.get()) {
    open_context_->params.set_result(PP_ERROR_ABORTED);
    host()->SendReply(*open_context_, PpapiPluginMsg_AudioInput_OpenReply());
    open_context_.reset();
  }
}

}  // namespace content

