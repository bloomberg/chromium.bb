// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/error_console/error_console.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_prefs.h"
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

// This is the default mask for which errors to report. That is, if an extension
// does not have specific preference set, this will be used instead.
const int kDefaultMask = 0;

const char kAppsDeveloperToolsExtensionId[] =
    "ohmmkhmmmpcnpikjeljgnaoabkaalbgc";

}  // namespace

void ErrorConsole::Observer::OnErrorConsoleDestroyed() {
}

ErrorConsole::ErrorConsole(Profile* profile)
     : enabled_(false),
       default_mask_(kDefaultMask),
       profile_(profile),
       prefs_(NULL),
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

  int mask = default_mask_;
  // This call can fail if the preference isn't set, but we don't really care
  // if it does, because we just use the default mask instead.
  prefs_->ReadPrefAsInteger(extension_id, kStoreExtensionErrorsPref, &mask);

  if (enabled)
    mask |= 1 << type;
  else
    mask &= ~(1 << type);

  prefs_->UpdateExtensionPref(extension_id,
                              kStoreExtensionErrorsPref,
                              new base::FundamentalValue(mask));
}

void ErrorConsole::SetReportingAllForExtension(
    const std::string& extension_id, bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enabled_ || !Extension::IdIsValid(extension_id))
    return;

  int mask = 0;
  if (enabled)
    mask = (1 << ExtensionError::NUM_ERROR_TYPES) - 1;

  prefs_->UpdateExtensionPref(extension_id,
                              kStoreExtensionErrorsPref,
                              new base::FundamentalValue(mask));
}

bool ErrorConsole::IsReportingEnabledForExtension(
    const std::string& extension_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enabled_ || !Extension::IdIsValid(extension_id))
    return false;

  return GetMaskForExtension(extension_id) != 0;
}

void ErrorConsole::UseDefaultReportingForExtension(
    const std::string& extension_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enabled_ || !Extension::IdIsValid(extension_id))
    return;

  prefs_->UpdateExtensionPref(extension_id, kStoreExtensionErrorsPref, NULL);
}

void ErrorConsole::ReportError(scoped_ptr<ExtensionError> error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enabled_ || !Extension::IdIsValid(error->extension_id()))
    return;

  int mask = GetMaskForExtension(error->extension_id());
  if (!(mask & (1 << error->type())))
    return;

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

  // We postpone the initialization of |prefs_| until now because they can be
  // NULL in unit_tests. Any unit tests that enable the error console should
  // also create an ExtensionPrefs object.
  prefs_ = ExtensionPrefs::Get(profile_);

  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_PROFILE_DESTROYED,
      content::NotificationService::AllBrowserContextsAndSources());

  const ExtensionSet& extensions =
      ExtensionRegistry::Get(profile_)->enabled_extensions();
  for (ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end();
       ++iter) {
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
                                       const Extension* extension,
                                       UnloadedExtensionInfo::Reason reason) {
  CheckEnabled();
}

void ErrorConsole::OnExtensionLoaded(content::BrowserContext* browser_context,
                                     const Extension* extension) {
  CheckEnabled();
}

void ErrorConsole::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  // We don't want to have manifest errors from previous installs. We want
  // to keep runtime errors, though, because extensions are reloaded on a
  // refresh of chrome:extensions, and we don't want to wipe our history
  // whenever that happens.
  errors_.RemoveErrorsForExtensionOfType(extension->id(),
                                         ExtensionError::MANIFEST_ERROR);
  AddManifestErrorsForExtension(extension);
}

void ErrorConsole::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  errors_.Remove(extension->id());
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
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_DESTROYED, type);
  Profile* profile = content::Source<Profile>(source).ptr();
  // If incognito profile which we are associated with is destroyed, also
  // destroy all incognito errors.
  if (profile->IsOffTheRecord() && profile_->IsSameProfile(profile))
    errors_.RemoveIncognitoErrors();
}

int ErrorConsole::GetMaskForExtension(const std::string& extension_id) const {
  // Registered preferences take priority over everything else.
  int pref = 0;
  if (prefs_->ReadPrefAsInteger(extension_id, kStoreExtensionErrorsPref, &pref))
    return pref;

  // If the extension is unpacked, we report all error types by default.
  const Extension* extension =
      ExtensionRegistry::Get(profile_)->GetExtensionById(
          extension_id, ExtensionRegistry::EVERYTHING);
  if (extension && extension->location() == Manifest::UNPACKED)
    return (1 << ExtensionError::NUM_ERROR_TYPES) - 1;

  // Otherwise, use the default mask.
  return default_mask_;
}

}  // namespace extensions
