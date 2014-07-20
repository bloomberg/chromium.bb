// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_install_manager.h"

#include <string>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/external_install_error.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"

namespace extensions {

namespace {

// Histogram values for logging events related to externally installed
// extensions.
enum ExternalExtensionEvent {
  EXTERNAL_EXTENSION_INSTALLED = 0,
  EXTERNAL_EXTENSION_IGNORED,
  EXTERNAL_EXTENSION_REENABLED,
  EXTERNAL_EXTENSION_UNINSTALLED,
  EXTERNAL_EXTENSION_BUCKET_BOUNDARY,
};

#if defined(ENABLE_EXTENSIONS)
// Prompt the user this many times before considering an extension acknowledged.
const int kMaxExtensionAcknowledgePromptCount = 3;
#endif

void LogExternalExtensionEvent(const Extension* extension,
                               ExternalExtensionEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Extensions.ExternalExtensionEvent",
                            event,
                            EXTERNAL_EXTENSION_BUCKET_BOUNDARY);
  if (ManifestURL::UpdatesFromGallery(extension)) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.ExternalExtensionEventWebstore",
                              event,
                              EXTERNAL_EXTENSION_BUCKET_BOUNDARY);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Extensions.ExternalExtensionEventNonWebstore",
                              event,
                              EXTERNAL_EXTENSION_BUCKET_BOUNDARY);
  }
}

}  // namespace

ExternalInstallManager::ExternalInstallManager(
    content::BrowserContext* browser_context,
    bool is_first_run)
    : browser_context_(browser_context),
      is_first_run_(is_first_run),
      extension_prefs_(ExtensionPrefs::Get(browser_context_)),
      extension_registry_observer_(this) {
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

  ExternalInstallError::AlertType alert_type =
      (ManifestURL::UpdatesFromGallery(extension) && !is_new_profile)
          ? ExternalInstallError::BUBBLE_ALERT
          : ExternalInstallError::MENU_ALERT;

  error_.reset(new ExternalInstallError(
      browser_context_, extension->id(), alert_type, this));
}

void ExternalInstallManager::RemoveExternalInstallError() {
  error_.reset();
  UpdateExternalExtensionAlert();
}

bool ExternalInstallManager::HasExternalInstallError() const {
  return error_.get() != NULL;
}

void ExternalInstallManager::UpdateExternalExtensionAlert() {
#if defined(ENABLE_EXTENSIONS)
  // If the feature is not enabled, or there is already an error displayed, do
  // nothing.
  if (!FeatureSwitch::prompt_for_external_extensions()->IsEnabled() ||
      HasExternalInstallError()) {
    return;
  }

  // Look for any extensions that were disabled because of being unacknowledged
  // external extensions.
  const Extension* extension = NULL;
  const ExtensionSet& disabled_extensions =
      ExtensionRegistry::Get(browser_context_)->disabled_extensions();
  for (ExtensionSet::const_iterator iter = disabled_extensions.begin();
       iter != disabled_extensions.end();
       ++iter) {
    if (IsUnacknowledgedExternalExtension(iter->get())) {
      extension = iter->get();
      break;
    }
  }

  if (!extension)
    return;  // No unacknowledged external extensions.

  // Otherwise, warn the user about the suspicious extension.
  if (extension_prefs_->IncrementAcknowledgePromptCount(extension->id()) >
      kMaxExtensionAcknowledgePromptCount) {
    // Stop prompting for this extension and record metrics.
    extension_prefs_->AcknowledgeExternalExtension(extension->id());
    LogExternalExtensionEvent(extension, EXTERNAL_EXTENSION_IGNORED);

    // Check if there's another extension that needs prompting.
    UpdateExternalExtensionAlert();
    return;
  }

  if (is_first_run_)
    extension_prefs_->SetExternalInstallFirstRun(extension->id());

  // |first_run| is true if the extension was installed during a first run
  // (even if it's post-first run now).
  AddExternalInstallError(
      extension, extension_prefs_->IsExternalInstallFirstRun(extension->id()));
#endif  // defined(ENABLE_EXTENSIONS)
}

void ExternalInstallManager::AcknowledgeExternalExtension(
    const std::string& id) {
  extension_prefs_->AcknowledgeExternalExtension(id);
  UpdateExternalExtensionAlert();
}

bool ExternalInstallManager::HasExternalInstallBubbleForTesting() const {
  return error_.get() &&
         error_->alert_type() == ExternalInstallError::BUBBLE_ALERT;
}

void ExternalInstallManager::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (!IsUnacknowledgedExternalExtension(extension))
    return;

  // We treat loading as acknowledgement (since the user consciously chose to
  // re-enable the extension).
  AcknowledgeExternalExtension(extension->id());
  LogExternalExtensionEvent(extension, EXTERNAL_EXTENSION_REENABLED);

  // If we had an error for this extension, remove it.
  if (error_.get() && extension->id() == error_->extension_id())
    RemoveExternalInstallError();
}

void ExternalInstallManager::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  // Certain extension locations are specific enough that we can
  // auto-acknowledge any extension that came from one of them.
  if (Manifest::IsPolicyLocation(extension->location()) ||
      extension->location() == Manifest::EXTERNAL_COMPONENT) {
    AcknowledgeExternalExtension(extension->id());
    return;
  }

  if (!IsUnacknowledgedExternalExtension(extension))
    return;

  LogExternalExtensionEvent(extension, EXTERNAL_EXTENSION_INSTALLED);

  UpdateExternalExtensionAlert();
}

void ExternalInstallManager::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  if (IsUnacknowledgedExternalExtension(extension))
    LogExternalExtensionEvent(extension, EXTERNAL_EXTENSION_UNINSTALLED);
}

bool ExternalInstallManager::IsUnacknowledgedExternalExtension(
    const Extension* extension) const {
  if (!FeatureSwitch::prompt_for_external_extensions()->IsEnabled())
    return false;

  return (Manifest::IsExternalLocation(extension->location()) &&
          !extension_prefs_->IsExternalExtensionAcknowledged(extension->id()) &&
          !(extension_prefs_->GetDisableReasons(extension->id()) &
            Extension::DISABLE_SIDELOAD_WIPEOUT));
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
