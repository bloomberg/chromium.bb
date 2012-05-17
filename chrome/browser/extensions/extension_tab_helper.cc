// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tab_helper.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/webstore_inline_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/image/image.h"

using content::WebContents;

namespace {

const char kPermissionError[] = "permission_error";

}  // namespace

ExtensionTabHelper::ExtensionTabHelper(TabContentsWrapper* wrapper)
    : content::WebContentsObserver(wrapper->web_contents()),
      delegate_(NULL),
      extension_app_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          extension_function_dispatcher_(wrapper->profile(), this)),
      wrapper_(wrapper) {
}

ExtensionTabHelper::~ExtensionTabHelper() {
}

void ExtensionTabHelper::CopyStateFrom(const ExtensionTabHelper& source) {
  SetExtensionApp(source.extension_app());
  extension_app_icon_ = source.extension_app_icon_;
}

void ExtensionTabHelper::PageActionStateChanged() {
  web_contents()->NotifyNavigationStateChanged(
      content::INVALIDATE_TYPE_PAGE_ACTIONS);
}

void ExtensionTabHelper::GetApplicationInfo(int32 page_id) {
  Send(new ExtensionMsg_GetApplicationInfo(routing_id(), page_id));
}

void ExtensionTabHelper::SetExtensionApp(const Extension* extension) {
  DCHECK(!extension || extension->GetFullLaunchURL().is_valid());
  extension_app_ = extension;

  UpdateExtensionAppIcon(extension_app_);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CONTENTS_APPLICATION_EXTENSION_CHANGED,
      content::Source<ExtensionTabHelper>(this),
      content::NotificationService::NoDetails());
}

void ExtensionTabHelper::SetExtensionAppById(
    const std::string& extension_app_id) {
  const Extension* extension = GetExtension(extension_app_id);
  if (extension)
    SetExtensionApp(extension);
}

void ExtensionTabHelper::SetExtensionAppIconById(
    const std::string& extension_app_id) {
  const Extension* extension = GetExtension(extension_app_id);
  if (extension)
    UpdateExtensionAppIcon(extension);
}

SkBitmap* ExtensionTabHelper::GetExtensionAppIcon() {
  if (extension_app_icon_.empty())
    return NULL;

  return &extension_app_icon_;
}

void ExtensionTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_in_page)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  ExtensionService* service = profile->GetExtensionService();
  if (!service)
    return;

  for (ExtensionSet::const_iterator it = service->extensions()->begin();
       it != service->extensions()->end(); ++it) {
    ExtensionAction* browser_action = (*it)->browser_action();
    if (browser_action) {
      browser_action->ClearAllValuesForTab(
          wrapper_->restore_tab_helper()->session_id().id());
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED,
          content::Source<ExtensionAction>(browser_action),
          content::NotificationService::NoDetails());
    }

    ExtensionAction* page_action = (*it)->page_action();
    if (page_action) {
      page_action->ClearAllValuesForTab(
          wrapper_->restore_tab_helper()->session_id().id());
      PageActionStateChanged();
    }
  }
}

bool ExtensionTabHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionTabHelper, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_DidGetApplicationInfo,
                        OnDidGetApplicationInfo)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_InstallApplication,
                        OnInstallApplication)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_InlineWebstoreInstall,
                        OnInlineWebstoreInstall)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_GetAppNotifyChannel,
                        OnGetAppNotifyChannel)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_GetAppInstallState,
                        OnGetAppInstallState);
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionTabHelper::OnDidGetApplicationInfo(
    int32 page_id, const WebApplicationInfo& info) {
  web_app_info_ = info;

  if (delegate_)
    delegate_->OnDidGetApplicationInfo(wrapper_, page_id);
}

void ExtensionTabHelper::OnInstallApplication(const WebApplicationInfo& info) {
  if (delegate_)
    delegate_->OnInstallApplication(wrapper_, info);
}

void ExtensionTabHelper::OnInlineWebstoreInstall(
    int install_id,
    int return_route_id,
    const std::string& webstore_item_id,
    const GURL& requestor_url) {
  scoped_refptr<WebstoreInlineInstaller> installer(new WebstoreInlineInstaller(
      web_contents(),
      install_id,
      return_route_id,
      webstore_item_id,
      requestor_url,
      this));
  installer->BeginInstall();
}

