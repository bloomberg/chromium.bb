// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_extensions_browser_client.h"

#include "apps/common/api/generated_api.h"
#include "apps/shell/browser/shell_app_sorting.h"
#include "apps/shell/browser/shell_app_window_api.h"
#include "apps/shell/browser/shell_extension_system_factory.h"
#include "apps/shell/browser/shell_extension_web_contents_observer.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_factory.h"
#include "base/prefs/testing_pref_store.h"
#include "chrome/common/extensions/api/generated_api.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_host_delegate.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/api/generated_api.h"

using content::BrowserContext;

namespace extensions {
namespace {

// See chrome::RegisterProfilePrefs() in chrome/browser/prefs/browser_prefs.cc
void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) {
  ExtensionPrefs::RegisterProfilePrefs(registry);
}

// A minimal ExtensionHostDelegate.
class ShellExtensionHostDelegate : public ExtensionHostDelegate {
 public:
  ShellExtensionHostDelegate() {}
  virtual ~ShellExtensionHostDelegate() {}

  // ExtensionHostDelegate implementation.
  virtual void OnExtensionHostCreated(content::WebContents* web_contents)
      OVERRIDE;

  virtual void OnRenderViewCreatedForBackgroundPage(ExtensionHost* host)
      OVERRIDE {}

  virtual content::JavaScriptDialogManager* GetJavaScriptDialogManager()
      OVERRIDE {
    // TODO(jamescook): Create a JavaScriptDialogManager or reuse the one from
    // content_shell.
    NOTREACHED();
    return NULL;
  }

  virtual void CreateTab(content::WebContents* web_contents,
                         const std::string& extension_id,
                         WindowOpenDisposition disposition,
                         const gfx::Rect& initial_pos,
                         bool user_gesture) OVERRIDE {
    // TODO(jamescook): Should app_shell support opening popup windows?
    NOTREACHED();
  }

  virtual void ProcessMediaAccessRequest(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback,
      const Extension* extension) OVERRIDE {
    // app_shell does not support media capture.
    NOTREACHED();
  }
};

void ShellExtensionHostDelegate::OnExtensionHostCreated(
    content::WebContents* web_contents) {
  ShellExtensionWebContentsObserver::CreateForWebContents(web_contents);
}

}  // namespace

ShellExtensionsBrowserClient::ShellExtensionsBrowserClient(
    BrowserContext* context)
    : browser_context_(context), api_client_(new ExtensionsAPIClient) {
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

ShellExtensionsBrowserClient::~ShellExtensionsBrowserClient() {}

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
    const extensions::Extension* extension,
    content::BrowserContext* context) const {
  return false;
}

PrefService* ShellExtensionsBrowserClient::GetPrefServiceForContext(
    BrowserContext* context) {
  return prefs_.get();
}

void ShellExtensionsBrowserClient::GetEarlyExtensionPrefsObservers(
    content::BrowserContext* context,
    std::vector<ExtensionPrefsObserver*>* observers) const {}

bool ShellExtensionsBrowserClient::DeferLoadingBackgroundHosts(
    BrowserContext* context) const {
  return false;
}

bool ShellExtensionsBrowserClient::IsBackgroundPageAllowed(
    BrowserContext* context) const {
  return true;
}

scoped_ptr<ExtensionHostDelegate>
ShellExtensionsBrowserClient::CreateExtensionHostDelegate() {
  return scoped_ptr<ExtensionHostDelegate>(new ShellExtensionHostDelegate);
}

bool ShellExtensionsBrowserClient::DidVersionUpdate(BrowserContext* context) {
  // TODO(jamescook): We might want to tell extensions when app_shell updates.
  return false;
}

scoped_ptr<AppSorting> ShellExtensionsBrowserClient::CreateAppSorting() {
  return scoped_ptr<AppSorting>(new apps::ShellAppSorting);
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
  // Generated APIs from lower-level modules.
  extensions::core_api::GeneratedFunctionRegistry::RegisterAll(registry);
  apps::api::GeneratedFunctionRegistry::RegisterAll(registry);

  // TODO(rockot): Remove dependency on src/chrome once we have some core APIs
  // moved out. Also clean up the comment below. See http://crbug.com/349042.
  extensions::api::GeneratedFunctionRegistry::RegisterAll(registry);

  // Register our simplified implementation for chrome.app.window.create().
  // By registering it after extensions::api::GeneratedFunctionRegistry above
  // we override the Chrome version of this function.
  // TODO(jamescook): Remove the switch when the simple window good enough to
  // use by default.
  const std::string kNewAppWindow = "new-app-window";
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kNewAppWindow))
    registry->RegisterFunction<ShellAppWindowCreateFunction>();
}

}  // namespace extensions
