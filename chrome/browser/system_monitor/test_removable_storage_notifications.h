// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_TEST_REMOVABLE_STORAGE_NOTIFICATIONS_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_TEST_REMOVABLE_STORAGE_NOTIFICATIONS_H_

#include "chrome/browser/system_monitor/removable_storage_notifications.h"

namespace chrome {
namespace test {

// Needed to set up the RemovableStorageNotifications singleton (done in
// the base class).
class TestRemovableStorageNotifications
    : public chrome::RemovableStorageNotifications {
 public:
  TestRemovableStorageNotifications();
  virtual ~TestRemovableStorageNotifications();

  virtual bool GetDeviceInfoForPath(
      const base::FilePath& path,
      StorageInfo* device_info) const OVERRIDE;

  virtual uint64 GetStorageSize(const std::string& location) const OVERRIDE;

#if defined(OS_WIN)
  virtual bool GetMTPStorageInfoFromDeviceId(
      const std::string& storage_device_id,
      string16* device_location,
      string16* storage_object_id) const OVERRIDE;
#endif

  // TODO(gbillock): Update tests to use receiver and
  // get rid of ProcessAttach/ProcessDetach here.
  void ProcessAttach(const std::string& id,
                     const string16& name,
                     const FilePath::StringType& location);

  void ProcessDetach(const std::string& id);

  virtual Receiver* receiver() const OVERRIDE;
};

}  // namespace test
}  // namespace chrome

#endif  // CHROME_BROWSER_SYSTEM_MONITOR_TEST_REMOVABLE_STORAGE_NOTIFICATIONS_H_