void ExtensionTabHelper::OnGetAppNotifyChannel(
    const GURL& requestor_url,
    const std::string& client_id,
    int return_route_id,
    int callback_id) {

  // Check for permission first.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  ExtensionService* extension_service = profile->GetExtensionService();
  extensions::ProcessMap* process_map = extension_service->process_map();
  content::RenderProcessHost* process =
      tab_contents_wrapper()->web_contents()->GetRenderProcessHost();
  const Extension* extension =
      extension_service->GetInstalledApp(requestor_url);

  std::string error;
  if (!extension ||
      !extension->HasAPIPermission(
          ExtensionAPIPermission::kAppNotifications) ||
      !process_map->Contains(extension->id(), process->GetID()))
    error = kPermissionError;

  // Make sure the extension can cross to the main profile, if called from an
  // an incognito window.
  if (profile->IsOffTheRecord() &&
      !extension_service->CanCrossIncognito(extension))
    error = extension_misc::kAppNotificationsIncognitoError;

  if (!error.empty()) {
    Send(new ExtensionMsg_GetAppNotifyChannelResponse(
        return_route_id, "", error, callback_id));
    return;
  }

  AppNotifyChannelUI* ui = new AppNotifyChannelUIImpl(
      profile, tab_contents_wrapper(), extension->name(),
      AppNotifyChannelUI::NOTIFICATION_INFOBAR);

  scoped_refptr<AppNotifyChannelSetup> channel_setup(
      new AppNotifyChannelSetup(profile,
                                extension->id(),
                                client_id,
                                requestor_url,
                                return_route_id,
                                callback_id,
                                ui,
                                this->AsWeakPtr()));
  channel_setup->Start();
  // We'll get called back in AppNotifyChannelSetupComplete.
}

void ExtensionTabHelper::OnGetAppInstallState(const GURL& requestor_url,
                                              int return_route_id,
                                              int callback_id) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  ExtensionService* extension_service = profile->GetExtensionService();
  const ExtensionSet* extensions = extension_service->extensions();
  const ExtensionSet* disabled = extension_service->disabled_extensions();

  ExtensionURLInfo url(requestor_url);
  std::string state;
  if (extensions->GetHostedAppByURL(url))
    state = extension_misc::kAppStateInstalled;
  else if (disabled->GetHostedAppByURL(url))
    state = extension_misc::kAppStateDisabled;
  else
    state = extension_misc::kAppStateNotInstalled;

  Send(new ExtensionMsg_GetAppInstallStateResponse(
      return_route_id, state, callback_id));
}

void ExtensionTabHelper::AppNotifyChannelSetupComplete(
    const std::string& channel_id,
    const std::string& error,
    const AppNotifyChannelSetup* setup) {
  CHECK(setup);

  // If the setup was successful, record that fact in ExtensionService.
  if (!channel_id.empty() && error.empty()) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    ExtensionService* service = profile->GetExtensionService();
    if (service->GetExtensionById(setup->extension_id(), true))
      service->SetAppNotificationSetupDone(setup->extension_id(),
                                           setup->client_id());
  }

  Send(new ExtensionMsg_GetAppNotifyChannelResponse(
      setup->return_route_id(), channel_id, error, setup->callback_id()));
}

void ExtensionTabHelper::OnRequest(
    const ExtensionHostMsg_Request_Params& request) {
  extension_function_dispatcher_.Dispatch(request,
                                          web_contents()->GetRenderViewHost());
}

const Extension* ExtensionTabHelper::GetExtension(
    const std::string& extension_app_id) {
  if (extension_app_id.empty())
    return NULL;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  ExtensionService* extension_service = profile->GetExtensionService();
  if (!extension_service || !extension_service->is_ready())
    return NULL;

  const Extension* extension =
      extension_service->GetExtensionById(extension_app_id, false);
  return extension;
}

void ExtensionTabHelper::UpdateExtensionAppIcon(const Extension* extension) {
  extension_app_icon_.reset();

  if (extension) {
    extension_app_image_loader_.reset(new ImageLoadingTracker(this));
    extension_app_image_loader_->LoadImage(
        extension,
        extension->GetIconResource(ExtensionIconSet::EXTENSION_ICON_SMALLISH,
                                   ExtensionIconSet::MATCH_EXACTLY),
        gfx::Size(ExtensionIconSet::EXTENSION_ICON_SMALLISH,
                  ExtensionIconSet::EXTENSION_ICON_SMALLISH),
        ImageLoadingTracker::CACHE);
  } else {
    extension_app_image_loader_.reset(NULL);
  }
}

void ExtensionTabHelper::SetAppIcon(const SkBitmap& app_icon) {
  extension_app_icon_ = app_icon;
  web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TITLE);
}

void ExtensionTabHelper::OnImageLoaded(const gfx::Image& image,
                                       const std::string& extension_id,
                                       int index) {
  if (!image.IsEmpty()) {
    extension_app_icon_ = *image.ToSkBitmap();
    web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  }
}

ExtensionWindowController*
ExtensionTabHelper::GetExtensionWindowController() const  {
  content::WebContents* contents = web_contents();
  TabContentsIterator tab_iterator;
  for (; !tab_iterator.done(); ++tab_iterator) {
    if (contents == (*tab_iterator)->web_contents())
      return tab_iterator.browser()->extension_window_controller();
  }

  return NULL;
}

void ExtensionTabHelper::OnInlineInstallSuccess(int install_id,
                                                int return_route_id) {
  Send(new ExtensionMsg_InlineWebstoreInstallResponse(
      return_route_id, install_id, true, ""));
}

void ExtensionTabHelper::OnInlineInstallFailure(int install_id,
                                                int return_route_id,
                                                const std::string& error) {
  Send(new ExtensionMsg_InlineWebstoreInstallResponse(
      return_route_id, install_id, false, error));
}

WebContents* ExtensionTabHelper::GetAssociatedWebContents() const {
  return web_contents();
}
