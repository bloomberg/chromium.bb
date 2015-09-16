// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_stream_camera_permission_context_factory.h"

#include "chrome/browser/media/media_stream_device_permission_context.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

MediaStreamCameraPermissionContextFactory::
    MediaStreamCameraPermissionContextFactory()
    : PermissionContextFactoryBase(
          "MediaStreamCameraPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {}

MediaStreamCameraPermissionContextFactory::
    ~MediaStreamCameraPermissionContextFactory() {}

KeyedService*
MediaStreamCameraPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new MediaStreamDevicePermissionContext(
      static_cast<Profile*>(profile), CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
}

// static
MediaStreamCameraPermissionContextFactory*
MediaStreamCameraPermissionContextFactory::GetInstance() {
  return base::Singleton<MediaStreamCameraPermissionContextFactory>::get();
}

// static
MediaStreamDevicePermissionContext*
MediaStreamCameraPermissionContextFactory::GetForProfile(Profile* profile) {
  return static_cast<MediaStreamDevicePermissionContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}
