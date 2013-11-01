// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extensions_browser_client.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"

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

// static
ChromeExtensionsBrowserClient* ChromeExtensionsBrowserClient::GetInstance() {
  return g_client.Pointer();
}

}  // namespace extensions
