// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper_delegate.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/navigation_details.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"

ExtensionTabHelper::ExtensionTabHelper(TabContentsWrapper* wrapper)
    : TabContentsObserver(wrapper->tab_contents()),
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
  tab_contents()->NotifyNavigationStateChanged(
      TabContents::INVALIDATE_PAGE_ACTIONS);
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
  if (extension_app_id.empty())
    return;

  Profile* profile =
      Profile::FromBrowserContext(tab_contents()->browser_context());
  ExtensionService* extension_service = profile->GetExtensionService();
  if (!extension_service || !extension_service->is_ready())
    return;

  const Extension* extension =
      extension_service->GetExtensionById(extension_app_id, false);
  if (extension)
    SetExtensionApp(extension);
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
      Profile::FromBrowserContext(tab_contents()->browser_context());
  ExtensionService* service = profile->GetExtensionService();
  if (!service)
    return;

  for (size_t i = 0; i < service->extensions()->size(); ++i) {
    ExtensionAction* browser_action =
        service->extensions()->at(i)->browser_action();
    if (browser_action) {
      browser_action->ClearAllValuesForTab(
          wrapper_->restore_tab_helper()->session_id().id());
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED,
          content::Source<ExtensionAction>(browser_action),
          content::NotificationService::NoDetails());
    }

    ExtensionAction* page_action =
        service->extensions()->at(i)->page_action();
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
    const std::string& webstore_item_id,
    const GURL& requestor_url) {
  scoped_refptr<WebstoreInlineInstaller> installer(new WebstoreInlineInstaller(
      tab_contents(), install_id, webstore_item_id, requestor_url, this));
  installer->BeginInstall();
}

void ExtensionTabHelper::OnGetAppNotifyChannel(
    const GURL& requestor_url,
    const std::string& client_id,
    int return_route_id,
    int callback_id) {

  // Check for permission first.
  Profile* profile =
      Profile::FromBrowserContext(tab_contents()->browser_context());
  ExtensionService* extension_service = profile->GetExtensionService();
  extensions::ProcessMap* process_map = extension_service->process_map();
  content::RenderProcessHost* process =
      tab_contents_wrapper()->render_view_host()->process();
  const Extension* extension =
      extension_service->GetInstalledApp(requestor_url);
  bool allowed =
      extension &&
      extension->HasAPIPermission(
          ExtensionAPIPermission::kExperimental) &&
      process_map->Contains(extension->id(), process->GetID());
  if (!allowed) {
    Send(new ExtensionMsg_GetAppNotifyChannelResponse(
        return_route_id, "", "permission_error", callback_id));
    return;
  }

  AppNotifyChannelUI* ui = new AppNotifyChannelUIImpl(
      GetBrowser(), tab_contents_wrapper(), extension->name());

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

void ExtensionTabHelper::AppNotifyChannelSetupComplete(
    const std::string& channel_id,
    const std::string& error,
    const AppNotifyChannelSetup* setup) {
  CHECK(setup);

  // If the setup was successful, record that fact in ExtensionService.
  if (!channel_id.empty() && error.empty()) {
    Profile* profile =
        Profile::FromBrowserContext(tab_contents()->browser_context());
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
                                          tab_contents()->render_view_host());
}

void ExtensionTabHelper::UpdateExtensionAppIcon(const Extension* extension) {
  extension_app_icon_.reset();

  if (extension) {
    extension_app_image_loader_.reset(new ImageLoadingTracker(this));
    extension_app_image_loader_->LoadImage(
        extension,
        extension->GetIconResource(Extension::EXTENSION_ICON_SMALLISH,
                                   ExtensionIconSet::MATCH_EXACTLY),
        gfx::Size(Extension::EXTENSION_ICON_SMALLISH,
                  Extension::EXTENSION_ICON_SMALLISH),
        ImageLoadingTracker::CACHE);
  } else {
    extension_app_image_loader_.reset(NULL);
  }
}

void ExtensionTabHelper::SetAppIcon(const SkBitmap& app_icon) {
  extension_app_icon_ = app_icon;
  tab_contents()->NotifyNavigationStateChanged(TabContents::INVALIDATE_TITLE);
}

void ExtensionTabHelper::OnImageLoaded(SkBitmap* image,
                                       const ExtensionResource& resource,
                                       int index) {
  if (image) {
    extension_app_icon_ = *image;
    tab_contents()->NotifyNavigationStateChanged(TabContents::INVALIDATE_TAB);
  }
}

Browser* ExtensionTabHelper::GetBrowser() {
  TabContents* contents = tab_contents();
  TabContentsIterator tab_iterator;
  for (; !tab_iterator.done(); ++tab_iterator) {
    if (contents == (*tab_iterator)->tab_contents())
      return tab_iterator.browser();
  }

  return NULL;
}

void ExtensionTabHelper::OnInlineInstallSuccess(int install_id) {
  Send(new ExtensionMsg_InlineWebstoreInstallResponse(
      routing_id(), install_id, true, ""));
}

void ExtensionTabHelper::OnInlineInstallFailure(int install_id,
                                                const std::string& error) {
  Send(new ExtensionMsg_InlineWebstoreInstallResponse(
      routing_id(), install_id, false, error));
}

TabContents* ExtensionTabHelper::GetAssociatedTabContents() const {
  return tab_contents();
}

gfx::NativeView ExtensionTabHelper::GetNativeViewOfHost() {
  RenderWidgetHostView* rwhv = tab_contents()->GetRenderWidgetHostView();
  return rwhv ? rwhv->GetNativeView() : NULL;
}
