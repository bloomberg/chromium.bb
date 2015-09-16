// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_stream_mic_permission_context_factory.h"

#include "chrome/browser/media/media_stream_device_permission_context.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

MediaStreamMicPermissionContextFactory::MediaStreamMicPermissionContextFactory()
    : PermissionContextFactoryBase(
          "MediaStreamMicPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {}

MediaStreamMicPermissionContextFactory::
    ~MediaStreamMicPermissionContextFactory() {}

KeyedService* MediaStreamMicPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new MediaStreamDevicePermissionContext(
      static_cast<Profile*>(profile), CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
}

// static
MediaStreamMicPermissionContextFactory*
MediaStreamMicPermissionContextFactory::GetInstance() {
  return base::Singleton<MediaStreamMicPermissionContextFactory>::get();
}

// static
MediaStreamDevicePermissionContext*
MediaStreamMicPermissionContextFactory::GetForProfile(Profile* profile) {
  return static_cast<MediaStreamDevicePermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}
