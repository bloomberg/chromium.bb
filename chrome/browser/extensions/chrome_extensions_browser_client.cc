// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extensions_browser_client.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_switches.h"

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

// static
ChromeExtensionsBrowserClient* ChromeExtensionsBrowserClient::GetInstance() {
  return g_client.Pointer();
}

}  // namespace extensions
