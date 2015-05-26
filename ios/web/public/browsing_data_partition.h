// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_BROWSING_DATA_PARTITION_H_
#define IOS_WEB_PUBLIC_BROWSING_DATA_PARTITION_H_

#include "base/macros.h"

#if defined(__OBJC__)
@class CRWBrowsingDataStore;
#else   // __OBJC__
class CRWBrowsingDataStore;
#endif  // __OBJC__

namespace web {

class BrowserState;

// Manages the browsing data associated with a particular BrowserState.
// Not thread safe. Must be used only on the main thread.
class BrowsingDataPartition {
 public:
  // Returns the underlying CRWBrowsingDataStore.
  virtual CRWBrowsingDataStore* GetBrowsingDataStore() = 0;

 protected:
  virtual ~BrowsingDataPartition(){};
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_BROWSING_DATA_PARTITION_H_
