// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extensions_browser_client.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/version.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/preference/chrome_direct_setting.h"
#include "chrome/browser/extensions/api/preference/preference_api.h"
#include "chrome/browser/extensions/api/runtime/chrome_runtime_api_delegate.h"
#include "chrome/browser/extensions/chrome_app_sorting.h"
#include "chrome/browser/extensions/chrome_component_extension_resource_manager.h"
#include "chrome/browser/extensions/chrome_extension_host_delegate.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/url_request_util.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/api/generated_api.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "chrome/common/pref_names.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/api/generated_api.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

// TODO(thestig): Remove this when extensions are fully removed on mobile.
#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_service.h"
#include "chrome/browser/extensions/chrome_process_manager_delegate.h"
#endif

namespace extensions {

ChromeExtensionsBrowserClient::ChromeExtensionsBrowserClient() {
#if defined(ENABLE_EXTENSIONS)
  process_manager_delegate_.reset(new ChromeProcessManagerDelegate);
  api_client_.reset(new ChromeExtensionsAPIClient);
#endif
  // Only set if it hasn't already been set (e.g. by a test).
  if (GetCurrentChannel() == GetDefaultChannel())
    SetCurrentChannel(chrome::VersionInfo::GetChannel());
}

ChromeExtensionsBrowserClient::~ChromeExtensionsBrowserClient() {}

bool ChromeExtensionsBrowserClient::IsShuttingDown() {
  return g_browser_process->IsShuttingDown();
}

bool ChromeExtensionsBrowserClient::AreExtensionsDisabled(
    const CommandLine& command_line,
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return command_line.HasSwitch(switches::kDisableExtensions) ||
      profile->GetPrefs()->GetBoolean(prefs::kDisableExtensions);
}

bool ChromeExtensionsBrowserClient::IsValidContext(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return g_browser_process->profile_manager()->IsValidProfile(profile);
}

bool ChromeExtensionsBrowserClient::IsSameContext(
    content::BrowserContext* first,
    content::BrowserContext* second) {
  return static_cast<Profile*>(first)->IsSameProfile(
      static_cast<Profile*>(second));
}

bool ChromeExtensionsBrowserClient::HasOffTheRecordContext(
    content::BrowserContext* context) {
  return static_cast<Profile*>(context)->HasOffTheRecordProfile();
}

content::BrowserContext* ChromeExtensionsBrowserClient::GetOffTheRecordContext(
    content::BrowserContext* context) {
  return static_cast<Profile*>(context)->GetOffTheRecordProfile();
}

content::BrowserContext* ChromeExtensionsBrowserClient::GetOriginalContext(
    content::BrowserContext* context) {
  return static_cast<Profile*>(context)->GetOriginalProfile();
}

bool ChromeExtensionsBrowserClient::IsGuestSession(
    content::BrowserContext* context) const {
  return static_cast<Profile*>(context)->IsGuestSession();
}

bool ChromeExtensionsBrowserClient::IsExtensionIncognitoEnabled(
    const std::string& extension_id,
    content::BrowserContext* context) const {
  return IsGuestSession(context)
      || util::IsIncognitoEnabled(extension_id, context);
}

bool ChromeExtensionsBrowserClient::CanExtensionCrossIncognito(
    const extensions::Extension* extension,
    content::BrowserContext* context) const {
  return IsGuestSession(context)
      || util::CanCrossIncognito(extension, context);
}

bool ChromeExtensionsBrowserClient::IsWebViewRequest(
    net::URLRequest* request) const {
  return url_request_util::IsWebViewRequest(request);
}

net::URLRequestJob*
ChromeExtensionsBrowserClient::MaybeCreateResourceBundleRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const base::FilePath& directory_path,
    const std::string& content_security_policy,
    bool send_cors_header) {
  return url_request_util::MaybeCreateURLRequestResourceBundleJob(
      request,
      network_delegate,
      directory_path,
      content_security_policy,
      send_cors_header);
}

bool ChromeExtensionsBrowserClient::AllowCrossRendererResourceLoad(
    net::URLRequest* request,
    bool is_incognito,
    const Extension* extension,
    InfoMap* extension_info_map) {
  return url_request_util::AllowCrossRendererResourceLoad(
      request, is_incognito, extension, extension_info_map);
}

PrefService* ChromeExtensionsBrowserClient::GetPrefServiceForContext(
    content::BrowserContext* context) {
  return static_cast<Profile*>(context)->GetPrefs();
}

