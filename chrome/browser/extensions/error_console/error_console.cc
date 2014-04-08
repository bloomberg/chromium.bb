// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/error_console/error_console.h"

#include <list>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"

namespace extensions {

namespace {

// The key into the Extension prefs for an Extension's specific reporting
// settings.
const char kStoreExtensionErrorsPref[] = "store_extension_errors";

// The default mask (for the time being) is to report everything.
const int32 kDefaultMask = (1 << ExtensionError::MANIFEST_ERROR) |
                           (1 << ExtensionError::RUNTIME_ERROR);

const char kAppsDeveloperToolsExtensionId[] =
    "ohmmkhmmmpcnpikjeljgnaoabkaalbgc";

}  // namespace

void ErrorConsole::Observer::OnErrorConsoleDestroyed() {
}

ErrorConsole::ErrorConsole(Profile* profile)
     : enabled_(false),
       default_mask_(kDefaultMask),
       profile_(profile),
       registry_observer_(this) {
// TODO(rdevlin.cronin): Remove once crbug.com/159265 is fixed.
#if !defined(ENABLE_EXTENSIONS)
  return;
#endif

  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(prefs::kExtensionsUIDeveloperMode,
                      base::Bind(&ErrorConsole::OnPrefChanged,
                                 base::Unretained(this)));

  registry_observer_.Add(ExtensionRegistry::Get(profile_));

  CheckEnabled();
}

ErrorConsole::~ErrorConsole() {
  FOR_EACH_OBSERVER(Observer, observers_, OnErrorConsoleDestroyed());
}

// static
ErrorConsole* ErrorConsole::Get(Profile* profile) {
  return ExtensionSystem::Get(profile)->error_console();
}

void ErrorConsole::SetReportingForExtension(const std::string& extension_id,
                                            ExtensionError::Type type,
                                            bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enabled_ || !Extension::IdIsValid(extension_id))
    return;

  ErrorPreferenceMap::iterator pref = pref_map_.find(extension_id);

  if (pref == pref_map_.end()) {
    pref = pref_map_.insert(
        std::pair<std::string, int32>(extension_id, default_mask_)).first;
  }

  pref->second =
      enabled ? pref->second | (1 << type) : pref->second &~(1 << type);

  ExtensionPrefs::Get(profile_)->UpdateExtensionPref(
      extension_id,
      kStoreExtensionErrorsPref,
      base::Value::CreateIntegerValue(pref->second));
}

void ErrorConsole::UseDefaultReportingForExtension(
    const std::string& extension_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enabled_ || !Extension::IdIsValid(extension_id))
    return;

  pref_map_.erase(extension_id);
  ExtensionPrefs::Get(profile_)->UpdateExtensionPref(
      extension_id,
      kStoreExtensionErrorsPref,
      NULL);
}

void ErrorConsole::ReportError(scoped_ptr<ExtensionError> error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enabled_ || !Extension::IdIsValid(error->extension_id()))
    return;

  ErrorPreferenceMap::const_iterator pref =
      pref_map_.find(error->extension_id());
  // Check the mask to see if we report the error. If we don't have a specific
  // entry, use the default mask.
  if ((pref == pref_map_.end() &&
          ((default_mask_ & (1 << error->type())) == 0)) ||
      (pref != pref_map_.end() && (pref->second & (1 << error->type())) == 0)) {
    return;
  }

  const ExtensionError* weak_error = errors_.AddError(error.Pass());
  FOR_EACH_OBSERVER(Observer, observers_, OnErrorAdded(weak_error));
}

const ErrorList& ErrorConsole::GetErrorsForExtension(
    const std::string& extension_id) const {
  return errors_.GetErrorsForExtension(extension_id);
}

void ErrorConsole::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void ErrorConsole::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

bool ErrorConsole::IsEnabledForChromeExtensionsPage() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode) &&
         (FeatureSwitch::error_console()->IsEnabled() ||
          GetCurrentChannel() <= chrome::VersionInfo::CHANNEL_DEV);
}

bool ErrorConsole::IsEnabledForAppsDeveloperTools() const {
  return ExtensionRegistry::Get(profile_)->enabled_extensions()
      .Contains(kAppsDeveloperToolsExtensionId);
}

void ErrorConsole::CheckEnabled() {
  bool should_be_enabled = IsEnabledForChromeExtensionsPage() ||
                           IsEnabledForAppsDeveloperTools();
  if (should_be_enabled && !enabled_)
    Enable();
  if (!should_be_enabled && enabled_)
    Disable();
}

void ErrorConsole::Enable() {
  enabled_ = true;

  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_PROFILE_DESTROYED,
      content::NotificationService::AllBrowserContextsAndSources());
  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
      content::Source<Profile>(profile_));
  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_EXTENSION_INSTALLED,
      content::Source<Profile>(profile_));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(profile_)->enabled_extensions();
  for (ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end();
       ++iter) {
    int mask = 0;
    if (prefs->ReadPrefAsInteger(iter->get()->id(),
                                 kStoreExtensionErrorsPref,
                                 &mask)) {
      pref_map_[iter->get()->id()] = mask;
    }
    AddManifestErrorsForExtension(iter->get());
  }
}

void ErrorConsole::Disable() {
  notification_registrar_.RemoveAll();
  errors_.RemoveAllErrors();
  enabled_ = false;
}

void ErrorConsole::OnPrefChanged() {
  CheckEnabled();
}

void ErrorConsole::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                       const Extension* extension) {
  CheckEnabled();
}

void ErrorConsole::OnExtensionLoaded(content::BrowserContext* browser_context,
                                     const Extension* extension) {
  CheckEnabled();
}

void ErrorConsole::AddManifestErrorsForExtension(const Extension* extension) {
  const std::vector<InstallWarning>& warnings =
      extension->install_warnings();
  for (std::vector<InstallWarning>::const_iterator iter = warnings.begin();
       iter != warnings.end(); ++iter) {
    ReportError(scoped_ptr<ExtensionError>(new ManifestError(
        extension->id(),
        base::UTF8ToUTF16(iter->message),
        base::UTF8ToUTF16(iter->key),
        base::UTF8ToUTF16(iter->specific))));
  }
}

void ErrorConsole::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      // If incognito profile which we are associated with is destroyed, also
      // destroy all incognito errors.
      if (profile->IsOffTheRecord() && profile_->IsSameProfile(profile))
        errors_.RemoveIncognitoErrors();
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED:
      // No need to check the profile here, since we registered to only receive
      // notifications from our own.
      errors_.Remove(content::Details<Extension>(details).ptr()->id());
      break;
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
      const InstalledExtensionInfo* info =
          content::Details<InstalledExtensionInfo>(details).ptr();

      // We don't want to have manifest errors from previous installs. We want
      // to keep runtime errors, though, because extensions are reloaded on a
      // refresh of chrome:extensions, and we don't want to wipe our history
      // whenever that happens.
      errors_.RemoveErrorsForExtensionOfType(info->extension->id(),
                                             ExtensionError::MANIFEST_ERROR);

      AddManifestErrorsForExtension(info->extension);
      break;
    }
    default:
      NOTREACHED();
  }
}

}  // namespace extensions
