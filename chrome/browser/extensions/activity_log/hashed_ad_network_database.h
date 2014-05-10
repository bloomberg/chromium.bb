// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_HASHED_AD_NETWORK_DATABASE_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_HASHED_AD_NETWORK_DATABASE_H_

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/activity_log/ad_network_database.h"

namespace base {
class RefCountedStaticMemory;
}

namespace extensions {

// The standard ("real") implementation of the AdNetworkDatabase, which stores
// a list of hashes of ad networks.
class HashedAdNetworkDatabase : public AdNetworkDatabase {
 public:
  explicit HashedAdNetworkDatabase(
      scoped_refptr<base::RefCountedStaticMemory> memory);
  virtual ~HashedAdNetworkDatabase();

 private:
  // AdNetworkDatabase implementation.
  virtual bool IsAdNetwork(const GURL& url) const OVERRIDE;

  // The set of partial hashes for known ad networks.
  base::hash_set<int64> entries_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_HASHED_AD_NETWORK_DATABASE_H_
