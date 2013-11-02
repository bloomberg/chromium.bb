// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_file_io_host.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_util_proxy.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/pepper/pepper_file_ref_host.h"
#include "content/browser/renderer_host/pepper/pepper_security_helper.h"
#include "content/browser/renderer_host/pepper/quota_file_io.h"
#include "content/common/fileapi/file_system_messages.h"
#include "content/common/sandbox_util.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "webkit/browser/fileapi/file_observers.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/task_runner_bound_observer_list.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace content {

using ppapi::FileIOStateManager;
using ppapi::PPTimeToTime;

namespace {

int32_t ErrorOrByteNumber(int32_t pp_error, int32_t byte_number) {
  // On the plugin side, some callbacks expect a parameter that means different
  // things depending on whether it is negative or not.  We translate for those
  // callbacks here.
  return pp_error == PP_OK ? byte_number : pp_error;
}

class QuotaFileIODelegate : public QuotaFileIO::Delegate {
 public:
  QuotaFileIODelegate(scoped_refptr<fileapi::FileSystemContext> context,
                      int render_process_id)
      : context_(context),
        weak_factory_(this) { }
  virtual ~QuotaFileIODelegate() {}

  virtual void QueryAvailableSpace(
      const GURL& origin,
      quota::StorageType type,
      const AvailableSpaceCallback& callback) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    quota::QuotaManagerProxy* quota_manager_proxy =
        context_->quota_manager_proxy();
    DCHECK(quota_manager_proxy);
    if (!quota_manager_proxy) {
      callback.Run(0);
      return;
    }
    quota::QuotaManager* qm = quota_manager_proxy->quota_manager();
    DCHECK(qm);
    if (!qm) {
      callback.Run(0);
      return;
    }
    qm->GetUsageAndQuotaForWebApps(
        origin,
        type,
        base::Bind(&QuotaFileIODelegate::GotUsageAndQuotaForWebApps,
            weak_factory_.GetWeakPtr(), callback));
  }

  void GotUsageAndQuotaForWebApps(const AvailableSpaceCallback& callback,
                                  quota::QuotaStatusCode code,
                                  int64 usage,
                                  int64 quota) {
    if (code == quota::kQuotaStatusOk)
      callback.Run(std::max(static_cast<int64>(0), quota - usage));
    else
      callback.Run(0);
  }

  virtual void WillUpdateFile(const GURL& file_path) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    fileapi::FileSystemURL url(context_->CrackURL(file_path));
    if (!url.is_valid())
      return;
    const fileapi::UpdateObserverList* observers =
        context_->GetUpdateObservers(url.type());
    if (!observers)
      return;
    observers->Notify(&fileapi::FileUpdateObserver::OnStartUpdate,
                      MakeTuple(url));
  }
  virtual void DidUpdateFile(const GURL& file_path, int64_t delta) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    fileapi::FileSystemURL url(context_->CrackURL(file_path));
    if (!url.is_valid())
      return;
    const fileapi::UpdateObserverList* observers =
        context_->GetUpdateObservers(url.type());
    if (!observers)
      return;
    observers->Notify(&fileapi::FileUpdateObserver::OnUpdate,
                      MakeTuple(url, delta));
    observers->Notify(&fileapi::FileUpdateObserver::OnEndUpdate,
                      MakeTuple(url));
  }
  virtual scoped_refptr<base::MessageLoopProxy>
      GetFileThreadMessageLoopProxy() OVERRIDE {
    return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE);
  }
 private:
  scoped_refptr<fileapi::FileSystemContext> context_;
  base::WeakPtrFactory<QuotaFileIODelegate> weak_factory_;
};

PepperFileIOHost::UIThreadStuff
GetUIThreadStuffForInternalFileSystems(int render_process_id) {
  PepperFileIOHost::UIThreadStuff stuff;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id);
  if (host) {
    stuff.resolved_render_process_id = base::GetProcId(host->GetHandle());
    StoragePartition* storage_partition = host->GetStoragePartition();
    if (storage_partition)
      stuff.file_system_context = storage_partition->GetFileSystemContext();
  }
  return stuff;
}

base::ProcessId GetResolvedRenderProcessId(int render_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id);
  if (!host)
    return base::kNullProcessId;
  return base::GetProcId(host->GetHandle());
}

bool GetPluginAllowedToCallRequestOSFileHandle(int render_process_id,
                                               const GURL& document_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ContentBrowserClient* client = GetContentClient()->browser();
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id);
  return client->IsPluginAllowedToCallRequestOSFileHandle(
      host->GetBrowserContext(), document_url);
}

}  // namespace

