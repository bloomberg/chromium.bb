// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"
#include "chrome/browser/extensions/api/tabs/windows_event_router.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"

namespace extensions {

TabsWindowsAPI::TabsWindowsAPI(Profile* profile) : profile_(profile) {
  // Tabs API Events.
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, api::tabs::OnCreated::kEventName);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, api::tabs::OnUpdated::kEventName);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, api::tabs::OnMoved::kEventName);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, api::tabs::OnSelectionChanged::kEventName);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, api::tabs::OnActiveChanged::kEventName);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, api::tabs::OnActivated::kEventName);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, api::tabs::OnHighlightChanged::kEventName);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, api::tabs::OnHighlighted::kEventName);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, api::tabs::OnDetached::kEventName);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, api::tabs::OnAttached::kEventName);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, api::tabs::OnRemoved::kEventName);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, api::tabs::OnReplaced::kEventName);

  // Windows API Events.
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, api::windows::OnCreated::kEventName);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, api::windows::OnRemoved::kEventName);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, api::windows::OnFocusChanged::kEventName);
}

TabsWindowsAPI::~TabsWindowsAPI() {
}

// static
TabsWindowsAPI* TabsWindowsAPI::Get(Profile* profile) {
  return ProfileKeyedAPIFactory<TabsWindowsAPI>::GetForProfile(profile);
}

TabsEventRouter* TabsWindowsAPI::tabs_event_router() {
  if (!tabs_event_router_.get())
    tabs_event_router_.reset(new TabsEventRouter(profile_));
  return tabs_event_router_.get();
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

void TabsWindowsAPI::OnListenerAdded(const EventListenerInfo& details) {
  // Initialize the event routers.
  tabs_event_router();
  windows_event_router();
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
}

}  // namespace extensions
