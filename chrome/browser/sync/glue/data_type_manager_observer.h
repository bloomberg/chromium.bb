// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_OBSERVER_H_

#include "chrome/browser/sync/glue/data_type_manager.h"

namespace browser_sync {

// Various data type configuration events can be consumed by observing the
// DataTypeManager through this interface.
class DataTypeManagerObserver {
 public:
  virtual void OnConfigureDone(
      const browser_sync::DataTypeManager::ConfigureResult& result) = 0;
  virtual void OnConfigureRetry() = 0;
  virtual void OnConfigureStart() = 0;

 protected:
  virtual ~DataTypeManagerObserver() { }
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_DATA_TYPE_MANAGER_OBSERVER_H_
