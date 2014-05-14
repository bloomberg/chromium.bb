// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_HASHED_AD_NETWORK_DATABASE_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_HASHED_AD_NETWORK_DATABASE_H_

#include "chrome/browser/extensions/activity_log/ad_network_database.h"

namespace extensions {

// The standard ("real") implementation of the AdNetworkDatabase, which stores
// a list of hashes of ad networks.
class HashedAdNetworkDatabase : public AdNetworkDatabase {
 public:
  HashedAdNetworkDatabase();
  virtual ~HashedAdNetworkDatabase();

  void set_entries_for_testing(const char** entries, int num_entries) {
    entries_ = entries;
    num_entries_ = num_entries;
  }

 private:
  // AdNetworkDatabase implementation.
  virtual bool IsAdNetwork(const GURL& url) const OVERRIDE;

  // Points to the array of hash entries. In practice, this is always set to
  // kHashedAdNetworks, but is exposed via set_entries_for_testing().
  const char** entries_;

  // The number of entries.
  int num_entries_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_HASHED_AD_NETWORK_DATABASE_H_
