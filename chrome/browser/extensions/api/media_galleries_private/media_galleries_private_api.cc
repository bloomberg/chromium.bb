// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_api.h"

#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_api_factory.h"
#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_event_router.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"

namespace extensions {

MediaGalleriesPrivateAPI::MediaGalleriesPrivateAPI(Profile* profile)
    : profile_(profile) {
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, event_names::kOnAttachEventName);
   ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, event_names::kOnDetachEventName);

}

MediaGalleriesPrivateAPI::~MediaGalleriesPrivateAPI() {
}

void MediaGalleriesPrivateAPI::Shutdown() {
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
}

void MediaGalleriesPrivateAPI::OnListenerAdded(
    const extensions::EventListenerInfo& details) {
  media_galleries_private_event_router_.reset(
      new MediaGalleriesPrivateEventRouter(profile_));
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
}

}  // namespace extensions
