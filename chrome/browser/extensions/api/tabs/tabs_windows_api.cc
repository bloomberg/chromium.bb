// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/api/tabs/windows_event_router.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"

namespace extensions {

TabsWindowsAPI::TabsWindowsAPI(Profile* profile)
    : profile_(profile) {
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, event_names::kOnWindowCreated);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, event_names::kOnWindowRemoved);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, event_names::kOnWindowFocusedChanged);
}

TabsWindowsAPI::~TabsWindowsAPI() {
}

// static
TabsWindowsAPI* TabsWindowsAPI::Get(Profile* profile) {
  return ProfileKeyedAPIFactory<TabsWindowsAPI>::GetForProfile(profile);
}

WindowsEventRouter* TabsWindowsAPI::windows_event_router() {
  if (!windows_event_router_)
    windows_event_router_.reset(new WindowsEventRouter(profile_));
  return windows_event_router_.get();
}

void TabsWindowsAPI::Shutdown() {
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
}

static base::LazyInstance<ProfileKeyedAPIFactory<TabsWindowsAPI> >
g_factory = LAZY_INSTANCE_INITIALIZER;

ProfileKeyedAPIFactory<TabsWindowsAPI>* TabsWindowsAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

void TabsWindowsAPI::OnListenerAdded(
    const extensions::EventListenerInfo& details) {
  windows_event_router();
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
}

}  // namespace extensions
