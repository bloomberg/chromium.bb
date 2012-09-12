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
#include "chrome/browser/profiles/profile.h"

namespace extensions {

namespace {

// Events
const char kOnAttachEventName[] = "mediaGalleriesPrivate.onDeviceAttached";
const char kOnDetachEventName[] = "mediaGalleriesPrivate.onDeviceDetached";

// Keys
const char kDeviceNameKey[] = "deviceName";
const char kDeviceIdKey[] = "deviceId";

// Used to keep track of transient IDs for removable devices, so persistent
// device IDs are not exposed to renderers.
class TransientDeviceIds {
 public:
  static TransientDeviceIds* GetInstance();

  // Returns the transient for a given |unique_id|.
  // Returns an empty string on error.
  std::string GetTransientIdForUniqueId(const std::string& unique_id) const {
    UniqueIdToTransientIdMap::const_iterator it = id_map_.find(unique_id);
    return (it == id_map_.end()) ? std::string() :
                                   base::Uint64ToString(it->second);
  }

  bool DeviceAttached(const std::string& unique_id) {
    bool inserted =
        id_map_.insert(std::make_pair(unique_id, transient_id_)).second;
    if (!inserted) {
      NOTREACHED();
      return false;
    }
    ++transient_id_;
    return true;
  }

  bool DeviceDetached(const std::string& unique_id) {
    if (id_map_.erase(unique_id) == 0) {
      NOTREACHED();
      return false;
    }
    return true;
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<TransientDeviceIds>;

  typedef std::map<std::string, uint64_t> UniqueIdToTransientIdMap;

  // Use GetInstance().
  TransientDeviceIds() : transient_id_(0) {}
  ~TransientDeviceIds() {}

  UniqueIdToTransientIdMap id_map_;
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

MediaGalleriesPrivateEventRouter::MediaGalleriesPrivateEventRouter(
    Profile* profile)
    : profile_(profile) {
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
  EventRouter* router = profile_->GetExtensionEventRouter();
  if (!router->HasEventListener(kOnAttachEventName))
    return;

  TransientDeviceIds* device_ids = TransientDeviceIds::GetInstance();
  if (!device_ids->DeviceAttached(id))
    return;
  std::string transient_id = device_ids->GetTransientIdForUniqueId(id);
  CHECK(!transient_id.empty());

  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(kDeviceNameKey, UTF16ToUTF8(name));
  dict->SetString(kDeviceIdKey, transient_id);

  scoped_ptr<ListValue> args(new ListValue());
  args->Append(dict);

  DispatchEvent(kOnAttachEventName, args.Pass());
}

void MediaGalleriesPrivateEventRouter::OnRemovableStorageDetached(
    const std::string& id) {
  EventRouter* router = profile_->GetExtensionEventRouter();
  if (!router->HasEventListener(kOnDetachEventName))
    return;

  TransientDeviceIds* device_ids = TransientDeviceIds::GetInstance();
  std::string transient_id = device_ids->GetTransientIdForUniqueId(id);
  if (transient_id.empty()) {
    NOTREACHED();
    return;
  }
  CHECK(device_ids->DeviceDetached(id));

  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(kDeviceIdKey, transient_id);

  scoped_ptr<ListValue> args(new ListValue());
  args->Append(dict);

  DispatchEvent(kOnDetachEventName, args.Pass());
}

void MediaGalleriesPrivateEventRouter::DispatchEvent(
    const std::string& event_name,
    scoped_ptr<ListValue> event_args) {
  EventRouter* router = profile_ ? profile_->GetExtensionEventRouter() : NULL;
  if (!router)
    return;
  router->DispatchEventToRenderers(event_name, event_args.Pass(), profile_,
                                   GURL());
}

}  // namespace extensions
