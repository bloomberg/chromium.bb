// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_BLACKLIST_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_BLACKLIST_H_

#include <map>
#include <vector>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace content {

struct BluetoothScanFilter;

// Implements the Web Bluetooth Blacklist policy as defined in the Web Bluetooth
// specification:
// https://webbluetoothcg.github.io/web-bluetooth/#the-gatt-blacklist
//
// Client code may query UUIDs to determine if they are excluded from use by the
// blacklist.
//
// Singleton access via Get() enforces only one copy of blacklist.
class CONTENT_EXPORT BluetoothBlacklist final {
 public:
  // Blacklist value terminology from Web Bluetooth specification:
  // https://webbluetoothcg.github.io/web-bluetooth/#the-gatt-blacklist
  enum class Value {
    EXCLUDE,        // Implies EXCLUDE_READS and EXCLUDE_WRITES.
    EXCLUDE_READS,  // Excluded from read operations.
    EXCLUDE_WRITES  // Excluded from write operations.
  };

  ~BluetoothBlacklist();

  // Returns a singleton instance of the blacklist.
  static BluetoothBlacklist& Get();

  // Adds a UUID to the blacklist to be excluded from operations. Crash if the
  // UUID is already in the blacklist.
  void AddOrDie(const device::BluetoothUUID&, Value);

  // Returns if a UUID is excluded from all operations.
  bool IsExcluded(const device::BluetoothUUID&) const;

  // Returns if any UUID in a set of filters is excluded from all operations.
  bool IsExcluded(const std::vector<content::BluetoothScanFilter>&);

  // Returns if a UUID is excluded from read operations.
  bool IsExcludedFromReads(const device::BluetoothUUID&) const;

  // Returns if a UUID is excluded from write operations.
  bool IsExcludedFromWrites(const device::BluetoothUUID&) const;

  // Modifies a list of UUIDs, removing any UUIDs with Value::EXCLUDE.
  void RemoveExcludedUuids(std::vector<device::BluetoothUUID>*);

  // Size of blacklist.
  size_t size() { return blacklisted_uuids_.size(); }

  void ResetToDefaultValuesForTest();

 private:
  // friend LazyInstance to permit access to private constructor.
  friend base::DefaultLazyInstanceTraits<BluetoothBlacklist>;

  BluetoothBlacklist();

  void PopulateWithDefaultValues();

  // Map of UUID to blacklisted value.
  std::map<device::BluetoothUUID, Value> blacklisted_uuids_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothBlacklist);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_BLACKLIST_H_
