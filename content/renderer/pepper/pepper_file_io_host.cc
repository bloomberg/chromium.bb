// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_file_io_host.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util_proxy.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/thunk/enter.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/plugins/ppapi/file_callbacks.h"
#include "webkit/plugins/ppapi/host_globals.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"
#include "webkit/plugins/ppapi/quota_file_io.h"

namespace content {

using ppapi::FileIOStateManager;
using ppapi::PPTimeToTime;
using ppapi::TimeToPPTime;
using ppapi::host::ReplyMessageContext;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_FileRef_API;
using webkit::ppapi::PPB_FileRef_Impl;
using webkit::ppapi::PluginDelegate;

namespace {

// The maximum size we'll support reading in one chunk. The renderer process
// must allocate a buffer sized according to the request of the plugin. To
// keep things from getting out of control, we cap the read size to this value.
// This should generally be OK since the API specifies that it may perform a
// partial read.
static const int32_t kMaxReadSize = 32 * 1024 * 1024;  // 32MB

typedef base::Callback<void (base::PlatformFileError)> PlatformGeneralCallback;

class PlatformGeneralCallbackTranslator
    : public fileapi::FileSystemCallbackDispatcher {
 public:
  explicit PlatformGeneralCallbackTranslator(
      const PlatformGeneralCallback& callback)
    : callback_(callback) {}

  virtual ~PlatformGeneralCallbackTranslator() {}

  virtual void DidSucceed() OVERRIDE {
    callback_.Run(base::PLATFORM_FILE_OK);
  }

  virtual void DidReadMetadata(const base::PlatformFileInfo& file_info,
                               const base::FilePath& platform_path) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidCreateSnapshotFile(
      const base::PlatformFileInfo& file_info,
      const base::FilePath& platform_path) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidOpenFileSystem(const std::string& name,
                                 const GURL& root) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidFail(base::PlatformFileError error_code) OVERRIDE {
    callback_.Run(error_code);
  }

  virtual void DidWrite(int64 bytes, bool complete) OVERRIDE {
    NOTREACHED();
  }

  virtual void DidOpenFile(base::PlatformFile file) OVERRIDE {
    NOTREACHED();
  }

 private:
  PlatformGeneralCallback callback_;
};

int32_t ErrorOrByteNumber(int32_t pp_error, int32_t byte_number) {
  // On the plugin side, some callbacks expect a parameter that means different
  // things depending on whether is negative or not.  We translate for those
  // callbacks here.
  return pp_error == PP_OK ? byte_number : pp_error;
}

}  // namespace

PepperFileIOHost::PepperFileIOHost(RendererPpapiHost* host,
                                   PP_Instance instance,
                                   PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      file_(base::kInvalidPlatformFileValue),
      file_system_type_(PP_FILESYSTEMTYPE_INVALID),
      is_running_in_process_(host->IsRunningInProcess()),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  // TODO(victorhsieh): eliminate plugin_delegate_ as it's no longer needed.
  webkit::ppapi::PluginInstance* plugin_instance =
      webkit::ppapi::HostGlobals::Get()->GetInstance(instance);
  plugin_delegate_ = plugin_instance ? plugin_instance->delegate() : NULL;
}

PepperFileIOHost::~PepperFileIOHost() {
  OnHostMsgClose(NULL);
}

int32_t PepperFileIOHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperFileIOHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileIO_Open,
                                      OnHostMsgOpen)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FileIO_Query,
                                        OnHostMsgQuery)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileIO_Touch,
                                      OnHostMsgTouch)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileIO_Read,
                                      OnHostMsgRead)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileIO_Write,
                                      OnHostMsgWrite)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileIO_SetLength,
                                      OnHostMsgSetLength)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FileIO_Flush,
                                        OnHostMsgFlush)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FileIO_Close,
                                        OnHostMsgClose)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileIO_WillWrite,
                                      OnHostMsgWillWrite)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileIO_WillSetLength,
                                      OnHostMsgWillSetLength)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FileIO_GetOSFileDescriptor,
                                        OnHostMsgGetOSFileDescriptor)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FileIO_RequestOSFileHandle,
                                        OnHostMsgRequestOSFileHandle)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperFileIOHost::OnHostMsgOpen(
    ppapi::host::HostMessageContext* context,
    PP_Resource file_ref_resource,
    int32_t open_flags) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, false);
  if (rv != PP_OK)
    return rv;

  int flags = 0;
  if (!::ppapi::PepperFileOpenFlagsToPlatformFileFlags(open_flags, &flags))
    return PP_ERROR_BADARGUMENT;

  EnterResourceNoLock<PPB_FileRef_API> enter(file_ref_resource, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;

  PPB_FileRef_API* file_ref_api = enter.object();
  PP_FileSystemType type = file_ref_api->GetFileSystemType();
  if (type != PP_FILESYSTEMTYPE_LOCALPERSISTENT &&
      type != PP_FILESYSTEMTYPE_LOCALTEMPORARY &&
      type != PP_FILESYSTEMTYPE_EXTERNAL)
    return PP_ERROR_FAILED;
  file_system_type_ = type;

  if (!plugin_delegate_)
    return PP_ERROR_FAILED;

  PPB_FileRef_Impl* file_ref = static_cast<PPB_FileRef_Impl*>(file_ref_api);
  if (file_ref->HasValidFileSystem()) {
    file_system_url_ = file_ref->GetFileSystemURL();
    if (!plugin_delegate_->AsyncOpenFileSystemURL(
            file_system_url_, flags,
            base::Bind(
                &PepperFileIOHost::ExecutePlatformOpenFileSystemURLCallback,
                weak_factory_.GetWeakPtr(),
                context->MakeReplyMessageContext())))
      return PP_ERROR_FAILED;
  } else {
    if (file_system_type_ != PP_FILESYSTEMTYPE_EXTERNAL)
      return PP_ERROR_FAILED;
    if (!plugin_delegate_->AsyncOpenFile(
            file_ref->GetSystemPath(), flags,
            base::Bind(&PepperFileIOHost::ExecutePlatformOpenFileCallback,
                       weak_factory_.GetWeakPtr(),
                       context->MakeReplyMessageContext())))
      return PP_ERROR_FAILED;
  }

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileIOHost::OnHostMsgQuery(
    ppapi::host::HostMessageContext* context) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;

  if (!plugin_delegate_)
    return PP_ERROR_FAILED;

  if (!base::FileUtilProxy::GetFileInfoFromPlatformFile(
          plugin_delegate_->GetFileThreadMessageLoopProxy(), file_,
          base::Bind(&PepperFileIOHost::ExecutePlatformQueryCallback,
                     weak_factory_.GetWeakPtr(),
                     context->MakeReplyMessageContext())))
    return PP_ERROR_FAILED;

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileIOHost::OnHostMsgTouch(
    ppapi::host::HostMessageContext* context,
    PP_Time last_access_time,
    PP_Time last_modified_time) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;

  if (!plugin_delegate_)
    return PP_ERROR_FAILED;

  if (file_system_type_ != PP_FILESYSTEMTYPE_EXTERNAL) {
    if (!plugin_delegate_->Touch(
            file_system_url_,
            PPTimeToTime(last_access_time),
            PPTimeToTime(last_modified_time),
            new PlatformGeneralCallbackTranslator(
                base::Bind(&PepperFileIOHost::ExecutePlatformGeneralCallback,
                           weak_factory_.GetWeakPtr(),
                           context->MakeReplyMessageContext()))))
      return PP_ERROR_FAILED;
    state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
    return PP_OK_COMPLETIONPENDING;
  }

  // TODO(nhiroki): fix a failure of FileIO.Touch for an external filesystem on
  // Mac and Linux due to sandbox restrictions (http://crbug.com/101128).
  if (!base::FileUtilProxy::Touch(
          plugin_delegate_->GetFileThreadMessageLoopProxy(),
          file_, PPTimeToTime(last_access_time),
          PPTimeToTime(last_modified_time),
          base::Bind(&PepperFileIOHost::ExecutePlatformGeneralCallback,
                     weak_factory_.GetWeakPtr(),
                     context->MakeReplyMessageContext())))
    return PP_ERROR_FAILED;

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileIOHost::OnHostMsgRead(
    ppapi::host::HostMessageContext* context,
    int64_t offset,
    int32_t max_read_length) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_READ, true);
  if (rv != PP_OK)
    return rv;

  // Validate max_read_length before allocating below. This value is coming from
  // the untrusted plugin.
  if (max_read_length < 0) {
    ReplyMessageContext reply_context = context->MakeReplyMessageContext();
    reply_context.params.set_result(PP_ERROR_FAILED);
    host()->SendReply(reply_context,
                      PpapiPluginMsg_FileIO_ReadReply(std::string()));
    return PP_OK_COMPLETIONPENDING;
  }

  if (!plugin_delegate_)
    return PP_ERROR_FAILED;

  if (!base::FileUtilProxy::Read(
          plugin_delegate_->GetFileThreadMessageLoopProxy(), file_, offset,
          max_read_length,
          base::Bind(&PepperFileIOHost::ExecutePlatformReadCallback,
                     weak_factory_.GetWeakPtr(),
                     context->MakeReplyMessageContext())))
    return PP_ERROR_FAILED;

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_READ);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileIOHost::OnHostMsgWrite(
    ppapi::host::HostMessageContext* context,
    int64_t offset,
    const std::string& buffer) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_WRITE, true);
  if (rv != PP_OK)
    return rv;

  if (quota_file_io_.get()) {
    if (!quota_file_io_->Write(
            offset, buffer.c_str(), buffer.size(),
            base::Bind(&PepperFileIOHost::ExecutePlatformWriteCallback,
                       weak_factory_.GetWeakPtr(),
                       context->MakeReplyMessageContext())))
      return PP_ERROR_FAILED;
  } else {
    if (!plugin_delegate_)
      return PP_ERROR_FAILED;

    if (!base::FileUtilProxy::Write(
            plugin_delegate_->GetFileThreadMessageLoopProxy(), file_, offset,
            buffer.c_str(), buffer.size(),
            base::Bind(&PepperFileIOHost::ExecutePlatformWriteCallback,
                       weak_factory_.GetWeakPtr(),
                       context->MakeReplyMessageContext())))
      return PP_ERROR_FAILED;
  }

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_WRITE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileIOHost::OnHostMsgSetLength(
    ppapi::host::HostMessageContext* context,
    int64_t length) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;

  if (!plugin_delegate_)
    return PP_ERROR_FAILED;

  if (file_system_type_ != PP_FILESYSTEMTYPE_EXTERNAL) {
    if (!plugin_delegate_->SetLength(
            file_system_url_, length,
            new PlatformGeneralCallbackTranslator(
                base::Bind(&PepperFileIOHost::ExecutePlatformGeneralCallback,
                           weak_factory_.GetWeakPtr(),
                           context->MakeReplyMessageContext()))))
      return PP_ERROR_FAILED;
  } else {
    // TODO(nhiroki): fix a failure of FileIO.SetLength for an external
    // filesystem on Mac due to sandbox restrictions (http://crbug.com/156077).
    if (!base::FileUtilProxy::Truncate(
            plugin_delegate_->GetFileThreadMessageLoopProxy(), file_, length,
            base::Bind(&PepperFileIOHost::ExecutePlatformGeneralCallback,
                       weak_factory_.GetWeakPtr(),
                       context->MakeReplyMessageContext())))
      return PP_ERROR_FAILED;
  }

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileIOHost::OnHostMsgFlush(
    ppapi::host::HostMessageContext* context) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;

  if (!plugin_delegate_)
    return PP_ERROR_FAILED;

  if (!base::FileUtilProxy::Flush(
          plugin_delegate_->GetFileThreadMessageLoopProxy(), file_,
          base::Bind(&PepperFileIOHost::ExecutePlatformGeneralCallback,
                     weak_factory_.GetWeakPtr(),
                     context->MakeReplyMessageContext())))
    return PP_ERROR_FAILED;

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileIOHost::OnHostMsgClose(
    ppapi::host::HostMessageContext* context) {
  if (file_ != base::kInvalidPlatformFileValue && plugin_delegate_) {
    base::FileUtilProxy::Close(
        plugin_delegate_->GetFileThreadMessageLoopProxy(),
        file_,
        base::ResetAndReturn(&notify_close_file_callback_));
    file_ = base::kInvalidPlatformFileValue;
    quota_file_io_.reset();
  }
  return PP_OK;
}

