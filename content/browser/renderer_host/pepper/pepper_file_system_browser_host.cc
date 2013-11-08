// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_file_system_browser_host.h"

#include "base/bind.h"
#include "base/callback.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace content {

namespace {

scoped_refptr<fileapi::FileSystemContext>
GetFileSystemContextFromRenderId(int render_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(render_process_id);
  if (!render_process_host)
    return NULL;
  StoragePartition* storage_partition =
      render_process_host->GetStoragePartition();
  if (!storage_partition)
    return NULL;
  return storage_partition->GetFileSystemContext();
}

// TODO(nhiroki): Move this function somewhere else to be shared.
std::string IsolatedFileSystemTypeToRootName(
    PP_IsolatedFileSystemType_Private type) {
  switch (type) {
    case PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_INVALID:
      break;
    case PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_CRX:
      return "crxfs";
  }
  NOTREACHED() << type;
  return std::string();
}

}  // namespace

PepperFileSystemBrowserHost::PepperFileSystemBrowserHost(BrowserPpapiHost* host,
                                                         PP_Instance instance,
                                                         PP_Resource resource,
                                                         PP_FileSystemType type)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      browser_ppapi_host_(host),
      type_(type),
      opened_(false),
      fs_context_(NULL),
      called_open_(false),
      weak_factory_(this) {
}

void PepperFileSystemBrowserHost::OpenExisting(const GURL& root_url,
                                               const base::Closure& callback) {
  root_url_ = root_url;
  int render_process_id = 0;
  int unused;
  if (!browser_ppapi_host_->GetRenderViewIDsForInstance(
           pp_instance(), &render_process_id, &unused)) {
    NOTREACHED();
  }
  called_open_ = true;
  // Get the file system context asynchronously, and then complete the Open
  // operation by calling |callback|.
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GetFileSystemContextFromRenderId, render_process_id),
      base::Bind(&PepperFileSystemBrowserHost::OpenExistingWithContext,
                 weak_factory_.GetWeakPtr(), callback));
}

PepperFileSystemBrowserHost::~PepperFileSystemBrowserHost() {
  // TODO(teravest): Create a FileSystemOperationRunner
  // per-PepperFileSystemBrowserHost, force users of this FileSystem to use it,
  // and call Shutdown() on it here.
}

int32_t PepperFileSystemBrowserHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperFileSystemBrowserHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_FileSystem_Open,
        OnHostMsgOpen)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_FileSystem_InitIsolatedFileSystem,
        OnHostMsgInitIsolatedFileSystem)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

bool PepperFileSystemBrowserHost::IsFileSystemHost() {
  return true;
}

int32_t PepperFileSystemBrowserHost::OnHostMsgOpen(
    ppapi::host::HostMessageContext* context,
    int64_t /* unused */) {
  // TODO(raymes): The file system size is now unused by FileSystemDispatcher.
  // Figure out why. Why is the file system size signed?

  // Not allow multiple opens.
  if (called_open_)
    return PP_ERROR_INPROGRESS;
  called_open_ = true;

  fileapi::FileSystemType file_system_type;
  switch (type_) {
    case PP_FILESYSTEMTYPE_LOCALTEMPORARY:
      file_system_type = fileapi::kFileSystemTypeTemporary;
      break;
    case PP_FILESYSTEMTYPE_LOCALPERSISTENT:
      file_system_type = fileapi::kFileSystemTypePersistent;
      break;
    case PP_FILESYSTEMTYPE_EXTERNAL:
      file_system_type = fileapi::kFileSystemTypeExternal;
      break;
    default:
      return PP_ERROR_FAILED;
  }

  int render_process_id = 0;
  int unused;
  if (!browser_ppapi_host_->GetRenderViewIDsForInstance(pp_instance(),
                                                        &render_process_id,
                                                        &unused)) {
      return PP_ERROR_FAILED;
  }
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GetFileSystemContextFromRenderId, render_process_id),
      base::Bind(&PepperFileSystemBrowserHost::GotFileSystemContext,
                 weak_factory_.GetWeakPtr(),
                 context->MakeReplyMessageContext(),
                 file_system_type));
  return PP_OK_COMPLETIONPENDING;
}

