// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/test_extensions_browser_client.h"

#include "content/public/browser/browser_context.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_host_delegate.h"
#include "extensions/browser/test_runtime_api_delegate.h"

using content::BrowserContext;

namespace extensions {

TestExtensionsBrowserClient::TestExtensionsBrowserClient(
    BrowserContext* main_context)
    : main_context_(main_context),
      incognito_context_(NULL),
      process_manager_delegate_(NULL) {
  DCHECK(main_context_);
  DCHECK(!main_context_->IsOffTheRecord());
}

TestExtensionsBrowserClient::~TestExtensionsBrowserClient() {}

void TestExtensionsBrowserClient::SetIncognitoContext(BrowserContext* context) {
  // If a context is provided it must be off-the-record.
  DCHECK(!context || context->IsOffTheRecord());
  incognito_context_ = context;
}

bool TestExtensionsBrowserClient::IsShuttingDown() { return false; }

bool TestExtensionsBrowserClient::AreExtensionsDisabled(
    const base::CommandLine& command_line,
    BrowserContext* context) {
  return false;
}

bool TestExtensionsBrowserClient::IsValidContext(BrowserContext* context) {
  return context == main_context_ ||
         (incognito_context_ && context == incognito_context_);
}

bool TestExtensionsBrowserClient::IsSameContext(BrowserContext* first,
                                                BrowserContext* second) {
  DCHECK(first);
  DCHECK(second);
  return first == second ||
         (first == main_context_ && second == incognito_context_) ||
         (first == incognito_context_ && second == main_context_);
}

bool TestExtensionsBrowserClient::HasOffTheRecordContext(
    BrowserContext* context) {
  return context == main_context_ && incognito_context_ != NULL;
}

BrowserContext* TestExtensionsBrowserClient::GetOffTheRecordContext(
    BrowserContext* context) {
  if (context == main_context_)
    return incognito_context_;
  return NULL;
}

BrowserContext* TestExtensionsBrowserClient::GetOriginalContext(
    BrowserContext* context) {
  return main_context_;
}

bool TestExtensionsBrowserClient::IsGuestSession(
    BrowserContext* context) const {
  return false;
}

bool TestExtensionsBrowserClient::IsExtensionIncognitoEnabled(
    const std::string& extension_id,
    content::BrowserContext* context) const {
  return false;
}

bool TestExtensionsBrowserClient::CanExtensionCrossIncognito(
    const extensions::Extension* extension,
    content::BrowserContext* context) const {
  return false;
}

bool TestExtensionsBrowserClient::IsWebViewRequest(
    net::URLRequest* request) const {
  return false;
}

net::URLRequestJob*
TestExtensionsBrowserClient::MaybeCreateResourceBundleRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const base::FilePath& directory_path,
    const std::string& content_security_policy,
    bool send_cors_header) {
  return NULL;
}

bool TestExtensionsBrowserClient::AllowCrossRendererResourceLoad(
    net::URLRequest* request,
    bool is_incognito,
    const Extension* extension,
    InfoMap* extension_info_map) {
  return false;
}

PrefService* TestExtensionsBrowserClient::GetPrefServiceForContext(
    BrowserContext* context) {
  return NULL;
}

void TestExtensionsBrowserClient::GetEarlyExtensionPrefsObservers(
    content::BrowserContext* context,
    std::vector<ExtensionPrefsObserver*>* observers) const {}

ProcessManagerDelegate* TestExtensionsBrowserClient::GetProcessManagerDelegate()
    const {
  return process_manager_delegate_;
}

scoped_ptr<ExtensionHostDelegate>
TestExtensionsBrowserClient::CreateExtensionHostDelegate() {
  return scoped_ptr<ExtensionHostDelegate>();
}

bool TestExtensionsBrowserClient::DidVersionUpdate(BrowserContext* context) {
  return false;
}

scoped_ptr<AppSorting> TestExtensionsBrowserClient::CreateAppSorting() {
  return scoped_ptr<AppSorting>();
}

bool TestExtensionsBrowserClient::IsRunningInForcedAppMode() { return false; }

ApiActivityMonitor* TestExtensionsBrowserClient::GetApiActivityMonitor(
    BrowserContext* context) {
  return NULL;
}

ExtensionSystemProvider*
TestExtensionsBrowserClient::GetExtensionSystemFactory() {
  // Tests requiring an extension system should override this function.
  NOTREACHED();
  return NULL;
}

void TestExtensionsBrowserClient::RegisterExtensionFunctions(
    ExtensionFunctionRegistry* registry) const {}

scoped_ptr<RuntimeAPIDelegate>
TestExtensionsBrowserClient::CreateRuntimeAPIDelegate(
    content::BrowserContext* context) const {
  return scoped_ptr<RuntimeAPIDelegate>(new TestRuntimeAPIDelegate());
}

ComponentExtensionResourceManager*
TestExtensionsBrowserClient::GetComponentExtensionResourceManager() {
  return NULL;
}

}  // namespace extensions
