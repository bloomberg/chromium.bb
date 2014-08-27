// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_extensions_browser_client.h"

#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_factory.h"
#include "base/prefs/testing_pref_store.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/api/generated_api.h"
#include "extensions/shell/browser/api/shell_extensions_api_client.h"
#include "extensions/shell/browser/shell_app_sorting.h"
#include "extensions/shell/browser/shell_extension_host_delegate.h"
#include "extensions/shell/browser/shell_extension_system_factory.h"
#include "extensions/shell/browser/shell_runtime_api_delegate.h"
#include "extensions/shell/common/api/generated_api.h"

using content::BrowserContext;

namespace extensions {
namespace {

// See chrome::RegisterProfilePrefs() in chrome/browser/prefs/browser_prefs.cc
void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) {
  ExtensionPrefs::RegisterProfilePrefs(registry);
}

}  // namespace

ShellExtensionsBrowserClient::ShellExtensionsBrowserClient(
    BrowserContext* context)
    : browser_context_(context), api_client_(new ShellExtensionsAPIClient) {
  // Set up the preferences service.
  base::PrefServiceFactory factory;
  factory.set_user_prefs(new TestingPrefStore);
  factory.set_extension_prefs(new TestingPrefStore);
  // app_shell should not require syncable preferences, but for now we need to
  // recycle some of the RegisterProfilePrefs() code in Chrome.
  // TODO(jamescook): Convert this to PrefRegistrySimple.
  user_prefs::PrefRegistrySyncable* pref_registry =
      new user_prefs::PrefRegistrySyncable;
  // Prefs should be registered before the PrefService is created.
  RegisterPrefs(pref_registry);
  prefs_ = factory.Create(pref_registry).Pass();
  user_prefs::UserPrefs::Set(browser_context_, prefs_.get());
}

ShellExtensionsBrowserClient::~ShellExtensionsBrowserClient() {
}

bool ShellExtensionsBrowserClient::IsShuttingDown() {
  return false;
}

bool ShellExtensionsBrowserClient::AreExtensionsDisabled(
    const base::CommandLine& command_line,
    BrowserContext* context) {
  return false;
}

bool ShellExtensionsBrowserClient::IsValidContext(BrowserContext* context) {
  return context == browser_context_;
}

bool ShellExtensionsBrowserClient::IsSameContext(BrowserContext* first,
                                                 BrowserContext* second) {
  return first == second;
}

bool ShellExtensionsBrowserClient::HasOffTheRecordContext(
    BrowserContext* context) {
  return false;
}

BrowserContext* ShellExtensionsBrowserClient::GetOffTheRecordContext(
    BrowserContext* context) {
  // app_shell only supports a single context.
  return NULL;
}

BrowserContext* ShellExtensionsBrowserClient::GetOriginalContext(
    BrowserContext* context) {
  return context;
}

bool ShellExtensionsBrowserClient::IsGuestSession(
    BrowserContext* context) const {
  return false;
}

bool ShellExtensionsBrowserClient::IsExtensionIncognitoEnabled(
    const std::string& extension_id,
    content::BrowserContext* context) const {
  return false;
}

bool ShellExtensionsBrowserClient::CanExtensionCrossIncognito(
    const Extension* extension,
    content::BrowserContext* context) const {
  return false;
}

bool ShellExtensionsBrowserClient::IsWebViewRequest(
    net::URLRequest* request) const {
  return false;
}

net::URLRequestJob*
ShellExtensionsBrowserClient::MaybeCreateResourceBundleRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const base::FilePath& directory_path,
    const std::string& content_security_policy,
    bool send_cors_header) {
  return NULL;
}

bool ShellExtensionsBrowserClient::AllowCrossRendererResourceLoad(
    net::URLRequest* request,
    bool is_incognito,
    const Extension* extension,
    InfoMap* extension_info_map) {
  // Note: This may need to change if app_shell supports webview.
  return false;
}

PrefService* ShellExtensionsBrowserClient::GetPrefServiceForContext(
    BrowserContext* context) {
  return prefs_.get();
}

void ShellExtensionsBrowserClient::GetEarlyExtensionPrefsObservers(
    content::BrowserContext* context,
    std::vector<ExtensionPrefsObserver*>* observers) const {
}

ProcessManagerDelegate*
ShellExtensionsBrowserClient::GetProcessManagerDelegate() const {
  return NULL;
}

scoped_ptr<ExtensionHostDelegate>
ShellExtensionsBrowserClient::CreateExtensionHostDelegate() {
  return scoped_ptr<ExtensionHostDelegate>(new ShellExtensionHostDelegate);
}

bool ShellExtensionsBrowserClient::DidVersionUpdate(BrowserContext* context) {
  // TODO(jamescook): We might want to tell extensions when app_shell updates.
  return false;
}

void ShellExtensionsBrowserClient::PermitExternalProtocolHandler() {
}

scoped_ptr<AppSorting> ShellExtensionsBrowserClient::CreateAppSorting() {
  return scoped_ptr<AppSorting>(new ShellAppSorting);
}

bool ShellExtensionsBrowserClient::IsRunningInForcedAppMode() {
  return false;
}

ApiActivityMonitor* ShellExtensionsBrowserClient::GetApiActivityMonitor(
    BrowserContext* context) {
  // app_shell doesn't monitor API function calls or events.
  return NULL;
}

ExtensionSystemProvider*
ShellExtensionsBrowserClient::GetExtensionSystemFactory() {
  return ShellExtensionSystemFactory::GetInstance();
}

void ShellExtensionsBrowserClient::RegisterExtensionFunctions(
    ExtensionFunctionRegistry* registry) const {
  // Register core extension-system APIs.
  core_api::GeneratedFunctionRegistry::RegisterAll(registry);

  // Register chrome.shell APIs.
  shell_api::GeneratedFunctionRegistry::RegisterAll(registry);
}

scoped_ptr<RuntimeAPIDelegate>
ShellExtensionsBrowserClient::CreateRuntimeAPIDelegate(
    content::BrowserContext* context) const {
  return scoped_ptr<RuntimeAPIDelegate>(new ShellRuntimeAPIDelegate());
}

ComponentExtensionResourceManager*
ShellExtensionsBrowserClient::GetComponentExtensionResourceManager() {
  return NULL;
}

net::NetLog* ShellExtensionsBrowserClient::GetNetLog() {
  return NULL;
}

}  // namespace extensions
