// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/test_extensions_browser_client.h"

#include "content/public/browser/browser_context.h"
#include "extensions/browser/app_sorting.h"

using content::BrowserContext;

namespace extensions {

TestExtensionsBrowserClient::TestExtensionsBrowserClient(
    BrowserContext* main_context)
    : main_context_(main_context), incognito_context_(NULL) {
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
    const CommandLine& command_line,
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

PrefService* TestExtensionsBrowserClient::GetPrefServiceForContext(
    BrowserContext* context) {
  return NULL;
}

bool TestExtensionsBrowserClient::DeferLoadingBackgroundHosts(
    BrowserContext* context) const {
  return false;
}

bool TestExtensionsBrowserClient::IsBackgroundPageAllowed(
    BrowserContext* context) const {
  return true;
}

void TestExtensionsBrowserClient::OnExtensionHostCreated(
    content::WebContents* web_contents) {}

void TestExtensionsBrowserClient::OnRenderViewCreatedForBackgroundPage(
    ExtensionHost* host) {}

bool TestExtensionsBrowserClient::DidVersionUpdate(BrowserContext* context) {
  return false;
}

void TestExtensionsBrowserClient::PermitExternalProtocolHandler() {}

scoped_ptr<AppSorting> TestExtensionsBrowserClient::CreateAppSorting() {
  return scoped_ptr<AppSorting>();
}

bool TestExtensionsBrowserClient::IsRunningInForcedAppMode() { return false; }

content::JavaScriptDialogManager*
TestExtensionsBrowserClient::GetJavaScriptDialogManager() {
  return NULL;
}

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

}  // namespace extensions
