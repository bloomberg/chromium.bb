// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_file_chooser_host.h"

#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/render_view_impl.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_file_ref_proxy.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileChooserCompletion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileChooserParams.h"
#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"

namespace content {

class PepperFileChooserHost::CompletionHandler
    : public WebKit::WebFileChooserCompletion {
 public:
  CompletionHandler(const base::WeakPtr<PepperFileChooserHost>& host)
      : host_(host) {
  }

  virtual ~CompletionHandler() {}

  virtual void didChooseFile(
      const WebKit::WebVector<WebKit::WebString>& file_names) {
    if (host_) {
      std::vector<PepperFileChooserHost::ChosenFileInfo> files;
      for (size_t i = 0; i < file_names.size(); i++) {
        files.push_back(PepperFileChooserHost::ChosenFileInfo(
            file_names[i].utf8(), std::string()));
      }
      host_->StoreChosenFiles(files);
    }

    // It is the responsibility of this method to delete the instance.
    delete this;
  }
  virtual void didChooseFile(
      const WebKit::WebVector<SelectedFileInfo>& file_names) {
    if (host_) {
      std::vector<PepperFileChooserHost::ChosenFileInfo> files;
      for (size_t i = 0; i < file_names.size(); i++) {
        files.push_back(PepperFileChooserHost::ChosenFileInfo(
            file_names[i].path.utf8(),
            file_names[i].displayName.utf8()));
      }
      host_->StoreChosenFiles(files);
    }

    // It is the responsibility of this method to delete the instance.
    delete this;
  }

 private:
  base::WeakPtr<PepperFileChooserHost> host_;

  DISALLOW_COPY_AND_ASSIGN(CompletionHandler);
};

PepperFileChooserHost::ChosenFileInfo::ChosenFileInfo(
    const std::string& path,
    const std::string& display_name)
    : path(path),
      display_name(display_name) {
}


PepperFileChooserHost::PepperFileChooserHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      handler_(NULL) {
}

PepperFileChooserHost::~PepperFileChooserHost() {
}

int32_t PepperFileChooserHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperFileChooserHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileChooser_Show, OnShow)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

void PepperFileChooserHost::StoreChosenFiles(
    const std::vector<ChosenFileInfo>& files) {
  std::vector<ppapi::PPB_FileRef_CreateInfo> chosen_files;
  for (size_t i = 0; i < files.size(); i++) {
#if defined(OS_WIN)
    FilePath file_path(UTF8ToWide(files[i].path));
#else
    FilePath file_path(files[i].path);
#endif

    webkit::ppapi::PPB_FileRef_Impl* ref =
        webkit::ppapi::PPB_FileRef_Impl::CreateExternal(
        pp_instance(), file_path, files[i].display_name);
    ppapi::PPB_FileRef_CreateInfo create_info;
    ppapi::proxy::PPB_FileRef_Proxy::SerializeFileRef(ref->GetReference(),
                                                      &create_info);
    chosen_files.push_back(create_info);
  }

  reply_context_.params.set_result(
      (chosen_files.size() > 0) ? PP_OK : PP_ERROR_USERCANCEL);
  host()->SendReply(reply_context_,
                    PpapiPluginMsg_FileChooser_ShowReply(chosen_files));

  reply_context_ = ppapi::host::ReplyMessageContext();
  handler_ = NULL;  // Handler deletes itself.
}

int32_t PepperFileChooserHost::OnShow(
    ppapi::host::HostMessageContext* context,
    bool save_as,
    bool open_multiple,
    const std::string& suggested_file_name,
    const std::vector<std::string>& accept_mime_types) {
  if (handler_)
    return PP_ERROR_INPROGRESS;  // Already pending.

  if (!host()->permissions().HasPermission(
          ppapi::PERMISSION_BYPASS_USER_GESTURE) &&
       !renderer_ppapi_host_->HasUserGesture(pp_instance())) {
    return PP_ERROR_NO_USER_GESTURE;
  }

  WebKit::WebFileChooserParams params;
  if (save_as) {
    params.saveAs = true;
    params.initialValue = WebKit::WebString::fromUTF8(
        suggested_file_name.data(), suggested_file_name.size());
  } else {
    params.multiSelect = open_multiple;
  }
  std::vector<WebKit::WebString> mine_types(accept_mime_types.size());
  for (size_t i = 0; i < accept_mime_types.size(); i++) {
    mine_types[i] = WebKit::WebString::fromUTF8(
        accept_mime_types[i].data(), accept_mime_types[i].size());
  }
  params.acceptTypes = mine_types;
  params.directory = false;

  handler_ = new CompletionHandler(AsWeakPtr());
  RenderViewImpl* render_view = static_cast<RenderViewImpl*>(
      renderer_ppapi_host_->GetRenderViewForInstance(pp_instance()));
  if (!render_view || !render_view->runFileChooser(params, handler_)) {
    delete handler_;
    handler_ = NULL;
    return PP_ERROR_NOACCESS;
  }

  reply_context_ = context->MakeReplyMessageContext();
  return PP_OK_COMPLETIONPENDING;
}

}  // namespace content