PepperFileIOHost::PepperFileIOHost(BrowserPpapiHostImpl* host,
                                   PP_Instance instance,
                                   PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      browser_ppapi_host_(host),
      render_process_host_(NULL),
      file_(base::kInvalidPlatformFileValue),
      file_system_type_(PP_FILESYSTEMTYPE_INVALID),
      quota_policy_(quota::kQuotaLimitTypeUnknown),
      open_flags_(0),
      weak_factory_(this) {
  int unused;
  if (!host->GetRenderViewIDsForInstance(instance,
                                         &render_process_id_,
                                         &unused)) {
    render_process_id_ = -1;
  }
  file_message_loop_ = BrowserThread::GetMessageLoopProxyForThread(
      BrowserThread::FILE);
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

PepperFileIOHost::UIThreadStuff::UIThreadStuff() {
  resolved_render_process_id = base::kNullProcessId;
}

PepperFileIOHost::UIThreadStuff::~UIThreadStuff() {
}

int32_t PepperFileIOHost::OnHostMsgOpen(
    ppapi::host::HostMessageContext* context,
    PP_Resource file_ref_resource,
    int32_t open_flags) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, false);
  if (rv != PP_OK)
    return rv;

  int platform_file_flags = 0;
  if (!ppapi::PepperFileOpenFlagsToPlatformFileFlags(open_flags,
                                                     &platform_file_flags))
    return PP_ERROR_BADARGUMENT;

  ppapi::host::ResourceHost* resource_host =
      host()->GetResourceHost(file_ref_resource);
  if (!resource_host || !resource_host->IsFileRefHost())
    return PP_ERROR_BADRESOURCE;
  PepperFileRefHost* file_ref_host =
      static_cast<PepperFileRefHost*>(resource_host);
  if (file_ref_host->GetFileSystemType() == PP_FILESYSTEMTYPE_INVALID)
    return PP_ERROR_FAILED;

  open_flags_ = open_flags;
  file_system_type_ = file_ref_host->GetFileSystemType();
  file_system_url_ = file_ref_host->GetFileSystemURL();

  if (file_system_type_ != PP_FILESYSTEMTYPE_EXTERNAL) {
    if (!file_system_url_.is_valid())
      return PP_ERROR_BADARGUMENT;
    if (!CanOpenFileSystemURLWithPepperFlags(open_flags,
                                             render_process_id_,
                                             file_system_url_))
      return PP_ERROR_NOACCESS;

    BrowserThread::PostTaskAndReplyWithResult(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GetUIThreadStuffForInternalFileSystems,
                   render_process_id_),
        base::Bind(&PepperFileIOHost::GotUIThreadStuffForInternalFileSystems,
                   weak_factory_.GetWeakPtr(),
                   context->MakeReplyMessageContext(),
                   platform_file_flags));
  } else {
    base::FilePath path = file_ref_host->GetExternalFilePath();
    if (!CanOpenWithPepperFlags(open_flags, render_process_id_, path))
      return PP_ERROR_NOACCESS;
    BrowserThread::PostTaskAndReplyWithResult(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&GetResolvedRenderProcessId, render_process_id_),
        base::Bind(&PepperFileIOHost::GotResolvedRenderProcessId,
                   weak_factory_.GetWeakPtr(),
                   context->MakeReplyMessageContext(),
                   path,
                   platform_file_flags));
  }
  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

void PepperFileIOHost::GotUIThreadStuffForInternalFileSystems(
    ppapi::host::ReplyMessageContext reply_context,
    int platform_file_flags,
    UIThreadStuff ui_thread_stuff) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  file_system_context_ = ui_thread_stuff.file_system_context;
  resolved_render_process_id_ = ui_thread_stuff.resolved_render_process_id;
  if (resolved_render_process_id_ == base::kNullProcessId ||
      !file_system_context_.get()) {
    reply_context.params.set_result(PP_ERROR_FAILED);
    host()->SendReply(reply_context, PpapiPluginMsg_FileIO_OpenReply());
    return;
  }

  if (!file_system_context_->GetFileSystemBackend(file_system_url_.type())) {
    reply_context.params.set_result(PP_ERROR_FAILED);
    host()->SendReply(reply_context, PpapiPluginMsg_FileIO_OpenReply());
    return;
  }
  quota_policy_ = quota::kQuotaLimitTypeUnknown;
  quota::QuotaManagerProxy* quota_manager_proxy =
      file_system_context_->quota_manager_proxy();
  CHECK(quota_manager_proxy);
  CHECK(quota_manager_proxy->quota_manager());
  if (quota_manager_proxy->quota_manager()->IsStorageUnlimited(
          file_system_url_.origin(),
          fileapi::FileSystemTypeToQuotaStorageType(file_system_url_.type())))
    quota_policy_ = quota::kQuotaLimitTypeUnlimited;
  else
    quota_policy_ = quota::kQuotaLimitTypeLimited;
  file_system_operation_runner_ =
      file_system_context_->CreateFileSystemOperationRunner();
  file_system_operation_runner_->OpenFile(
      file_system_url_,
      platform_file_flags,
      base::Bind(&PepperFileIOHost::DidOpenInternalFile,
                 weak_factory_.GetWeakPtr(),
                 reply_context));
}

