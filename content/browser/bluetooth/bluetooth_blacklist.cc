// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_blacklist.h"

#include "base/logging.h"
#include "content/common/bluetooth/bluetooth_scan_filter.h"

using device::BluetoothUUID;

namespace {

static base::LazyInstance<content::BluetoothBlacklist>::Leaky g_singleton =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace content {

BluetoothBlacklist::~BluetoothBlacklist() {}

// static
BluetoothBlacklist& BluetoothBlacklist::Get() {
  return g_singleton.Get();
}

void BluetoothBlacklist::AddOrDie(const device::BluetoothUUID& uuid,
                                  Value value) {
  auto insert_result = blacklisted_uuids_.insert(std::make_pair(uuid, value));
  CHECK(insert_result.second);
}

bool BluetoothBlacklist::IsExcluded(const BluetoothUUID& uuid) const {
  const auto& it = blacklisted_uuids_.find(uuid);
  if (it == blacklisted_uuids_.end())
    return false;
  return it->second == Value::EXCLUDE;
}

bool BluetoothBlacklist::IsExcluded(
    const std::vector<content::BluetoothScanFilter>& filters) {
  for (const BluetoothScanFilter& filter : filters) {
    for (const BluetoothUUID& service : filter.services) {
      if (IsExcluded(service)) {
        return true;
      }
    }
  }
  return false;
}

bool BluetoothBlacklist::IsExcludedFromReads(const BluetoothUUID& uuid) const {
  const auto& it = blacklisted_uuids_.find(uuid);
  if (it == blacklisted_uuids_.end())
    return false;
  return it->second == Value::EXCLUDE || it->second == Value::EXCLUDE_READS;
}

bool BluetoothBlacklist::IsExcludedFromWrites(const BluetoothUUID& uuid) const {
  const auto& it = blacklisted_uuids_.find(uuid);
  if (it == blacklisted_uuids_.end())
    return false;
  return it->second == Value::EXCLUDE || it->second == Value::EXCLUDE_WRITES;
}

void BluetoothBlacklist::RemoveExcludedUuids(
    std::vector<device::BluetoothUUID>* uuids) {
  auto it = uuids->begin();
  while (it != uuids->end()) {
    if (IsExcluded(*it)) {
      it = uuids->erase(it);
    } else {
      it++;
    }
  }
}

void BluetoothBlacklist::ResetToDefaultValuesForTest() {
  blacklisted_uuids_.clear();
  PopulateWithDefaultValues();
}

BluetoothBlacklist::BluetoothBlacklist() {
  PopulateWithDefaultValues();
}

void BluetoothBlacklist::PopulateWithDefaultValues() {
  blacklisted_uuids_.clear();

  // Blacklist UUIDs updated 2016-01-30 from:
  // https://github.com/WebBluetoothCG/registries/blob/master/gatt_blacklist.txt
  // Short UUIDs are used for readability of this list.
  DCHECK(BluetoothUUID("00001800-0000-1000-8000-00805f9b34fb") ==
         BluetoothUUID("1800"));
  // ## Services
  AddOrDie(BluetoothUUID("1800"), Value::EXCLUDE);
  AddOrDie(BluetoothUUID("1801"), Value::EXCLUDE);
  AddOrDie(BluetoothUUID("1812"), Value::EXCLUDE);
  // ## Characteristics
  AddOrDie(BluetoothUUID("2a25"), Value::EXCLUDE);
  // ## Descriptors
  AddOrDie(BluetoothUUID("2902"), Value::EXCLUDE_WRITES);
  AddOrDie(BluetoothUUID("2903"), Value::EXCLUDE_WRITES);
}

}  // namespace content
