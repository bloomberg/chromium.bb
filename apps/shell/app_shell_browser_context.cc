// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/app_shell_browser_context.h"

#include "apps/app_load_service_factory.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_factory.h"
#include "base/prefs/testing_pref_store.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "components/user_prefs/user_prefs.h"

namespace {

// See ChromeBrowserMainExtraPartsProfiles for details.
void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  apps::AppLoadServiceFactory::GetInstance();
}

// See chrome::RegisterProfilePrefs() in chrome/browser/prefs/browser_prefs.cc
void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) {
  // TODO(jamescook): ExtensionPrefs::RegisterProfilePrefs(registry)
}

}  // namespace

namespace apps {

// TODO(jamescook): Should this be an off-the-record context?
// TODO(jamescook): Could initialize NetLog here to get logs from the networking
// stack.
AppShellBrowserContext::AppShellBrowserContext()
    : content::ShellBrowserContext(false, NULL) {
  EnsureBrowserContextKeyedServiceFactoriesBuilt();

  base::PrefServiceFactory factory;
  factory.set_user_prefs(new TestingPrefStore);
  factory.set_extension_prefs(new TestingPrefStore);
  // app_shell should not require syncable preferences, but for now we need to
  // recycle some of the RegisterProfilePrefs() code in Chrome.
  // TODO(jamescook): Convert this to user_prefs::PrefRegistrySimple.
  user_prefs::PrefRegistrySyncable* pref_registry =
      new user_prefs::PrefRegistrySyncable;
  // Prefs should be registered before the PrefService is created.
  RegisterPrefs(pref_registry);
  prefs_ = factory.Create(pref_registry).Pass();
  user_prefs::UserPrefs::Set(this, prefs_.get());
}

AppShellBrowserContext::~AppShellBrowserContext() {
}

void AppShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext1() {
  NOTREACHED();
}
void AppShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext2() {
  NOTREACHED();
}
void AppShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext3() {
  NOTREACHED();
}
void AppShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext4() {
  NOTREACHED();
}
void AppShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext5() {
  NOTREACHED();
}
void AppShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext6() {
  NOTREACHED();
}
void AppShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext7() {
  NOTREACHED();
}
void AppShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext8() {
  NOTREACHED();
}
void AppShellBrowserContext::ProfileFunctionCallOnNonProfileBrowserContext9() {
  NOTREACHED();
}

}  // namespace apps
