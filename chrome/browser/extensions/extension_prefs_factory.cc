// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/extensions/extension_pref_value_map_factory.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_prefs_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "extensions/common/constants.h"

namespace extensions {

// static
ExtensionPrefs* ExtensionPrefsFactory::GetForProfile(Profile* profile) {
  return static_cast<ExtensionPrefs*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ExtensionPrefsFactory* ExtensionPrefsFactory::GetInstance() {
  return Singleton<ExtensionPrefsFactory>::get();
}

void ExtensionPrefsFactory::SetInstanceForTesting(
    content::BrowserContext* context, ExtensionPrefs* prefs) {
  Associate(context, prefs);
}

ExtensionPrefsFactory::ExtensionPrefsFactory()
    : BrowserContextKeyedServiceFactory(
        "ExtensionPrefs",
        BrowserContextDependencyManager::GetInstance()) {
}

ExtensionPrefsFactory::~ExtensionPrefsFactory() {
}

BrowserContextKeyedService* ExtensionPrefsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  bool extensions_disabled =
      profile->GetPrefs()->GetBoolean(prefs::kDisableExtensions) ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableExtensions);
  return ExtensionPrefs::Create(
      profile->GetPrefs(),
      profile->GetPath().AppendASCII(extensions::kInstallDirectoryName),
      ExtensionPrefValueMapFactory::GetForBrowserContext(profile),
      extensions_disabled);
}

content::BrowserContext* ExtensionPrefsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace extensions
