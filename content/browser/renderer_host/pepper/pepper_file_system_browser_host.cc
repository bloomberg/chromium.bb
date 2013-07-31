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

// TODO(teravest): Move this function to be shared and public in fileapi.
bool LooksLikeAGuid(const std::string& fsid) {
  const size_t kExpectedFsIdSize = 32;
  if (fsid.size() != kExpectedFsIdSize)
    return false;
  for (std::string::const_iterator it = fsid.begin(); it != fsid.end(); ++it) {
    if (('A' <= *it && *it <= 'F') || ('0' <= *it && *it <= '9'))
      continue;
    return false;
  }
  return true;
}

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

}  // namespace

PepperFileSystemBrowserHost::PepperFileSystemBrowserHost(BrowserPpapiHost* host,
                                                         PP_Instance instance,
                                                         PP_Resource resource,
                                                         PP_FileSystemType type)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      browser_ppapi_host_(host),
      weak_factory_(this),
      type_(type),
      opened_(false),
      fs_context_(NULL),
      called_open_(false) {
}

PepperFileSystemBrowserHost::~PepperFileSystemBrowserHost() {
  if (fs_context_.get())
    fs_context_->operation_runner()->Shutdown();
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

void PepperFileSystemBrowserHost::GotFileSystemContext(
    ppapi::host::ReplyMessageContext reply_context,
    fileapi::FileSystemType file_system_type,
    scoped_refptr<fileapi::FileSystemContext> fs_context) {
  if (!fs_context.get()) {
    OpenFileSystemComplete(
        reply_context, base::PLATFORM_FILE_ERROR_FAILED, std::string(), GURL());
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

void PepperFileSystemBrowserHost::OpenFileSystemComplete(
    ppapi::host::ReplyMessageContext reply_context,
    base::PlatformFileError error,
    const std::string& /* unused */,
    const GURL& root) {
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
    const std::string& fsid) {
  called_open_ = true;
  // Do a sanity check.
  if (!LooksLikeAGuid(fsid))
    return PP_ERROR_BADARGUMENT;
  const GURL& url =
      browser_ppapi_host_->GetDocumentURLForInstance(pp_instance());
  root_url_ = GURL(fileapi::GetIsolatedFileSystemRootURIString(
      url.GetOrigin(), fsid, "crxfs"));
  opened_ = true;
  return PP_OK;
}

}  // namespace content
