// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/app_shell_extensions_browser_client.h"

#include "apps/shell/app_shell_app_sorting.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_factory.h"
#include "base/prefs/testing_pref_store.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "extensions/browser/app_sorting.h"

using content::BrowserContext;

namespace {

// See chrome::RegisterProfilePrefs() in chrome/browser/prefs/browser_prefs.cc
void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) {
  // TODO(jamescook): ExtensionPrefs::RegisterProfilePrefs(registry)
}

}  // namespace

namespace apps {

AppShellExtensionsBrowserClient::AppShellExtensionsBrowserClient(
    BrowserContext* context)
    : browser_context_(context) {
  // Set up the preferences service.
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
  user_prefs::UserPrefs::Set(browser_context_, prefs_.get());
}

AppShellExtensionsBrowserClient::~AppShellExtensionsBrowserClient() {}

bool AppShellExtensionsBrowserClient::IsShuttingDown() {
  return false;
}

bool AppShellExtensionsBrowserClient::AreExtensionsDisabled(
    const CommandLine& command_line,
    BrowserContext* context) {
  return false;
}

bool AppShellExtensionsBrowserClient::IsValidContext(BrowserContext* context) {
  return context == browser_context_;
}

bool AppShellExtensionsBrowserClient::IsSameContext(BrowserContext* first,
                                                    BrowserContext* second) {
  return first == second;
}

bool AppShellExtensionsBrowserClient::HasOffTheRecordContext(
    BrowserContext* context) {
  return false;
}

BrowserContext* AppShellExtensionsBrowserClient::GetOffTheRecordContext(
    BrowserContext* context) {
  // app_shell only supports a single context.
  return NULL;
}

BrowserContext* AppShellExtensionsBrowserClient::GetOriginalContext(
    BrowserContext* context) {
  return context;
}

PrefService* AppShellExtensionsBrowserClient::GetPrefServiceForContext(
    BrowserContext* context) {
  return prefs_.get();
}

bool AppShellExtensionsBrowserClient::DeferLoadingBackgroundHosts(
    BrowserContext* context) const {
  return false;
}

bool AppShellExtensionsBrowserClient::DidVersionUpdate(
    BrowserContext* context) {
  // TODO(jamescook): We might want to tell extensions when app_shell updates.
  return false;
}

scoped_ptr<extensions::AppSorting>
AppShellExtensionsBrowserClient::CreateAppSorting() {
  return scoped_ptr<extensions::AppSorting>(new AppShellAppSorting).Pass();
}

bool AppShellExtensionsBrowserClient::IsRunningInForcedAppMode() {
  return false;
}

}  // namespace apps
