// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_enable_flow.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

using extensions::Extension;

ExtensionEnableFlow::ExtensionEnableFlow(Profile* profile,
                                         const std::string& extension_id,
                                         ExtensionEnableFlowDelegate* delegate)
    : profile_(profile),
      extension_id_(extension_id),
      delegate_(delegate),
      parent_contents_(NULL),
      parent_window_(NULL) {
}

ExtensionEnableFlow::~ExtensionEnableFlow() {
}

void ExtensionEnableFlow::StartForWebContents(
    content::WebContents* parent_contents) {
  parent_contents_ = parent_contents;
  parent_window_ = NULL;
  Run();
}

void ExtensionEnableFlow::StartForNativeWindow(
    gfx::NativeWindow parent_window) {
  parent_contents_ = NULL;
  parent_window_ = parent_window;
  Run();
}

void ExtensionEnableFlow::StartForCurrentlyNonexistentWindow(
    base::Callback<gfx::NativeWindow(void)> window_getter) {
  window_getter_ = window_getter;
  Run();
}

void ExtensionEnableFlow::Run() {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  const Extension* extension = service->GetExtensionById(extension_id_, true);
  if (!extension) {
    extension = service->GetTerminatedExtension(extension_id_);
    // It's possible (though unlikely) the app could have been uninstalled since
    // the user clicked on it.
    if (!extension)
      return;
    // If the app was terminated, reload it first.
    service->ReloadExtension(extension_id_);

    // ReloadExtension reallocates the Extension object.
    extension = service->GetExtensionById(extension_id_, true);

    // |extension| could be NULL for asynchronous load, such as the case of
    // an unpacked extension. Wait for the load to continue the flow.
    if (!extension) {
      StartObserving();
      return;
    }
  }

  CheckPermissionAndMaybePromptUser();
}

void ExtensionEnableFlow::CheckPermissionAndMaybePromptUser() {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  const Extension* extension = service->GetExtensionById(extension_id_, true);
  if (!extension) {
    delegate_->ExtensionEnableFlowAborted(false);  // |delegate_| may delete us.
    return;
  }

  extensions::ExtensionPrefs* extension_prefs = service->extension_prefs();
  if (!extension_prefs->DidExtensionEscalatePermissions(extension_id_)) {
    // Enable the extension immediately if its privileges weren't escalated.
    // This is a no-op if the extension was previously terminated.
    service->EnableExtension(extension_id_);

    delegate_->ExtensionEnableFlowFinished();  // |delegate_| may delete us.
    return;
  }

  CreatePrompt();
  int disable_reasons = extension_prefs->GetDisableReasons(extension->id());
  if (disable_reasons & Extension::DISABLE_PERMISSIONS_CONSENT)
    prompt_->ConfirmDefaultInstallFirstRun(this, extension);
  else
    prompt_->ConfirmReEnable(this, extension);
}

void ExtensionEnableFlow::CreatePrompt() {
  if (!window_getter_.is_null())
    parent_window_ = window_getter_.Run();
  prompt_.reset(parent_contents_ ?
      new ExtensionInstallPrompt(parent_contents_) :
      new ExtensionInstallPrompt(profile_, parent_window_, this));
}

void ExtensionEnableFlow::StartObserving() {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOAD_ERROR,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_));
}

void ExtensionEnableFlow::StopObserving() {
  registrar_.RemoveAll();
}

void ExtensionEnableFlow::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      if (extension->id() == extension_id_) {
        StopObserving();
        CheckPermissionAndMaybePromptUser();
      }

      break;
    }
    case chrome::NOTIFICATION_EXTENSION_LOAD_ERROR: {
      StopObserving();
      delegate_->ExtensionEnableFlowAborted(false);
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      if (extension->id() == extension_id_) {
        StopObserving();
        delegate_->ExtensionEnableFlowAborted(false);
      }

      break;
    }
    default:
      NOTREACHED();
  }
}

void ExtensionEnableFlow::InstallUIProceed() {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();

  // The extension can be uninstalled in another window while the UI was
  // showing. Treat it as a cancellation and notify |delegate_|.
  const Extension* extension = service->GetExtensionById(extension_id_, true);
  if (!extension) {
    delegate_->ExtensionEnableFlowAborted(true);
    return;
  }

  service->GrantPermissionsAndEnableExtension(extension);
  delegate_->ExtensionEnableFlowFinished();  // |delegate_| may delete us.
}

void ExtensionEnableFlow::InstallUIAbort(bool user_initiated) {
  delegate_->ExtensionEnableFlowAborted(user_initiated);
  // |delegate_| may delete us.
}

content::WebContents* ExtensionEnableFlow::OpenURL(
    const content::OpenURLParams& params) {
  Browser* browser = chrome::FindOrCreateTabbedBrowser(
      profile_, chrome::GetActiveDesktop());
  return browser->OpenURL(params);
}