int32_t PepperFileIOHost::OnHostMsgWillWrite(
    ppapi::host::HostMessageContext* context,
    int64_t offset,
    int32_t bytes_to_write) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;

  if (!quota_file_io_.get())
    return PP_OK;

  if (!quota_file_io_->WillWrite(
          offset, bytes_to_write,
          base::Bind(&PepperFileIOHost::ExecutePlatformWillWriteCallback,
                     weak_factory_.GetWeakPtr(),
                     context->MakeReplyMessageContext())))
    return PP_ERROR_FAILED;

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileIOHost::OnHostMsgWillSetLength(
    ppapi::host::HostMessageContext* context,
    int64_t length) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;

  if (!quota_file_io_.get())
    return PP_OK;

  if (!quota_file_io_->WillSetLength(
          length,
          base::Bind(&PepperFileIOHost::ExecutePlatformGeneralCallback,
                     weak_factory_.GetWeakPtr(),
                     context->MakeReplyMessageContext())))
    return PP_ERROR_FAILED;

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileIOHost::OnHostMsgRequestOSFileHandle(
    ppapi::host::HostMessageContext* context) {
  if (!is_running_in_process_ &&
      !GetContentClient()->renderer()->IsRequestOSFileHandleAllowedForURL(
          file_system_url_))
    return PP_ERROR_FAILED;

  // TODO(hamaji): Should fail if quota is not unlimited.
  // http://crbug.com/224123

  RendererPpapiHost* renderer_ppapi_host =
      RendererPpapiHost::GetForPPInstance(pp_instance());

  IPC::PlatformFileForTransit file =
      renderer_ppapi_host->ShareHandleWithRemote(file_, false);
  if (file == IPC::InvalidPlatformFileForTransit())
    return PP_ERROR_FAILED;
  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  reply_context.params.AppendHandle(ppapi::proxy::SerializedHandle(
      ppapi::proxy::SerializedHandle::FILE, file));
  host()->SendReply(reply_context,
                    PpapiPluginMsg_FileIO_RequestOSFileHandleReply());
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileIOHost::OnHostMsgGetOSFileDescriptor(
    ppapi::host::HostMessageContext* context) {
  if (!is_running_in_process_)
    return PP_ERROR_FAILED;

  int32_t fd =
#if defined(OS_POSIX)
      file_;
#elif defined(OS_WIN)
      reinterpret_cast<uintptr_t>(file_);
#else
      -1;
#endif

  host()->SendReply(context->MakeReplyMessageContext(),
                    PpapiPluginMsg_FileIO_GetOSFileDescriptorReply(fd));
  return PP_OK_COMPLETIONPENDING;
}

void PepperFileIOHost::ExecutePlatformGeneralCallback(
    ppapi::host::ReplyMessageContext reply_context,
    base::PlatformFileError error_code) {
  reply_context.params.set_result(
      ::ppapi::PlatformFileErrorToPepperError(error_code));
  host()->SendReply(reply_context, PpapiPluginMsg_FileIO_GeneralReply());
  state_manager_.SetOperationFinished();
}

void PepperFileIOHost::ExecutePlatformOpenFileCallback(
    ppapi::host::ReplyMessageContext reply_context,
    base::PlatformFileError error_code,
    base::PassPlatformFile file) {
  int32_t pp_error = ::ppapi::PlatformFileErrorToPepperError(error_code);
  if (pp_error == PP_OK)
    state_manager_.SetOpenSucceed();

  DCHECK(file_ == base::kInvalidPlatformFileValue);
  file_ = file.ReleaseValue();

  DCHECK(!quota_file_io_.get());
  if (file_ != base::kInvalidPlatformFileValue &&
      (file_system_type_ == PP_FILESYSTEMTYPE_LOCALTEMPORARY ||
       file_system_type_ == PP_FILESYSTEMTYPE_LOCALPERSISTENT)) {
    quota_file_io_.reset(new webkit::ppapi::QuotaFileIO(
        pp_instance(), file_, file_system_url_, file_system_type_));
  }

  reply_context.params.set_result(pp_error);
  host()->SendReply(reply_context, PpapiPluginMsg_FileIO_OpenReply());
  state_manager_.SetOperationFinished();
}

void PepperFileIOHost::ExecutePlatformOpenFileSystemURLCallback(
    ppapi::host::ReplyMessageContext reply_context,
    base::PlatformFileError error_code,
    base::PassPlatformFile file,
    const PluginDelegate::NotifyCloseFileCallback& callback) {
  if (error_code == base::PLATFORM_FILE_OK)
    notify_close_file_callback_ = callback;
  ExecutePlatformOpenFileCallback(reply_context, error_code, file);
}

void PepperFileIOHost::ExecutePlatformQueryCallback(
    ppapi::host::ReplyMessageContext reply_context,
    base::PlatformFileError error_code,
    const base::PlatformFileInfo& file_info) {
  PP_FileInfo pp_info;
  pp_info.size = file_info.size;
  pp_info.creation_time = TimeToPPTime(file_info.creation_time);
  pp_info.last_access_time = TimeToPPTime(file_info.last_accessed);
  pp_info.last_modified_time = TimeToPPTime(file_info.last_modified);
  pp_info.system_type = file_system_type_;
  if (file_info.is_directory)
    pp_info.type = PP_FILETYPE_DIRECTORY;
  else
    pp_info.type = PP_FILETYPE_REGULAR;

  int32_t pp_error = ::ppapi::PlatformFileErrorToPepperError(error_code);
  reply_context.params.set_result(pp_error);
  host()->SendReply(reply_context,
                    PpapiPluginMsg_FileIO_QueryReply(pp_info));
  state_manager_.SetOperationFinished();
}

void PepperFileIOHost::ExecutePlatformReadCallback(
    ppapi::host::ReplyMessageContext reply_context,
    base::PlatformFileError error_code,
    const char* data, int bytes_read) {
  int32_t pp_error = ::ppapi::PlatformFileErrorToPepperError(error_code);

  // Only send the amount of data in the string that was actually read.
  std::string buffer;
  if (pp_error == PP_OK)
    buffer.append(data, bytes_read);
  reply_context.params.set_result(ErrorOrByteNumber(pp_error, bytes_read));
  host()->SendReply(reply_context, PpapiPluginMsg_FileIO_ReadReply(buffer));
  state_manager_.SetOperationFinished();
}

void PepperFileIOHost::ExecutePlatformWriteCallback(
    ppapi::host::ReplyMessageContext reply_context,
    base::PlatformFileError error_code,
    int bytes_written) {
  int32_t pp_error = ::ppapi::PlatformFileErrorToPepperError(error_code);
  reply_context.params.set_result(ErrorOrByteNumber(pp_error, bytes_written));
  host()->SendReply(reply_context, PpapiPluginMsg_FileIO_GeneralReply());
  state_manager_.SetOperationFinished();
}

void PepperFileIOHost::ExecutePlatformWillWriteCallback(
    ppapi::host::ReplyMessageContext reply_context,
    base::PlatformFileError error_code,
    int bytes_written) {
  // On the plugin side, the callback expects a parameter with different meaning
  // depends on whether is negative or not. It is the result here. We translate
  // for the callback.
  int32_t pp_error = ::ppapi::PlatformFileErrorToPepperError(error_code);
  reply_context.params.set_result(ErrorOrByteNumber(pp_error, bytes_written));
  host()->SendReply(reply_context, PpapiPluginMsg_FileIO_GeneralReply());
  state_manager_.SetOperationFinished();
}

}  // namespace content
