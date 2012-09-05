// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/navigation_observer.h"

#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

using content::NavigationController;
using content::NavigationEntry;

namespace extensions {

NavigationObserver::NavigationObserver(Profile* profile) : profile_(profile) {
  RegisterForNotifications();
}

NavigationObserver::~NavigationObserver() {}

void NavigationObserver::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  if (type != content::NOTIFICATION_NAV_ENTRY_COMMITTED) {
    NOTREACHED();
    return;
  }

  NavigationController* controller =
      content::Source<NavigationController>(source).ptr();
  if (!profile_->IsSameProfile(
          Profile::FromBrowserContext(controller->GetBrowserContext())))
    return;

  PromptToEnableExtensionIfNecessary(controller);
}

void NavigationObserver::RegisterForNotifications() {
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 content::NotificationService::AllSources());
}

void NavigationObserver::PromptToEnableExtensionIfNecessary(
    NavigationController* nav_controller) {
  // Bail out if we're already running a prompt.
  if (!in_progress_prompt_extension_id_.empty())
    return;

  NavigationEntry* nav_entry = nav_controller->GetActiveEntry();
  if (!nav_entry)
    return;

  ExtensionService* extension_service = profile_->GetExtensionService();
  const Extension* extension = extension_service->disabled_extensions()->
      GetExtensionOrAppByURL(ExtensionURLInfo(nav_entry->GetURL()));
  if (!extension)
    return;

  // Try not to repeatedly prompt the user about the same extension.
  if (prompted_extensions_.find(extension->id()) != prompted_extensions_.end())
    return;
  prompted_extensions_.insert(extension->id());

  ExtensionPrefs* extension_prefs = extension_service->extension_prefs();
  if (extension_prefs->DidExtensionEscalatePermissions(extension->id())) {
    // Keep track of the extension id and nav controller we're prompting for.
    // These must be reset in InstallUIProceed and InstallUIAbort.
    in_progress_prompt_extension_id_ = extension->id();
    in_progress_prompt_navigation_controller_ = nav_controller;

    extension_install_prompt_.reset(
        ExtensionInstallUI::CreateInstallPromptWithWebContents(
            nav_controller->GetWebContents()));
    extension_install_prompt_->ConfirmReEnable(this, extension);
  }
}

void NavigationObserver::InstallUIProceed() {
  ExtensionService* extension_service = profile_->GetExtensionService();
  const Extension* extension = extension_service->GetExtensionById(
      in_progress_prompt_extension_id_, true);
  NavigationController* nav_controller =
      in_progress_prompt_navigation_controller_;
  CHECK(extension);
  CHECK(nav_controller);

  in_progress_prompt_extension_id_ = "";
  in_progress_prompt_navigation_controller_ = NULL;
  bool record_oauth2_grant = extension_install_prompt_->record_oauth2_grant();
  extension_install_prompt_.reset();

  // Grant permissions, re-enable the extension, and then reload the tab.
  extension_service->GrantPermissionsAndEnableExtension(
      extension, record_oauth2_grant);
  nav_controller->Reload(true);
}

void NavigationObserver::InstallUIAbort(bool user_initiated) {
  ExtensionService* extension_service = profile_->GetExtensionService();
  const Extension* extension = extension_service->GetExtensionById(
      in_progress_prompt_extension_id_, true);

  in_progress_prompt_extension_id_ = "";
  in_progress_prompt_navigation_controller_ = NULL;
  extension_install_prompt_.reset();

  std::string histogram_name = user_initiated ?
      "Extensions.Permissions_ReEnableCancel" :
      "Extensions.Permissions_ReEnableAbort";
  ExtensionService::RecordPermissionMessagesHistogram(
      extension, histogram_name.c_str());
}

}  // namespace extensions
