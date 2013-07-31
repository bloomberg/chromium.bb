// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_file_system_host.h"

#include "base/bind.h"
#include "base/callback.h"
#include "content/child/child_thread.h"
#include "content/child/fileapi/file_system_dispatcher.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace content {

namespace {

bool LooksLikeAGuid(const std::string& fsid) {
  const size_t kExpectedFsIdSize = 32;
  if (fsid.size() != kExpectedFsIdSize)
    return false;
  for (std::string::const_iterator it = fsid.begin(); it != fsid.end(); ++it) {
    if (('A' <= *it && *it <= 'F') ||
        ('0' <= *it && *it <= '9'))
      continue;
    return false;
  }
  return true;
}

}  // namespace

PepperFileSystemHost::PepperFileSystemHost(RendererPpapiHost* host,
                                           PP_Instance instance,
                                           PP_Resource resource,
                                           PP_FileSystemType type)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      weak_factory_(this),
      type_(type),
      opened_(false),
      called_open_(false) {
}

PepperFileSystemHost::~PepperFileSystemHost() {
}

int32_t PepperFileSystemHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperFileSystemHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_FileSystem_Open,
        OnHostMsgOpen)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_FileSystem_InitIsolatedFileSystem,
        OnHostMsgInitIsolatedFileSystem)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

bool PepperFileSystemHost::IsFileSystemHost() {
  return true;
}

void PepperFileSystemHost::DidOpenFileSystem(
    const std::string& /* name_unused */,
    const GURL& root) {
  opened_ = true;
  root_url_ = root;
  reply_context_.params.set_result(PP_OK);
  host()->SendReply(reply_context_, PpapiPluginMsg_FileSystem_OpenReply());
  reply_context_ = ppapi::host::ReplyMessageContext();
}

void PepperFileSystemHost::DidFailOpenFileSystem(
    base::PlatformFileError error) {
  int32 pp_error = ppapi::PlatformFileErrorToPepperError(error);
  opened_ = (pp_error == PP_OK);
  reply_context_.params.set_result(pp_error);
  host()->SendReply(reply_context_, PpapiPluginMsg_FileSystem_OpenReply());
  reply_context_ = ppapi::host::ReplyMessageContext();
}

int32_t PepperFileSystemHost::OnHostMsgOpen(
    ppapi::host::HostMessageContext* context,
    int64_t expected_size) {
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

  PepperPluginInstance* plugin_instance =
      renderer_ppapi_host_->GetPluginInstance(pp_instance());
  if (!plugin_instance)
    return PP_ERROR_FAILED;

  FileSystemDispatcher* file_system_dispatcher =
      ChildThread::current()->file_system_dispatcher();
  reply_context_ = context->MakeReplyMessageContext();
  file_system_dispatcher->OpenFileSystem(
      GURL(plugin_instance->GetContainer()->element().document().url()).
          GetOrigin(),
      file_system_type, expected_size, true /* create */,
      base::Bind(&PepperFileSystemHost::DidOpenFileSystem,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&PepperFileSystemHost::DidFailOpenFileSystem,
                 weak_factory_.GetWeakPtr()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileSystemHost::OnHostMsgInitIsolatedFileSystem(
    ppapi::host::HostMessageContext* context,
    const std::string& fsid) {
  called_open_ = true;
  // Do a sanity check.
  if (!LooksLikeAGuid(fsid))
    return PP_ERROR_BADARGUMENT;
  RenderView* view =
      renderer_ppapi_host_->GetRenderViewForInstance(pp_instance());
  if (!view)
    return PP_ERROR_FAILED;
  const GURL& url = view->GetWebView()->mainFrame()->document().url();
  root_url_ = GURL(fileapi::GetIsolatedFileSystemRootURIString(
      url.GetOrigin(), fsid, "crxfs"));
  opened_ = true;
  return PP_OK;
}

}  // namespace content
