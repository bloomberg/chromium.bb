// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_install_manager.h"

#include <string>

#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/external_install_error.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"

namespace extensions {

ExternalInstallManager::ExternalInstallManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context), extension_registry_observer_(this) {
  DCHECK(browser_context_);
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
  registrar_.Add(
      this,
      chrome::NOTIFICATION_EXTENSION_REMOVED,
      content::Source<Profile>(Profile::FromBrowserContext(browser_context_)));
}

ExternalInstallManager::~ExternalInstallManager() {
}

void ExternalInstallManager::AddExternalInstallError(const Extension* extension,
                                                     bool is_new_profile) {
  if (HasExternalInstallError())
    return;  // Only have one external install error at a time.

  if (ManifestURL::UpdatesFromGallery(extension) && !is_new_profile) {
    error_.reset(new ExternalInstallError(browser_context_,
                                          extension->id(),
                                          ExternalInstallError::BUBBLE_ALERT,
                                          this));
  } else {
    error_.reset(new ExternalInstallError(browser_context_,
                                          extension->id(),
                                          ExternalInstallError::MENU_ALERT,
                                          this));
  }
}

void ExternalInstallManager::RemoveExternalInstallError() {
  if (error_.get()) {
    error_.reset();
    ExtensionSystem::Get(browser_context_)
        ->extension_service()
        ->UpdateExternalExtensionAlert();
  }
}

bool ExternalInstallManager::HasExternalInstallError() const {
  return error_.get() != NULL;
}

bool ExternalInstallManager::HasExternalInstallBubble() const {
  return error_.get() &&
         error_->alert_type() == ExternalInstallError::BUBBLE_ALERT;
}

void ExternalInstallManager::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  // The error is invalidated if the extension has been loaded or removed.
  if (error_.get() && extension->id() == error_->extension_id()) {
    // We treat loading as acknowledgement (since the user consciously chose to
    // re-enable the extension).
    error_->AcknowledgeExtension();
    RemoveExternalInstallError();
  }
}

void ExternalInstallManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_EXTENSION_REMOVED, type);
  // The error is invalidated if the extension has been loaded or removed.
  // It's a shame we have to use the notification system (instead of the
  // registry observer) for this, but the ExtensionUnloaded notification is
  // not sent out if the extension is disabled (which it is here).
  if (error_.get() &&
      content::Details<const Extension>(details).ptr()->id() ==
          error_->extension_id()) {
    RemoveExternalInstallError();
  }
}

}  // namespace extensions
