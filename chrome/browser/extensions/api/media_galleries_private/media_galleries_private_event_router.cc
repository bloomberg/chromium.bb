// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaGalleriesPrivateEventRouter implementation.

#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_event_router.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/media_galleries_private.h"

namespace extensions {

namespace {

// Events
const char kOnAttachEventName[] = "mediaGalleriesPrivate.onDeviceAttached";
const char kOnDetachEventName[] = "mediaGalleriesPrivate.onDeviceDetached";

// Used to keep track of transient IDs for removable devices, so persistent
// device IDs are not exposed to renderers.
class TransientDeviceIds {
 public:
  static TransientDeviceIds* GetInstance();

  // Returns the transient for a given |device_id|.
  // Returns an empty string on error.
  std::string GetTransientIdForDeviceId(const std::string& device_id) const {
    DeviceIdToTransientIdMap::const_iterator it = id_map_.find(device_id);
    CHECK(it != id_map_.end());
    return base::Uint64ToString(it->second);
  }

  void DeviceAttached(const std::string& device_id) {
    bool inserted =
        id_map_.insert(std::make_pair(device_id, transient_id_)).second;
    if (inserted) {
      // Inserted a device that has never been seen before.
      ++transient_id_;
    }
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<TransientDeviceIds>;

  typedef std::map<std::string, uint64_t> DeviceIdToTransientIdMap;

  // Use GetInstance().
  TransientDeviceIds() : transient_id_(0) {}
  ~TransientDeviceIds() {}

  DeviceIdToTransientIdMap id_map_;
  uint64_t transient_id_;

  DISALLOW_COPY_AND_ASSIGN(TransientDeviceIds);
};

static base::LazyInstance<TransientDeviceIds> g_transient_device_ids =
    LAZY_INSTANCE_INITIALIZER;

// static
TransientDeviceIds* TransientDeviceIds::GetInstance() {
  return g_transient_device_ids.Pointer();
}

}  // namespace

using extensions::api::media_galleries_private::DeviceAttachmentDetails;
using extensions::api::media_galleries_private::DeviceDetachmentDetails;

MediaGalleriesPrivateEventRouter::MediaGalleriesPrivateEventRouter(
    Profile* profile)
    : profile_(profile) {
  CHECK(profile_);

  base::SystemMonitor* system_monitor = base::SystemMonitor::Get();
  if (system_monitor) {
    system_monitor->AddDevicesChangedObserver(this);

    // Add the devices that were already present before
    // MediaGalleriesPrivateEventRouter creation.
    std::vector<base::SystemMonitor::RemovableStorageInfo> storage_info =
        system_monitor->GetAttachedRemovableStorage();
    TransientDeviceIds* device_ids = TransientDeviceIds::GetInstance();
    for (size_t i = 0; i < storage_info.size(); ++i)
      device_ids->DeviceAttached(storage_info[i].device_id);
  }
}

MediaGalleriesPrivateEventRouter::~MediaGalleriesPrivateEventRouter() {
  base::SystemMonitor* system_monitor = base::SystemMonitor::Get();
  if (system_monitor)
    system_monitor->RemoveDevicesChangedObserver(this);
}

// static
std::string MediaGalleriesPrivateEventRouter::GetTransientIdForDeviceId(
    const std::string& device_id) {
  return TransientDeviceIds::GetInstance()->GetTransientIdForDeviceId(
      device_id);
}

void MediaGalleriesPrivateEventRouter::OnRemovableStorageAttached(
    const std::string& id,
    const string16& name,
    const FilePath::StringType& location) {
  TransientDeviceIds::GetInstance()->DeviceAttached(id);

  EventRouter* router =
      extensions::ExtensionSystem::Get(profile_)->event_router();
  if (!router->HasEventListener(kOnAttachEventName))
    return;

  DeviceAttachmentDetails details;
  details.device_name = UTF16ToUTF8(name);
  details.device_id = GetTransientIdForDeviceId(id);

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(details.ToValue().release());
  DispatchEvent(kOnAttachEventName, args.Pass());
}

void MediaGalleriesPrivateEventRouter::OnRemovableStorageDetached(
    const std::string& id) {
  EventRouter* router =
      extensions::ExtensionSystem::Get(profile_)->event_router();
  if (!router->HasEventListener(kOnDetachEventName))
    return;

  DeviceDetachmentDetails details;
  details.device_id = GetTransientIdForDeviceId(id);

  scoped_ptr<base::ListValue> args(new ListValue());
  args->Append(details.ToValue().release());
  DispatchEvent(kOnDetachEventName, args.Pass());
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
