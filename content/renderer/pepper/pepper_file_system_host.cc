// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_file_system_host.h"

#include "base/bind.h"
#include "base/callback.h"
#include "content/common/child_thread.h"
#include "content/common/fileapi/file_system_dispatcher.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/null_file_system_callback_dispatcher.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

namespace content {

namespace {

class PlatformCallbackAdaptor : public NullFileSystemCallbackDispatcher {
 public:
  explicit PlatformCallbackAdaptor(
      const base::WeakPtr<PepperFileSystemHost>& weak_host)
      : weak_host_(weak_host) {}

  virtual ~PlatformCallbackAdaptor() {}

  virtual void DidOpenFileSystem(const std::string& /* unused */,
                                 const GURL& root) OVERRIDE {
    if (weak_host_)
      weak_host_->OpenFileSystemReply(PP_OK, root);
  }

  virtual void DidFail(base::PlatformFileError platform_error) OVERRIDE {
    if (weak_host_) {
      weak_host_->OpenFileSystemReply(
          ppapi::PlatformFileErrorToPepperError(platform_error), GURL());
    }
  }

 private:
  base::WeakPtr<PepperFileSystemHost> weak_host_;
};

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
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileSystem_Open,
                                      OnHostMsgOpen)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

void PepperFileSystemHost::OpenFileSystemReply(int32_t pp_error,
                                               const GURL& root) {
  opened_ = (pp_error == PP_OK);
  root_url_ = root;
  reply_context_.params.set_result(pp_error);
  host()->SendReply(reply_context_,
                    PpapiPluginMsg_FileSystem_OpenReply());
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

  webkit::ppapi::PluginInstance* plugin_instance =
      renderer_ppapi_host_->GetPluginInstance(pp_instance());
  if (!plugin_instance)
    return PP_ERROR_FAILED;

  FileSystemDispatcher* file_system_dispatcher =
      ChildThread::current()->file_system_dispatcher();
  reply_context_ = context->MakeReplyMessageContext();
  if (!file_system_dispatcher->OpenFileSystem(
      GURL(plugin_instance->container()->element().document().url()).
          GetOrigin(),
      file_system_type, expected_size, true /* create */,
      new PlatformCallbackAdaptor(weak_factory_.GetWeakPtr()))) {
    return PP_ERROR_FAILED;
  }

  return PP_OK_COMPLETIONPENDING;
}

}  // namespace content
