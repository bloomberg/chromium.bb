// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_DEVICE_TASK_HELPER_MAP_SERVICE_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_DEVICE_TASK_HELPER_MAP_SERVICE_H_

#include <map>
#include <string>

#include "base/lazy_instance.h"

class MTPDeviceTaskHelper;

// MTPDeviceTaskHelperMapService manages MTPDeviceTaskHelper objects.
// MTPDeviceTaskHelperMapService lives on the UI thread.
class MTPDeviceTaskHelperMapService {
 public:
  static MTPDeviceTaskHelperMapService* GetInstance();

  // Creates and returns the MTPDeviceTaskHelper object for the storage device
  // specified by the |storage_name|.
  MTPDeviceTaskHelper* CreateDeviceTaskHelper(const std::string& storage_name);

  // Destroys the MTPDeviceTaskHelper object created by
  // CreateDeviceTaskHelper().
  // |storage_name| specifies the name of the storage device.
  void DestroyDeviceTaskHelper(const std::string& storage_name);

  // Gets the MTPDeviceTaskHelper object associated with the device storage.
  // |storage_name| specifies the name of the storage device.
  // Return NULL if the |storage_name| is no longer valid (e.g. because the
  // corresponding storage device is detached, etc).
  MTPDeviceTaskHelper* GetDeviceTaskHelper(const std::string& storage_name);

 private:
  friend struct base::DefaultLazyInstanceTraits<MTPDeviceTaskHelperMapService>;

  // Key: Storage name.
  // Value: MTPDeviceTaskHelper object.
  typedef std::map<std::string, MTPDeviceTaskHelper*> TaskHelperMap;

  // Get access to this class using GetInstance() method.
  MTPDeviceTaskHelperMapService();
  ~MTPDeviceTaskHelperMapService();

  // Mapping of |storage_name| and MTPDeviceTaskHelper*.
  // TaskHelperMap owns MTPDeviceTaskHelper objects.
  TaskHelperMap task_helper_map_;

  DISALLOW_COPY_AND_ASSIGN(MTPDeviceTaskHelperMapService);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_LINUX_MTP_DEVICE_TASK_HELPER_MAP_SERVICE_H_
