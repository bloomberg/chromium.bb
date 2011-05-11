// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tab_helper.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper_delegate.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/common/notification_service.h"

ExtensionTabHelper::ExtensionTabHelper(TabContentsWrapper* wrapper)
    : TabContentsObserver(wrapper->tab_contents()),
      extension_app_(NULL),
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

  NotificationService::current()->Notify(
      NotificationType::TAB_CONTENTS_APPLICATION_EXTENSION_CHANGED,
      Source<ExtensionTabHelper>(this),
      NotificationService::NoDetails());
}

void ExtensionTabHelper::SetExtensionAppById(
    const std::string& extension_app_id) {
  if (extension_app_id.empty())
    return;

  ExtensionService* extension_service =
      tab_contents()->profile()->GetExtensionService();
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

void ExtensionTabHelper::DidNavigateMainFramePostCommit(
    const NavigationController::LoadCommittedDetails& details,
    const ViewHostMsg_FrameNavigate_Params& params) {
  if (details.is_in_page)
    return;

  ExtensionService* service = tab_contents()->profile()->GetExtensionService();
  if (!service)
    return;

  for (size_t i = 0; i < service->extensions()->size(); ++i) {
    ExtensionAction* browser_action =
        service->extensions()->at(i)->browser_action();
    if (browser_action) {
      browser_action->ClearAllValuesForTab(
          tab_contents()->controller().session_id().id());
      NotificationService::current()->Notify(
          NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
          Source<ExtensionAction>(browser_action),
          NotificationService::NoDetails());
    }

    ExtensionAction* page_action =
        service->extensions()->at(i)->page_action();
    if (page_action) {
      page_action->ClearAllValuesForTab(
          tab_contents()->controller().session_id().id());
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
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionTabHelper::OnDidGetApplicationInfo(
    int32 page_id, const WebApplicationInfo& info) {
  web_app_info_ = info;

  if (wrapper_->delegate())
    wrapper_->delegate()->OnDidGetApplicationInfo(wrapper_, page_id);
}

void ExtensionTabHelper::OnInstallApplication(const WebApplicationInfo& info) {
  if (wrapper_->delegate())
    wrapper_->delegate()->OnInstallApplication(wrapper_, info);
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
