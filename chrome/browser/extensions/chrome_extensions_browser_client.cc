// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extensions_browser_client.h"

#include "base/command_line.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"

namespace extensions {

namespace {

static base::LazyInstance<ChromeExtensionsBrowserClient> g_client =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ChromeExtensionsBrowserClient::ChromeExtensionsBrowserClient() {}

ChromeExtensionsBrowserClient::~ChromeExtensionsBrowserClient() {}

bool ChromeExtensionsBrowserClient::IsShuttingDown() {
  return g_browser_process->IsShuttingDown();
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

bool ChromeExtensionsBrowserClient::DeferLoadingBackgroundHosts(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  // The profile may not be valid yet if it is still being initialized.
  // In that case, defer loading, since it depends on an initialized profile.
  // http://crbug.com/222473
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return true;

#if defined(OS_ANDROID)
  return false;
#else
  // There are no browser windows open and the browser process was
  // started to show the app launcher.
  return chrome::GetTotalBrowserCountForProfile(profile) == 0 &&
         CommandLine::ForCurrentProcess()->HasSwitch(switches::kShowAppList);
#endif
}

bool ChromeExtensionsBrowserClient::DidVersionUpdate(
    ExtensionPrefs* extension_prefs) {
  // Unit tests may not provide prefs; assume everything is up-to-date.
  if (!extension_prefs)
    return false;

  // If we're inside a browser test, then assume prefs are all up-to-date.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType))
    return false;

  PrefService* pref_service = extension_prefs->pref_service();
  base::Version last_version;
  if (pref_service->HasPrefPath(prefs::kExtensionsLastChromeVersion)) {
    std::string last_version_str =
        pref_service->GetString(prefs::kExtensionsLastChromeVersion);
    last_version = base::Version(last_version_str);
  }

  chrome::VersionInfo current_version_info;
  std::string current_version = current_version_info.Version();
  pref_service->SetString(prefs::kExtensionsLastChromeVersion,
                          current_version);

  // If there was no version string in prefs, assume we're out of date.
  if (!last_version.IsValid())
    return true;

  return last_version.IsOlderThan(current_version);
}

// static
ChromeExtensionsBrowserClient* ChromeExtensionsBrowserClient::GetInstance() {
  return g_client.Pointer();
}

}  // namespace extensions