void PepperFileSystemBrowserHost::OpenExistingWithContext(
    const base::Closure& callback,
    scoped_refptr<fileapi::FileSystemContext> fs_context) {
  if (fs_context.get()) {
    opened_ = true;
  } else {
    // If there is no file system context, we log a warning and continue with an
    // invalid resource (which will produce errors when used), since we have no
    // way to communicate the error to the caller.
    LOG(WARNING) << "Could not retrieve file system context.";
  }
  fs_context_ = fs_context;
  callback.Run();
}

void PepperFileSystemBrowserHost::GotFileSystemContext(
    ppapi::host::ReplyMessageContext reply_context,
    fileapi::FileSystemType file_system_type,
    scoped_refptr<fileapi::FileSystemContext> fs_context) {
  if (!fs_context.get()) {
    OpenFileSystemComplete(
        reply_context, GURL(), std::string(), base::PLATFORM_FILE_ERROR_FAILED);
    return;
  }
  GURL origin = browser_ppapi_host_->GetDocumentURLForInstance(
      pp_instance()).GetOrigin();
  fs_context->OpenFileSystem(origin, file_system_type,
      fileapi::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      base::Bind(&PepperFileSystemBrowserHost::OpenFileSystemComplete,
                 weak_factory_.GetWeakPtr(),
                 reply_context));
  fs_context_ = fs_context;
}

void PepperFileSystemBrowserHost::GotIsolatedFileSystemContext(
    ppapi::host::ReplyMessageContext reply_context,
    scoped_refptr<fileapi::FileSystemContext> fs_context) {
  fs_context_ = fs_context;
  if (fs_context.get())
    reply_context.params.set_result(PP_OK);
  else
    reply_context.params.set_result(PP_ERROR_FAILED);
  host()->SendReply(reply_context,
                    PpapiPluginMsg_FileSystem_InitIsolatedFileSystemReply());
}

void PepperFileSystemBrowserHost::OpenFileSystemComplete(
    ppapi::host::ReplyMessageContext reply_context,
    const GURL& root,
    const std::string& /* unused */,
    base::PlatformFileError error) {
  int32 pp_error = ppapi::PlatformFileErrorToPepperError(error);
  if (pp_error == PP_OK) {
    opened_ = true;
    root_url_ = root;
  }
  reply_context.params.set_result(pp_error);
  host()->SendReply(reply_context, PpapiPluginMsg_FileSystem_OpenReply());
}

int32_t PepperFileSystemBrowserHost::OnHostMsgInitIsolatedFileSystem(
    ppapi::host::HostMessageContext* context,
    const std::string& fsid,
    PP_IsolatedFileSystemType_Private type) {
  // Do not allow multiple opens.
  if (called_open_)
    return PP_ERROR_INPROGRESS;
  called_open_ = true;

  // Do a sanity check.
  if (!fileapi::ValidateIsolatedFileSystemId(fsid))
    return PP_ERROR_BADARGUMENT;

  const GURL& url =
      browser_ppapi_host_->GetDocumentURLForInstance(pp_instance());
  const std::string root_name = IsolatedFileSystemTypeToRootName(type);
  if (root_name.empty())
    return PP_ERROR_BADARGUMENT;

  root_url_ = GURL(fileapi::GetIsolatedFileSystemRootURIString(
      url.GetOrigin(), fsid, root_name));
  opened_ = true;

  int render_process_id = 0;
  int unused;
  if (!browser_ppapi_host_->GetRenderViewIDsForInstance(pp_instance(),
                                                        &render_process_id,
                                                        &unused)) {
    return PP_ERROR_FAILED;
  }
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GetFileSystemContextFromRenderId, render_process_id),
      base::Bind(&PepperFileSystemBrowserHost::GotIsolatedFileSystemContext,
                 weak_factory_.GetWeakPtr(),
                 context->MakeReplyMessageContext()));
  return PP_OK_COMPLETIONPENDING;
}

}  // namespace content
