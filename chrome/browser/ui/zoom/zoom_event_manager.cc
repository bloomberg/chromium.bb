// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/zoom/zoom_event_manager.h"

#include "content/public/browser/browser_context.h"

namespace {
static const char kBrowserZoomEventManager[] = "browser_zoom_event_manager";
}

ZoomEventManager* ZoomEventManager::GetForBrowserContext(
    content::BrowserContext* context) {
  if (!context->GetUserData(kBrowserZoomEventManager))
    context->SetUserData(kBrowserZoomEventManager, new ZoomEventManager);
  return static_cast<ZoomEventManager*>(
      context->GetUserData(kBrowserZoomEventManager));
}

ZoomEventManager::ZoomEventManager() {}

ZoomEventManager::~ZoomEventManager() {}

void ZoomEventManager::OnZoomLevelChanged(
    const content::HostZoomMap::ZoomLevelChange& change) {
  zoom_level_changed_callbacks_.Notify(change);
}

scoped_ptr<content::HostZoomMap::Subscription>
ZoomEventManager::AddZoomLevelChangedCallback(
    const content::HostZoomMap::ZoomLevelChangedCallback& callback) {
  return zoom_level_changed_callbacks_.Add(callback);
}
