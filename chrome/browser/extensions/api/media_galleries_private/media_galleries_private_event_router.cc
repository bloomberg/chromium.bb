// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaGalleriesPrivateEventRouter implementation.

#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_event_router.h"

#include <map>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "chrome/common/extensions/api/media_galleries_private.h"
#include "content/public/browser/browser_thread.h"

namespace media_galleries_private = extensions::api::media_galleries_private;

namespace extensions {

namespace {

std::string GetTransientIdForDeviceId(const std::string& device_id) {
  StorageMonitor* monitor = StorageMonitor::GetInstance();
  return monitor->GetTransientIdForDeviceId(device_id);
}

}  // namespace

using media_galleries_private::DeviceAttachmentDetails;
using media_galleries_private::DeviceDetachmentDetails;
using media_galleries_private::GalleryChangeDetails;

MediaGalleriesPrivateEventRouter::MediaGalleriesPrivateEventRouter(
    Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  StorageMonitor::GetInstance()->AddObserver(this);
}

MediaGalleriesPrivateEventRouter::~MediaGalleriesPrivateEventRouter() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // TODO(gbillock): Remove this check once we have destruction order
  // fixed up for profile services and the storage monitor.
  if (StorageMonitor::GetInstance())
    StorageMonitor::GetInstance()->RemoveObserver(this);
}

void MediaGalleriesPrivateEventRouter::OnGalleryChanged(
    MediaGalleryPrefId gallery_id,
    const std::set<std::string>& extension_ids) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  EventRouter* router =
      extensions::ExtensionSystem::Get(profile_)->event_router();
  if (!router->HasEventListener(
          media_galleries_private::OnGalleryChanged::kEventName))
    return;

  for (std::set<std::string>::const_iterator it = extension_ids.begin();
       it != extension_ids.end(); ++it) {
    GalleryChangeDetails details;
    details.gallery_id = gallery_id;
    scoped_ptr<base::ListValue> args(new base::ListValue());
    args->Append(details.ToValue().release());
    scoped_ptr<extensions::Event> event(new extensions::Event(
        media_galleries_private::OnGalleryChanged::kEventName,
        args.Pass()));
    // Use DispatchEventToExtension() instead of BroadcastEvent().
    // BroadcastEvent() sends the gallery changed events to all the extensions
    // who have added a listener to the onGalleryChanged event. There is a
    // chance that an extension might have added an onGalleryChanged() listener
    // without calling addGalleryWatch(). Therefore, use
    // DispatchEventToExtension() to dispatch the gallery changed event only to
    // the watching extensions.
    router->DispatchEventToExtension(*it, event.Pass());
  }
}

void MediaGalleriesPrivateEventRouter::OnRemovableStorageAttached(
    const StorageInfo& info) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  EventRouter* router =
      extensions::ExtensionSystem::Get(profile_)->event_router();
  if (!router->HasEventListener(
          media_galleries_private::OnDeviceAttached::kEventName))
    return;

  MediaGalleryPrefInfo pref_info;
  pref_info.display_name = info.name();
  pref_info.device_id = info.device_id();
  pref_info.type = MediaGalleryPrefInfo::kAutoDetected;
  pref_info.volume_label = info.storage_label();
  pref_info.vendor_name = info.vendor_name();
  pref_info.model_name = info.model_name();
  pref_info.total_size_in_bytes = info.total_size_in_bytes();
  pref_info.volume_metadata_valid = true;

  DeviceAttachmentDetails details;
  details.device_name = UTF16ToUTF8(pref_info.GetGalleryDisplayName());
  details.device_id = GetTransientIdForDeviceId(info.device_id());

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(details.ToValue().release());
  DispatchEvent(
      media_galleries_private::OnDeviceAttached::kEventName, args.Pass());
}

void MediaGalleriesPrivateEventRouter::OnRemovableStorageDetached(
    const StorageInfo& info) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  EventRouter* router =
      extensions::ExtensionSystem::Get(profile_)->event_router();
  if (!router->HasEventListener(
          media_galleries_private::OnDeviceDetached::kEventName))
    return;

  DeviceDetachmentDetails details;
  details.device_id = GetTransientIdForDeviceId(info.device_id());

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(details.ToValue().release());
  DispatchEvent(
      media_galleries_private::OnDeviceDetached::kEventName, args.Pass());
}

void MediaGalleriesPrivateEventRouter::DispatchEvent(
    const std::string& event_name,
    scoped_ptr<base::ListValue> event_args) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  EventRouter* router =
      extensions::ExtensionSystem::Get(profile_)->event_router();
  if (!router)
    return;
  scoped_ptr<extensions::Event> event(new extensions::Event(
      event_name, event_args.Pass()));
  event->restrict_to_browser_context = profile_;
  router->BroadcastEvent(event.Pass());
}

}  // namespace extensions
