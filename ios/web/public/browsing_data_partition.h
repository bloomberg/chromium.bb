// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_BROWSING_DATA_PARTITION_H_
#define IOS_WEB_PUBLIC_BROWSING_DATA_PARTITION_H_

#include "base/macros.h"

@class CRWBrowsingDataStore;

namespace web {

class BrowserState;

// Manages the browsing data associated with a particular BrowserState.
// Not thread safe. Must be used only on the main thread.
class BrowsingDataPartition {
 public:
  // Returns true if the BrowsingDataParition is synchronized.
  // There is a moment of time when an ActiveStateManager's active state is out
  // of sync with its associated CRWBrowsingDataStore's mode, this method's
  // return value reflects that.
  // If the BrowsingDataPartition is synchronized, every CRWBrowsingDataStore's
  // mode matches its associated ActiveStateManager's active state.
  // If BrowsingDataPartition is not synchronized, this means that a
  // CRWBrowsingDataStore's mode is in the process of changing to its associated
  // ActiveStateManager's active state.
  static bool IsSynchronized();

  // Returns the underlying CRWBrowsingDataStore.
  virtual CRWBrowsingDataStore* GetBrowsingDataStore() = 0;

 protected:
  virtual ~BrowsingDataPartition() {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_BROWSING_DATA_PARTITION_H_
