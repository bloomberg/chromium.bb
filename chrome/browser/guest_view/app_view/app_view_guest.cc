// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/app_view/app_view_guest.h"

#include "base/command_line.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/guest_view/app_view/app_view_constants.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/common/chrome_switches.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/renderer_preferences.h"
#include "extensions/browser/api/app_runtime/app_runtime_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension_messages.h"
#include "ipc/ipc_message_macros.h"

namespace app_runtime = extensions::core_api::app_runtime;

using content::RenderFrameHost;
using content::WebContents;
using extensions::ExtensionHost;

namespace extensions {

namespace {

struct ResponseInfo {
  scoped_refptr<const Extension> guest_extension;
  base::WeakPtr<AppViewGuest> app_view_guest;
  GuestViewBase::WebContentsCreatedCallback callback;

  ResponseInfo(const Extension* guest_extension,
               const base::WeakPtr<AppViewGuest>& app_view_guest,
               const GuestViewBase::WebContentsCreatedCallback& callback)
      : guest_extension(guest_extension),
        app_view_guest(app_view_guest),
        callback(callback) {}

  ~ResponseInfo() {}
};

typedef std::map<int, linked_ptr<ResponseInfo> > PendingResponseMap;
static base::LazyInstance<PendingResponseMap> pending_response_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static.
const char AppViewGuest::Type[] = "appview";

// static.
bool AppViewGuest::CompletePendingRequest(
    content::BrowserContext* browser_context,
    const GURL& url,
    int guest_instance_id,
    const std::string& guest_extension_id) {
  PendingResponseMap* response_map = pending_response_map.Pointer();
  PendingResponseMap::iterator it = response_map->find(guest_instance_id);
  if (it == response_map->end()) {
    // TODO(fsamuel): An app is sending invalid responses. We should probably
    // kill it.
    return false;
  }

  linked_ptr<ResponseInfo> response_info = it->second;
  if (!response_info->app_view_guest ||
      (response_info->guest_extension->id() != guest_extension_id)) {
    // TODO(fsamuel): An app is trying to respond to an <appview> that didn't
    // initiate communication with it. We should kill the app here.
    return false;
  }

  response_info->app_view_guest->
      CompleteCreateWebContents(url,
                                response_info->guest_extension,
                                response_info->callback);

  response_map->erase(guest_instance_id);
  return true;
}

// static
GuestViewBase* AppViewGuest::Create(content::BrowserContext* browser_context,
                                    int guest_instance_id) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableAppView)) {
    return NULL;
  }
  return new AppViewGuest(browser_context, guest_instance_id);
}

AppViewGuest::AppViewGuest(content::BrowserContext* browser_context,
                           int guest_instance_id)
    : GuestView<AppViewGuest>(browser_context, guest_instance_id),
      weak_ptr_factory_(this) {
}

AppViewGuest::~AppViewGuest() {
}

WindowController* AppViewGuest::GetExtensionWindowController() const {
  return NULL;
}

content::WebContents* AppViewGuest::GetAssociatedWebContents() const {
  return guest_web_contents();
}

bool AppViewGuest::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AppViewGuest, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool AppViewGuest::HandleContextMenu(const content::ContextMenuParams& params) {
  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(guest_web_contents());
  DCHECK(menu_delegate);

  scoped_ptr<RenderViewContextMenu> menu =
      menu_delegate->BuildMenu(guest_web_contents(), params);
  menu_delegate->ShowMenu(menu.Pass());
  return true;
}

const char* AppViewGuest::GetAPINamespace() {
  return appview::kEmbedderAPINamespace;
}