void ChromeExtensionsBrowserClient::GetEarlyExtensionPrefsObservers(
    content::BrowserContext* context,
    std::vector<ExtensionPrefsObserver*>* observers) const {
#if defined(ENABLE_EXTENSIONS)
  observers->push_back(ContentSettingsService::Get(context));
#endif
}

ProcessManagerDelegate*
ChromeExtensionsBrowserClient::GetProcessManagerDelegate() const {
#if defined(ENABLE_EXTENSIONS)
  return process_manager_delegate_.get();
#else
  return NULL;
#endif
}

scoped_ptr<ExtensionHostDelegate>
ChromeExtensionsBrowserClient::CreateExtensionHostDelegate() {
  return scoped_ptr<ExtensionHostDelegate>(new ChromeExtensionHostDelegate);
}

bool ChromeExtensionsBrowserClient::DidVersionUpdate(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);

  // Unit tests may not provide prefs; assume everything is up-to-date.
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile);
  if (!extension_prefs)
    return false;

  // If we're inside a browser test, then assume prefs are all up-to-date.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType))
    return false;

  PrefService* pref_service = extension_prefs->pref_service();
  base::Version last_version;
  if (pref_service->HasPrefPath(pref_names::kLastChromeVersion)) {
    std::string last_version_str =
        pref_service->GetString(pref_names::kLastChromeVersion);
    last_version = base::Version(last_version_str);
  }

  chrome::VersionInfo current_version_info;
  std::string current_version = current_version_info.Version();
  pref_service->SetString(pref_names::kLastChromeVersion,
                          current_version);

  // If there was no version string in prefs, assume we're out of date.
  if (!last_version.IsValid())
    return true;

  return last_version.IsOlderThan(current_version);
}

scoped_ptr<AppSorting> ChromeExtensionsBrowserClient::CreateAppSorting() {
  return scoped_ptr<AppSorting>(new ChromeAppSorting());
}

bool ChromeExtensionsBrowserClient::IsRunningInForcedAppMode() {
  return chrome::IsRunningInForcedAppMode();
}

ApiActivityMonitor* ChromeExtensionsBrowserClient::GetApiActivityMonitor(
    content::BrowserContext* context) {
#if defined(ENABLE_EXTENSIONS)
  // The ActivityLog monitors and records function calls and events.
  return ActivityLog::GetInstance(context);
#else
  return NULL;
#endif
}

ExtensionSystemProvider*
ChromeExtensionsBrowserClient::GetExtensionSystemFactory() {
  return ExtensionSystemFactory::GetInstance();
}

void ChromeExtensionsBrowserClient::RegisterExtensionFunctions(
    ExtensionFunctionRegistry* registry) const {
// TODO(rockot): Figure out if and why Android really needs to build
// ChromeExtensionsBrowserClient and refactor so this ifdef isn't necessary.
// See http://crbug.com/349436
#if defined(ENABLE_EXTENSIONS)
  // Preferences.
  registry->RegisterFunction<extensions::GetPreferenceFunction>();
  registry->RegisterFunction<extensions::SetPreferenceFunction>();
  registry->RegisterFunction<extensions::ClearPreferenceFunction>();

  // Direct Preference Access for Component Extensions.
  registry->RegisterFunction<
      extensions::chromedirectsetting::GetDirectSettingFunction>();
  registry->RegisterFunction<
      extensions::chromedirectsetting::SetDirectSettingFunction>();
  registry->RegisterFunction<
      extensions::chromedirectsetting::ClearDirectSettingFunction>();

  // Generated APIs from lower-level modules.
  extensions::core_api::GeneratedFunctionRegistry::RegisterAll(registry);

  // Generated APIs from Chrome.
  extensions::api::GeneratedFunctionRegistry::RegisterAll(registry);
#endif
}

ComponentExtensionResourceManager*
ChromeExtensionsBrowserClient::GetComponentExtensionResourceManager() {
  if (!resource_manager_)
    resource_manager_.reset(new ChromeComponentExtensionResourceManager());
  return resource_manager_.get();
}

scoped_ptr<extensions::RuntimeAPIDelegate>
ChromeExtensionsBrowserClient::CreateRuntimeAPIDelegate(
    content::BrowserContext* context) const {
#if defined(ENABLE_EXTENSIONS)
  return scoped_ptr<extensions::RuntimeAPIDelegate>(
      new ChromeRuntimeAPIDelegate(context));
#else
  return scoped_ptr<extensions::RuntimeAPIDelegate>();
#endif
}

}  // namespace extensions
