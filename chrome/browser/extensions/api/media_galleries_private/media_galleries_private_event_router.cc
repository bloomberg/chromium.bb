// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaGalleriesPrivateEventRouter implementation.

#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_event_router.h"

#include <map>

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/media_gallery/media_file_system_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/media_galleries_private.h"

namespace extensions {

namespace {

std::string GetTransientIdForDeviceId(const std::string& device_id) {
  chrome::MediaFileSystemRegistry* registry =
      g_browser_process->media_file_system_registry();
  return registry->GetTransientIdForDeviceId(device_id);
}

}  // namespace

using extensions::api::media_galleries_private::DeviceAttachmentDetails;
using extensions::api::media_galleries_private::DeviceDetachmentDetails;

MediaGalleriesPrivateEventRouter::MediaGalleriesPrivateEventRouter(
    Profile* profile)
    : profile_(profile) {
  CHECK(profile_);

  base::SystemMonitor* system_monitor = base::SystemMonitor::Get();
  if (system_monitor)
    system_monitor->AddDevicesChangedObserver(this);
}

MediaGalleriesPrivateEventRouter::~MediaGalleriesPrivateEventRouter() {
  base::SystemMonitor* system_monitor = base::SystemMonitor::Get();
  if (system_monitor)
    system_monitor->RemoveDevicesChangedObserver(this);
}

void MediaGalleriesPrivateEventRouter::OnRemovableStorageAttached(
    const std::string& id,
    const string16& name,
    const FilePath::StringType& location) {
  EventRouter* router =
      extensions::ExtensionSystem::Get(profile_)->event_router();
  if (!router->HasEventListener(event_names::kOnAttachEventName))
    return;

  DeviceAttachmentDetails details;
  details.device_name = UTF16ToUTF8(name);
  details.device_id = GetTransientIdForDeviceId(id);

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(details.ToValue().release());
  DispatchEvent(event_names::kOnAttachEventName, args.Pass());
}

void MediaGalleriesPrivateEventRouter::OnRemovableStorageDetached(
    const std::string& id) {
  EventRouter* router =
      extensions::ExtensionSystem::Get(profile_)->event_router();
  if (!router->HasEventListener(event_names::kOnDetachEventName))
    return;

  DeviceDetachmentDetails details;
  details.device_id = GetTransientIdForDeviceId(id);

  scoped_ptr<base::ListValue> args(new ListValue());
  args->Append(details.ToValue().release());
  DispatchEvent(event_names::kOnDetachEventName, args.Pass());
}

void MediaGalleriesPrivateEventRouter::DispatchEvent(
    const std::string& event_name,
    scoped_ptr<base::ListValue> event_args) {
  EventRouter* router =
      extensions::ExtensionSystem::Get(profile_)->event_router();
  if (!router)
    return;
  scoped_ptr<extensions::Event> event(new extensions::Event(
      event_name, event_args.Pass()));
  event->restrict_to_profile = profile_;
  router->BroadcastEvent(event.Pass());
}

}  // namespace extensions