void PepperFileIOHost::DidOpenInternalFile(
    ppapi::host::ReplyMessageContext reply_context,
    base::PlatformFileError result,
    base::PlatformFile file,
    const base::Closure& on_close_callback) {
  if (result == base::PLATFORM_FILE_OK)
    on_close_callback_ = on_close_callback;
  ExecutePlatformOpenFileCallback(
      reply_context, result, base::PassPlatformFile(&file), true);
}

void PepperFileIOHost::GotResolvedRenderProcessId(
    ppapi::host::ReplyMessageContext reply_context,
    base::FilePath path,
    int platform_file_flags,
    base::ProcessId resolved_render_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  resolved_render_process_id_ = resolved_render_process_id;
  base::FileUtilProxy::CreateOrOpen(
      file_message_loop_,
      path,
      platform_file_flags,
      base::Bind(&PepperFileIOHost::ExecutePlatformOpenFileCallback,
                 weak_factory_.GetWeakPtr(),
                 reply_context));
}

int32_t PepperFileIOHost::OnHostMsgTouch(
    ppapi::host::HostMessageContext* context,
    PP_Time last_access_time,
    PP_Time last_modified_time) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;

  base::FileUtilProxy::StatusCallback cb =
      base::Bind(&PepperFileIOHost::ExecutePlatformGeneralCallback,
                 weak_factory_.GetWeakPtr(),
                 context->MakeReplyMessageContext());

  if (file_system_type_ != PP_FILESYSTEMTYPE_EXTERNAL) {
    // We use FileSystemOperationRunner here instead of working on the
    // opened file because it may not have been opened with enough
    // permissions for this operation. See http://crbug.com/313426 for
    // details.
    file_system_operation_runner_->TouchFile(
        file_system_url_,
        PPTimeToTime(last_access_time),
        PPTimeToTime(last_modified_time),
        cb);
  } else {
    if (!base::FileUtilProxy::Touch(
            file_message_loop_,
            file_,
            PPTimeToTime(last_access_time),
            PPTimeToTime(last_modified_time),
            cb))
      return PP_ERROR_FAILED;
  }
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

  QuotaFileIO::WriteCallback cb =
      base::Bind(&PepperFileIOHost::ExecutePlatformWriteCallback,
                 weak_factory_.GetWeakPtr(),
                 context->MakeReplyMessageContext());

  if (quota_file_io_) {
    if (!quota_file_io_->Write(offset, buffer.c_str(), buffer.size(), cb))
      return PP_ERROR_FAILED;
  } else {
    if (!base::FileUtilProxy::Write(
            file_message_loop_,
            file_,
            offset,
            buffer.c_str(),
            buffer.size(),
            cb))
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

  base::FileUtilProxy::StatusCallback cb =
      base::Bind(&PepperFileIOHost::ExecutePlatformGeneralCallback,
                 weak_factory_.GetWeakPtr(),
                 context->MakeReplyMessageContext());

  // TODO(teravest): Use QuotaFileIO::SetLength here.
  // The previous implementation did not use it in the renderer, so I'll
  // do it in a follow-up change.
  if (file_system_type_ != PP_FILESYSTEMTYPE_EXTERNAL) {
    // We use FileSystemOperationRunner here instead of working on the
    // opened file because it may not have been opened with enough
    // permissions for this operation. See http://crbug.com/313426 for
    // details.
    file_system_operation_runner_->Truncate(file_system_url_, length, cb);
  } else {
    if (!base::FileUtilProxy::Truncate(file_message_loop_, file_, length, cb))
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
          file_message_loop_,
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
        file_message_loop_,
        file_,
        base::Bind(&PepperFileIOHost::DidCloseFile,
                   weak_factory_.GetWeakPtr()));
    file_ = base::kInvalidPlatformFileValue;
    quota_file_io_.reset();
  }
  return PP_OK;
}

