// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/pepper_extensions_common_message_filter.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace chrome {

class PepperExtensionsCommonMessageFilter::DispatcherOwner
    : public content::WebContentsObserver,
      public extensions::ExtensionFunctionDispatcher::Delegate {
 public:
  DispatcherOwner(PepperExtensionsCommonMessageFilter* message_filter,
                  Profile* profile,
                  content::RenderFrameHost* frame_host,
                  content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        render_frame_host_(frame_host),
        message_filter_(message_filter),
        dispatcher_(profile, this) {}

  virtual ~DispatcherOwner() { message_filter_->DetachDispatcherOwner(); }

  // content::WebContentsObserver implementation.
  virtual void RenderFrameDeleted(content::RenderFrameHost* render_frame_host)
      OVERRIDE {
    if (render_frame_host == render_frame_host_)
      delete this;
  }

  // extensions::ExtensionFunctionDispatcher::Delegate implementation.
  virtual extensions::WindowController* GetExtensionWindowController() const
      OVERRIDE {
    NOTREACHED();
    return NULL;
  }

  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE {
    NOTREACHED();
    return NULL;
  }

  extensions::ExtensionFunctionDispatcher* dispatcher() { return &dispatcher_; }
  content::RenderFrameHost* render_frame_host() { return render_frame_host_; }

 private:
  content::RenderFrameHost* render_frame_host_;
  scoped_refptr<PepperExtensionsCommonMessageFilter> message_filter_;
  extensions::ExtensionFunctionDispatcher dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(DispatcherOwner);
};

// static
PepperExtensionsCommonMessageFilter*
PepperExtensionsCommonMessageFilter::Create(content::BrowserPpapiHost* host,
                                            PP_Instance instance) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  int render_process_id = 0;
  int render_frame_id = 0;
  if (!host->GetRenderFrameIDsForInstance(
          instance, &render_process_id, &render_frame_id)) {
    return NULL;
  }

  base::FilePath profile_directory = host->GetProfileDataDirectory();
  GURL document_url = host->GetDocumentURLForInstance(instance);

  return new PepperExtensionsCommonMessageFilter(
      render_process_id, render_frame_id, profile_directory, document_url);
}

PepperExtensionsCommonMessageFilter::PepperExtensionsCommonMessageFilter(
    int render_process_id,
    int render_frame_id,
    const base::FilePath& profile_directory,
    const GURL& document_url)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      profile_directory_(profile_directory),
      document_url_(document_url),
      dispatcher_owner_(NULL),
      dispatcher_owner_initialized_(false) {}

PepperExtensionsCommonMessageFilter::~PepperExtensionsCommonMessageFilter() {}

scoped_refptr<base::TaskRunner>
PepperExtensionsCommonMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& msg) {
  return content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::UI);
}

int32_t PepperExtensionsCommonMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperExtensionsCommonMessageFilter, msg)
  PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_ExtensionsCommon_Post, OnPost)
  PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_ExtensionsCommon_Call, OnCall)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperExtensionsCommonMessageFilter::OnPost(
    ppapi::host::HostMessageContext* context,
    const std::string& request_name,
    base::ListValue& args) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (!EnsureDispatcherOwnerInitialized())
    return PP_ERROR_FAILED;

  ExtensionHostMsg_Request_Params params;
  PopulateParams(request_name, &args, false, &params);

  dispatcher_owner_->dispatcher()->Dispatch(
      params, dispatcher_owner_->render_frame_host()->GetRenderViewHost());
  // There will be no callback so return PP_OK.
  return PP_OK;
}

int32_t PepperExtensionsCommonMessageFilter::OnCall(
    ppapi::host::HostMessageContext* context,
    const std::string& request_name,
    base::ListValue& args) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (!EnsureDispatcherOwnerInitialized())
    return PP_ERROR_FAILED;

  ExtensionHostMsg_Request_Params params;
  PopulateParams(request_name, &args, true, &params);

  dispatcher_owner_->dispatcher()->DispatchWithCallback(
      params,
      dispatcher_owner_->render_frame_host(),
      base::Bind(&PepperExtensionsCommonMessageFilter::OnCallCompleted,
                 this,
                 context->MakeReplyMessageContext()));
  return PP_OK_COMPLETIONPENDING;
}

bool PepperExtensionsCommonMessageFilter::EnsureDispatcherOwnerInitialized() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (dispatcher_owner_initialized_)
    return (!!dispatcher_owner_);
  dispatcher_owner_initialized_ = true;

  DCHECK(!dispatcher_owner_);
  content::RenderFrameHost* frame_host =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame_host);

  if (!document_url_.SchemeIs(extensions::kExtensionScheme))
    return false;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return false;
  Profile* profile = profile_manager->GetProfile(profile_directory_);

  // It will be automatically destroyed when |view_host| goes away.
  dispatcher_owner_ =
      new DispatcherOwner(this, profile, frame_host, web_contents);
  return true;
}

void PepperExtensionsCommonMessageFilter::DetachDispatcherOwner() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  dispatcher_owner_ = NULL;
}

void PepperExtensionsCommonMessageFilter::PopulateParams(
    const std::string& request_name,
    base::ListValue* args,
    bool has_callback,
    ExtensionHostMsg_Request_Params* params) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  params->name = request_name;
  params->arguments.Swap(args);

  params->extension_id = document_url_.host();
  params->source_url = document_url_;

  // We don't need an ID to map a response to the corresponding request.
  params->request_id = 0;
  params->has_callback = has_callback;
  params->user_gesture = false;
}

void PepperExtensionsCommonMessageFilter::OnCallCompleted(
    ppapi::host::ReplyMessageContext reply_context,
    ExtensionFunction::ResponseType type,
    const base::ListValue& results,
    const std::string& /* error */) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (type == ExtensionFunction::BAD_MESSAGE) {
    // The input arguments were not validated at the plugin side, so don't kill
    // the plugin process when we see a message with invalid arguments.
    // TODO(yzshen): It is nicer to also do the validation at the plugin side.
    type = ExtensionFunction::FAILED;
  }

  reply_context.params.set_result(
      type == ExtensionFunction::SUCCEEDED ? PP_OK : PP_ERROR_FAILED);
  SendReply(reply_context, PpapiPluginMsg_ExtensionsCommon_CallReply(results));
}

}  // namespace chrome
