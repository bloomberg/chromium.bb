// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_file_io_host.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util_proxy.h"
#include "content/child/child_thread.h"
#include "content/child/fileapi/file_system_dispatcher.h"
#include "content/child/quota_dispatcher.h"
#include "content/common/fileapi/file_system_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/pepper/pepper_file_ref_renderer_host.h"
#include "content/renderer/pepper/quota_file_io.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_system_util.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/thunk/enter.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"

namespace content {

using ppapi::FileIOStateManager;
using ppapi::PPTimeToTime;
using ppapi::host::ReplyMessageContext;
using ppapi::thunk::EnterResourceNoLock;

namespace {

typedef base::Callback<void (base::PlatformFileError)> PlatformGeneralCallback;

int32_t ErrorOrByteNumber(int32_t pp_error, int32_t byte_number) {
  // On the plugin side, some callbacks expect a parameter that means different
  // things depending on whether is negative or not.  We translate for those
  // callbacks here.
  return pp_error == PP_OK ? byte_number : pp_error;
}

class QuotaCallbackTranslator : public QuotaDispatcher::Callback {
 public:
  typedef QuotaFileIO::Delegate::AvailableSpaceCallback PluginCallback;
  explicit QuotaCallbackTranslator(const PluginCallback& cb) : callback_(cb) {}
  virtual void DidQueryStorageUsageAndQuota(int64 usage, int64 quota) OVERRIDE {
    callback_.Run(std::max(static_cast<int64>(0), quota - usage));
  }
  virtual void DidGrantStorageQuota(int64 granted_quota) OVERRIDE {
    NOTREACHED();
  }
  virtual void DidFail(quota::QuotaStatusCode error) OVERRIDE {
    callback_.Run(0);
  }
 private:
  PluginCallback callback_;
};

class QuotaFileIODelegate : public QuotaFileIO::Delegate {
 public:
  QuotaFileIODelegate() {}
  virtual ~QuotaFileIODelegate() {}

  virtual void QueryAvailableSpace(
      const GURL& origin,
      quota::StorageType type,
      const AvailableSpaceCallback& callback) OVERRIDE {
    ChildThread::current()->quota_dispatcher()->QueryStorageUsageAndQuota(
        origin, type, new QuotaCallbackTranslator(callback));
  }
  virtual void WillUpdateFile(const GURL& file_path) OVERRIDE {
    ChildThread::current()->Send(new FileSystemHostMsg_WillUpdate(file_path));
  }
  virtual void DidUpdateFile(const GURL& file_path, int64_t delta) OVERRIDE {
    ChildThread::current()->Send(new FileSystemHostMsg_DidUpdate(
        file_path, delta));
  }
  virtual scoped_refptr<base::MessageLoopProxy>
      GetFileThreadMessageLoopProxy() OVERRIDE {
    return RenderThreadImpl::current()->GetFileThreadMessageLoopProxy();
  }
};

typedef base::Callback<
    void (base::PlatformFileError error,
          base::PassPlatformFile file,
          quota::QuotaLimitType quota_policy,
          const PepperFileIOHost::NotifyCloseFileCallback& close_file_callback)>
    AsyncOpenFileSystemURLCallback;

void DoNotifyCloseFile(int file_open_id, base::PlatformFileError error) {
  ChildThread::current()->file_system_dispatcher()->NotifyCloseFile(
      file_open_id);
}

void DidOpenFileSystemURL(const AsyncOpenFileSystemURLCallback& callback,
                          base::PlatformFile file,
                          int file_open_id,
                          quota::QuotaLimitType quota_policy) {
  callback.Run(base::PLATFORM_FILE_OK,
               base::PassPlatformFile(&file),
               quota_policy,
               base::Bind(&DoNotifyCloseFile, file_open_id));
  // Make sure we won't leak file handle if the requester has died.
  if (file != base::kInvalidPlatformFileValue) {
    base::FileUtilProxy::Close(
        RenderThreadImpl::current()->GetFileThreadMessageLoopProxy().get(),
        file,
        base::Bind(&DoNotifyCloseFile, file_open_id));
  }
}

void DidFailOpenFileSystemURL(const AsyncOpenFileSystemURLCallback& callback,
    base::PlatformFileError error_code) {
  base::PlatformFile invalid_file = base::kInvalidPlatformFileValue;
  callback.Run(error_code,
               base::PassPlatformFile(&invalid_file),
               quota::kQuotaLimitTypeUnknown,
               PepperFileIOHost::NotifyCloseFileCallback());
}

}  // namespace

PepperFileIOHost::PepperFileIOHost(RendererPpapiHost* host,
                                   PP_Instance instance,
                                   PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      file_(base::kInvalidPlatformFileValue),
      file_system_type_(PP_FILESYSTEMTYPE_INVALID),
      quota_policy_(quota::kQuotaLimitTypeUnknown),
      open_flags_(0),
      routing_id_(RenderThreadImpl::current()->GenerateRoutingID()),
      weak_factory_(this) {
  ChildThread::current()->AddRoute(routing_id_, this);
}

PepperFileIOHost::~PepperFileIOHost() {
  OnHostMsgClose(NULL);
  ChildThread::current()->RemoveRoute(routing_id_);
}

int32_t PepperFileIOHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperFileIOHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileIO_Open,
                                      OnHostMsgOpen)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileIO_Touch,
                                      OnHostMsgTouch)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileIO_Write,
                                      OnHostMsgWrite)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileIO_SetLength,
                                      OnHostMsgSetLength)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FileIO_Flush,
                                        OnHostMsgFlush)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FileIO_Close,
                                        OnHostMsgClose)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FileIO_RequestOSFileHandle,
                                        OnHostMsgRequestOSFileHandle)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