void PepperFileIOHost::DidCloseFile(base::PlatformFileError error) {
  // Silently ignore if we fail to close the file.
  if (!on_close_callback_.is_null()) {
    on_close_callback_.Run();
    on_close_callback_.Reset();
  }
}

int32_t PepperFileIOHost::OnHostMsgRequestOSFileHandle(
    ppapi::host::HostMessageContext* context) {
  if (open_flags_ != PP_FILEOPENFLAG_READ &&
      quota_policy_ != quota::kQuotaLimitTypeUnlimited)
    return PP_ERROR_FAILED;

  GURL document_url =
      browser_ppapi_host_->GetDocumentURLForInstance(pp_instance());
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GetPluginAllowedToCallRequestOSFileHandle,
                 render_process_id_,
                 document_url),
      base::Bind(&PepperFileIOHost::GotPluginAllowedToCallRequestOSFileHandle,
                 weak_factory_.GetWeakPtr(),
                 context->MakeReplyMessageContext()));
  return PP_OK_COMPLETIONPENDING;
}

void PepperFileIOHost::GotPluginAllowedToCallRequestOSFileHandle(
    ppapi::host::ReplyMessageContext reply_context,
    bool plugin_allowed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!browser_ppapi_host_->external_plugin() ||
      host()->permissions().HasPermission(ppapi::PERMISSION_PRIVATE) ||
      plugin_allowed) {
    if (!AddFileToReplyContext(open_flags_, &reply_context))
      reply_context.params.set_result(PP_ERROR_FAILED);
  } else {
    reply_context.params.set_result(PP_ERROR_NOACCESS);
  }
  host()->SendReply(reply_context,
                    PpapiPluginMsg_FileIO_RequestOSFileHandleReply());
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
    base::PassPlatformFile file,
    bool unused_created) {
  int32_t pp_error = ppapi::PlatformFileErrorToPepperError(error_code);
  if (pp_error == PP_OK)
    state_manager_.SetOpenSucceed();

  DCHECK(file_ == base::kInvalidPlatformFileValue);
  file_ = file.ReleaseValue();

  DCHECK(!quota_file_io_.get());
  if (file_ != base::kInvalidPlatformFileValue) {
    if (file_system_type_ == PP_FILESYSTEMTYPE_LOCALTEMPORARY ||
        file_system_type_ == PP_FILESYSTEMTYPE_LOCALPERSISTENT) {
      quota_file_io_.reset(new QuotaFileIO(
          new QuotaFileIODelegate(file_system_context_, render_process_id_),
              file_, file_system_url_.ToGURL(), file_system_type_));
    }
    int32_t flags_to_send = open_flags_;
    if (!host()->permissions().HasPermission(ppapi::PERMISSION_DEV)) {
      // IMPORTANT: Clear PP_FILEOPENFLAG_WRITE and PP_FILEOPENFLAG_APPEND so
      // the plugin can't write and so bypass our quota checks.
      flags_to_send =
          open_flags_ & ~(PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_APPEND);
    }
    if (!AddFileToReplyContext(flags_to_send, &reply_context))
      pp_error = PP_ERROR_FAILED;
  }
  reply_context.params.set_result(pp_error);
  host()->SendReply(reply_context, PpapiPluginMsg_FileIO_OpenReply());
  state_manager_.SetOperationFinished();
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

bool PepperFileIOHost::AddFileToReplyContext(
    int32_t open_flags,
    ppapi::host::ReplyMessageContext* reply_context) const {
  base::ProcessId plugin_process_id;
  if (browser_ppapi_host_->in_process()) {
    plugin_process_id = resolved_render_process_id_;
  } else {
    plugin_process_id = base::GetProcId(
        browser_ppapi_host_->GetPluginProcessHandle());
  }
  IPC::PlatformFileForTransit transit_file = BrokerGetFileHandleForProcess(
      file_, plugin_process_id, false);
  if (transit_file == IPC::InvalidPlatformFileForTransit())
    return false;
  ppapi::proxy::SerializedHandle file_handle;
  file_handle.set_file_handle(transit_file, open_flags);
  reply_context->params.AppendHandle(file_handle);
  return true;
}

}  // namespace content