void AppViewGuest::CreateWebContents(
    const std::string& embedder_extension_id,
    int embedder_render_process_id,
    const base::DictionaryValue& create_params,
    const WebContentsCreatedCallback& callback) {
  std::string app_id;
  if (!create_params.GetString(appview::kAppID, &app_id)) {
    callback.Run(NULL);
    return;
  }

  const base::DictionaryValue* data = NULL;
  if (!create_params.GetDictionary(appview::kData, &data)) {
    callback.Run(NULL);
    return;
  }

  ExtensionService* service =
      ExtensionSystem::Get(browser_context())->extension_service();
  const Extension* guest_extension = service->GetExtensionById(app_id, false);
  const Extension* embedder_extension =
      service->GetExtensionById(embedder_extension_id, false);

  if (!guest_extension || !guest_extension->is_platform_app() ||
      !embedder_extension | !embedder_extension->is_platform_app()) {
    callback.Run(NULL);
    return;
  }

  pending_response_map.Get().insert(
      std::make_pair(GetGuestInstanceID(),
                     make_linked_ptr(new ResponseInfo(
                                        guest_extension,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        callback))));

  LazyBackgroundTaskQueue* queue =
      ExtensionSystem::Get(browser_context())->lazy_background_task_queue();
  if (queue->ShouldEnqueueTask(browser_context(), guest_extension)) {
    queue->AddPendingTask(browser_context(),
                          guest_extension->id(),
                          base::Bind(
                              &AppViewGuest::LaunchAppAndFireEvent,
                              weak_ptr_factory_.GetWeakPtr(),
                              base::Passed(make_scoped_ptr(data->DeepCopy())),
                              callback));
    return;
  }

  ProcessManager* process_manager =
      ExtensionSystem::Get(browser_context())->process_manager();
  ExtensionHost* host =
      process_manager->GetBackgroundHostForExtension(guest_extension->id());
  DCHECK(host);
  LaunchAppAndFireEvent(make_scoped_ptr(data->DeepCopy()), callback, host);
}

void AppViewGuest::DidAttachToEmbedder() {
  // This is called after the guest process has been attached to a host
  // element. This means that the host element knows how to route input
  // events to the guest, and the guest knows how to get frames to the
  // embedder.
  guest_web_contents()->GetController().LoadURL(
      url_, content::Referrer(), content::PAGE_TRANSITION_LINK, std::string());
}

void AppViewGuest::DidInitialize() {
  extension_function_dispatcher_.reset(
      new ExtensionFunctionDispatcher(browser_context(), this));
}

void AppViewGuest::OnRequest(const ExtensionHostMsg_Request_Params& params) {
  extension_function_dispatcher_->Dispatch(
      params, guest_web_contents()->GetRenderViewHost());
}

void AppViewGuest::CompleteCreateWebContents(
    const GURL& url,
    const Extension* guest_extension,
    const WebContentsCreatedCallback& callback) {
  if (!url.is_valid()) {
    callback.Run(NULL);
    return;
  }
  url_ = url;
  guest_extension_id_ = guest_extension->id();

  WebContents::CreateParams params(
      browser_context(),
      content::SiteInstance::CreateForURL(browser_context(),
                                          guest_extension->url()));
  params.guest_delegate = this;
  callback.Run(WebContents::Create(params));
}

void AppViewGuest::LaunchAppAndFireEvent(
    scoped_ptr<base::DictionaryValue> data,
    const WebContentsCreatedCallback& callback,
    ExtensionHost* extension_host) {
  ExtensionSystem* system = ExtensionSystem::Get(browser_context());
  bool has_event_listener = system->event_router()->ExtensionHasEventListener(
      extension_host->extension()->id(),
      app_runtime::OnEmbedRequested::kEventName);
  if (!has_event_listener) {
    callback.Run(NULL);
    return;
  }

  scoped_ptr<base::DictionaryValue> embed_request(new base::DictionaryValue());
  embed_request->SetInteger(appview::kGuestInstanceID, GetGuestInstanceID());
  embed_request->SetString(appview::kEmbedderID, embedder_extension_id());
  embed_request->Set(appview::kData, data.release());
  AppRuntimeEventRouter::DispatchOnEmbedRequestedEvent(
      browser_context(), embed_request.Pass(), extension_host->extension());
}

}  // namespace extensions