bool PepperFileIOHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PepperFileIOHost, msg)
    IPC_MESSAGE_HANDLER(ViewMsg_AsyncOpenPepperFile_ACK, OnAsyncFileOpened)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PepperFileIOHost::OnAsyncFileOpened(
    base::PlatformFileError error_code,
    IPC::PlatformFileForTransit file_for_transit) {
  DCHECK(!pending_open_callback_.is_null());
  base::PlatformFile file =
      IPC::PlatformFileForTransitToPlatformFile(file_for_transit);
  if (!pending_open_callback_.is_null())
    pending_open_callback_.Run(error_code, base::PassPlatformFile(&file));

  // Make sure we won't leak file handle if the requester has died.
  if (file != base::kInvalidPlatformFileValue) {
    base::FileUtilProxy::Close(
        RenderThreadImpl::current()->GetFileThreadMessageLoopProxy().get(),
        file,
        base::FileUtilProxy::StatusCallback());
  }
  pending_open_callback_.Reset();
}

int32_t PepperFileIOHost::OnHostMsgOpen(
    ppapi::host::HostMessageContext* context,
    PP_Resource file_ref_resource,
    int32_t open_flags) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, false);
  if (rv != PP_OK)
    return rv;

  if (!ppapi::PepperFileOpenFlagsToPlatformFileFlags(open_flags, NULL))
    return PP_ERROR_BADARGUMENT;

  ppapi::host::ResourceHost* resource_host =
      renderer_ppapi_host_->GetPpapiHost()->GetResourceHost(file_ref_resource);
  if (!resource_host || !resource_host->IsFileRefHost())
    return PP_ERROR_BADRESOURCE;

  PepperFileRefRendererHost* file_ref_host =
      static_cast<PepperFileRefRendererHost*>(resource_host);
  if (file_ref_host->GetFileSystemType() == PP_FILESYSTEMTYPE_INVALID)
    return PP_ERROR_FAILED;

  open_flags_ = open_flags;
  file_system_type_ = file_ref_host->GetFileSystemType();

  if (file_system_type_ != PP_FILESYSTEMTYPE_EXTERNAL) {
    file_system_url_ = file_ref_host->GetFileSystemURL();
    FileSystemDispatcher* file_system_dispatcher =
        ChildThread::current()->file_system_dispatcher();

    AsyncOpenFileSystemURLCallback callback = base::Bind(
        &PepperFileIOHost::ExecutePlatformOpenFileSystemURLCallback,
        weak_factory_.GetWeakPtr(),
        context->MakeReplyMessageContext());
    file_system_dispatcher->OpenPepperFile(
        file_system_url_, open_flags,
        base::Bind(&DidOpenFileSystemURL, callback),
        base::Bind(&DidFailOpenFileSystemURL, callback));
  } else {
    pending_open_callback_ =
        base::Bind(&PepperFileIOHost::ExecutePlatformOpenFileCallback,
                   weak_factory_.GetWeakPtr(),
                   context->MakeReplyMessageContext());
    RenderThreadImpl::current()->Send(new ViewHostMsg_AsyncOpenPepperFile(
        routing_id_,
        file_ref_host->GetExternalFilePath(),
        open_flags_));
  }
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

  if (file_system_type_ != PP_FILESYSTEMTYPE_EXTERNAL) {
    FileSystemDispatcher* file_system_dispatcher =
        ChildThread::current()->file_system_dispatcher();
    file_system_dispatcher->TouchFile(
        file_system_url_,
        PPTimeToTime(last_access_time),
        PPTimeToTime(last_modified_time),
        base::Bind(&PepperFileIOHost::ExecutePlatformGeneralCallback,
                   weak_factory_.GetWeakPtr(),
                   context->MakeReplyMessageContext()));
    state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
    return PP_OK_COMPLETIONPENDING;
  }

  // TODO(nhiroki): fix a failure of FileIO.Touch for an external filesystem on
  // Mac and Linux due to sandbox restrictions (http://crbug.com/101128).
  if (!base::FileUtilProxy::Touch(
          RenderThreadImpl::current()->GetFileThreadMessageLoopProxy().get(),
          file_,
          PPTimeToTime(last_access_time),
          PPTimeToTime(last_modified_time),
          base::Bind(&PepperFileIOHost::ExecutePlatformGeneralCallback,
                     weak_factory_.GetWeakPtr(),
                     context->MakeReplyMessageContext())))
    return PP_ERROR_FAILED;

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
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

  if (quota_file_io_) {
    if (!quota_file_io_->Write(
            offset, buffer.c_str(), buffer.size(),
            base::Bind(&PepperFileIOHost::ExecutePlatformWriteCallback,
                       weak_factory_.GetWeakPtr(),
                       context->MakeReplyMessageContext())))
      return PP_ERROR_FAILED;
  } else {
    if (!base::FileUtilProxy::Write(
            RenderThreadImpl::current()->GetFileThreadMessageLoopProxy().get(),
            file_,
            offset,
            buffer.c_str(),
            buffer.size(),
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

  if (file_system_type_ != PP_FILESYSTEMTYPE_EXTERNAL) {
    FileSystemDispatcher* file_system_dispatcher =
        ChildThread::current()->file_system_dispatcher();
    file_system_dispatcher->Truncate(
        file_system_url_, length, NULL,
        base::Bind(&PepperFileIOHost::ExecutePlatformGeneralCallback,
                   weak_factory_.GetWeakPtr(),
                   context->MakeReplyMessageContext()));
  } else {
    // TODO(nhiroki): fix a failure of FileIO.SetLength for an external
    // filesystem on Mac due to sandbox restrictions (http://crbug.com/156077).
    if (!base::FileUtilProxy::Truncate(
            RenderThreadImpl::current()->GetFileThreadMessageLoopProxy().get(),
            file_,
            length,
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

  if (!base::FileUtilProxy::Flush(
          RenderThreadImpl::current()->GetFileThreadMessageLoopProxy().get(),
          file_,
          base::Bind(&PepperFileIOHost::ExecutePlatformGeneralCallback,
                     weak_factory_.GetWeakPtr(),
                     context->MakeReplyMessageContext())))
    return PP_ERROR_FAILED;

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileIOHost::OnHostMsgClose(
    ppapi::host::HostMessageContext* context) {
  if (file_ != base::kInvalidPlatformFileValue) {
    base::FileUtilProxy::Close(
        RenderThreadImpl::current()->GetFileThreadMessageLoopProxy().get(),
        file_,
        base::ResetAndReturn(&notify_close_file_callback_));
    file_ = base::kInvalidPlatformFileValue;
    quota_file_io_.reset();
  }
  return PP_OK;
}

int32_t PepperFileIOHost::OnHostMsgRequestOSFileHandle(
    ppapi::host::HostMessageContext* context) {
  if (open_flags_ != PP_FILEOPENFLAG_READ &&
      quota_policy_ != quota::kQuotaLimitTypeUnlimited)
    return PP_ERROR_FAILED;

  // Whitelist to make it privately accessible.
  if (!host()->permissions().HasPermission(ppapi::PERMISSION_PRIVATE) &&
      !GetContentClient()->renderer()->IsPluginAllowedToCallRequestOSFileHandle(
          renderer_ppapi_host_->GetContainerForInstance(pp_instance())))
    return PP_ERROR_NOACCESS;

  IPC::PlatformFileForTransit file =
      renderer_ppapi_host_->ShareHandleWithRemote(file_, false);
  if (file == IPC::InvalidPlatformFileForTransit())
    return PP_ERROR_FAILED;
  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  ppapi::proxy::SerializedHandle file_handle;
  file_handle.set_file_handle(file, open_flags_);
  reply_context.params.AppendHandle(file_handle);
  host()->SendReply(reply_context,
                    PpapiPluginMsg_FileIO_RequestOSFileHandleReply());
  return PP_OK_COMPLETIONPENDING;
}

void PepperFileIOHost::ExecutePlatformGeneralCallback(
    ppapi::host::ReplyMessageContext reply_context,
    base::PlatformFileError error_code) {
  reply_context.params.set_result(
      ppapi::PlatformFileErrorToPepperError(error_code));
  host()->SendReply(reply_context, PpapiPluginMsg_FileIO_GeneralReply());
  state_manager_.SetOperationFinished();
}

void PepperFileIOHost::ExecutePlatformOpenFileCallback(
    ppapi::host::ReplyMessageContext reply_context,
    base::PlatformFileError error_code,
    base::PassPlatformFile file) {
  int32_t pp_error = ppapi::PlatformFileErrorToPepperError(error_code);
  if (pp_error == PP_OK)
    state_manager_.SetOpenSucceed();

  DCHECK(file_ == base::kInvalidPlatformFileValue);
  file_ = file.ReleaseValue();

  DCHECK(!quota_file_io_.get());
  if (file_ != base::kInvalidPlatformFileValue) {
    if (ppapi::FileSystemTypeHasQuota(file_system_type_)) {
      quota_file_io_.reset(new QuotaFileIO(
          new QuotaFileIODelegate, file_, file_system_url_, file_system_type_));
    }

    IPC::PlatformFileForTransit file_for_transit =
        renderer_ppapi_host_->ShareHandleWithRemote(file_, false);
    if (!(file_for_transit == IPC::InvalidPlatformFileForTransit())) {
      // Send the file descriptor to the plugin process. This is used in the
      // plugin for any file operations that can be done there.
      int32_t flags_to_send = open_flags_;
      if (!host()->permissions().HasPermission(ppapi::PERMISSION_DEV)) {
        // IMPORTANT: Clear PP_FILEOPENFLAG_WRITE and PP_FILEOPENFLAG_APPEND so
        // the plugin can't write and so bypass our quota checks.
        flags_to_send =
            open_flags_ & ~(PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_APPEND);
      }
      ppapi::proxy::SerializedHandle file_handle;
      file_handle.set_file_handle(file_for_transit, flags_to_send);
      reply_context.params.AppendHandle(file_handle);
    }
  }

  reply_context.params.set_result(pp_error);
  host()->SendReply(reply_context, PpapiPluginMsg_FileIO_OpenReply());
  state_manager_.SetOperationFinished();
}

void PepperFileIOHost::ExecutePlatformOpenFileSystemURLCallback(
    ppapi::host::ReplyMessageContext reply_context,
    base::PlatformFileError error_code,
    base::PassPlatformFile file,
    quota::QuotaLimitType quota_policy,
    const PepperFileIOHost::NotifyCloseFileCallback& callback) {
  if (error_code == base::PLATFORM_FILE_OK)
    notify_close_file_callback_ = callback;
  quota_policy_ = quota_policy;
  ExecutePlatformOpenFileCallback(reply_context, error_code, file);
}

void PepperFileIOHost::ExecutePlatformWriteCallback(
    ppapi::host::ReplyMessageContext reply_context,
    base::PlatformFileError error_code,
    int bytes_written) {
  // On the plugin side, the callback expects a parameter with different meaning
  // depends on whether is negative or not. It is the result here. We translate
  // for the callback.
  int32_t pp_error = ppapi::PlatformFileErrorToPepperError(error_code);
  reply_context.params.set_result(ErrorOrByteNumber(pp_error, bytes_written));
  host()->SendReply(reply_context, PpapiPluginMsg_FileIO_GeneralReply());
  state_manager_.SetOperationFinished();
}

}  // namespace content
