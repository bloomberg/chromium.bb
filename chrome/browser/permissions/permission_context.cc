// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_context.h"

#include "chrome/browser/geolocation/geolocation_permission_context.h"
#include "chrome/browser/geolocation/geolocation_permission_context_factory.h"
#include "chrome/browser/media/media_stream_camera_permission_context_factory.h"
#include "chrome/browser/media/media_stream_device_permission_context.h"
#include "chrome/browser/media/media_stream_mic_permission_context_factory.h"
#include "chrome/browser/media/midi_permission_context.h"
#include "chrome/browser/media/midi_permission_context_factory.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/notifications/notification_permission_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_permission_context.h"
#include "chrome/browser/push_messaging/push_messaging_permission_context_factory.h"
#include "chrome/browser/storage/durable_storage_permission_context.h"
#include "chrome/browser/storage/durable_storage_permission_context_factory.h"
#include "content/public/browser/permission_type.h"

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#include "chrome/browser/media/protected_media_identifier_permission_context_factory.h"
#endif

using content::PermissionType;

// static
PermissionContextBase* PermissionContext::Get(Profile* profile,
                                              PermissionType permission_type) {
  // NOTE: the factories used in this method have to stay in sync with
  // ::GetFactories() below.
  switch (permission_type) {
    case PermissionType::GEOLOCATION:
      return GeolocationPermissionContextFactory::GetForProfile(profile);
    case PermissionType::MIDI_SYSEX:
      return MidiPermissionContextFactory::GetForProfile(profile);
    case PermissionType::NOTIFICATIONS:
      return NotificationPermissionContextFactory::GetForProfile(profile);
    case PermissionType::PUSH_MESSAGING:
      return PushMessagingPermissionContextFactory::GetForProfile(profile);
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return ProtectedMediaIdentifierPermissionContextFactory::GetForProfile(
          profile);
#endif
    case content::PermissionType::DURABLE_STORAGE:
      return DurableStoragePermissionContextFactory::GetForProfile(profile);
    case PermissionType::MIDI:
      // PermissionType::MIDI is a valid permission but does not have a
      // permission context. It has a constant value instead.
      break;
    case PermissionType::AUDIO_CAPTURE:
      return MediaStreamMicPermissionContextFactory::GetForProfile(profile);
    case PermissionType::VIDEO_CAPTURE:
      return MediaStreamCameraPermissionContextFactory::GetForProfile(profile);
    default:
      NOTREACHED() << "No PermissionContext associated with "
                   << static_cast<int>(permission_type);
      break;
  }

  return nullptr;
}

// static
const std::list<KeyedServiceBaseFactory*>& PermissionContext::GetFactories() {
  // NOTE: this list has to stay in sync with the factories used by ::Get().
  CR_DEFINE_STATIC_LOCAL(std::list<KeyedServiceBaseFactory*>, factories, ());

  if (factories.empty()) {
    factories.push_back(GeolocationPermissionContextFactory::GetInstance());
    factories.push_back(MidiPermissionContextFactory::GetInstance());
    factories.push_back(NotificationPermissionContextFactory::GetInstance());
    factories.push_back(PushMessagingPermissionContextFactory::GetInstance());
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    factories.push_back(
        ProtectedMediaIdentifierPermissionContextFactory::GetInstance());
#endif
    factories.push_back(DurableStoragePermissionContextFactory::GetInstance());
    factories.push_back(MediaStreamMicPermissionContextFactory::GetInstance());
    factories.push_back(
        MediaStreamCameraPermissionContextFactory::GetInstance());
  }

  return factories;
}
