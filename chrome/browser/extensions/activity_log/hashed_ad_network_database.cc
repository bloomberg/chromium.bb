// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/hashed_ad_network_database.h"

#include <algorithm>  // std::binary_search
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/activity_log/hashed_ad_networks.h"
#include "crypto/sha2.h"
#include "url/gurl.h"

namespace extensions {

namespace {

typedef char shorthash[8];

bool CompareEntries(const char* entry1, const char* entry2) {
  return strcmp(entry1, entry2) < 0;
}

}

HashedAdNetworkDatabase::HashedAdNetworkDatabase()
    : entries_(kHashedAdNetworks),
      num_entries_(kNumHashedAdNetworks) {
}

HashedAdNetworkDatabase::~HashedAdNetworkDatabase() {
}

bool HashedAdNetworkDatabase::IsAdNetwork(const GURL& url) const {
// The list should be sorted. Check once in debug builds.
#if DCHECK_IS_ON
  static bool is_sorted = false;
  if (!is_sorted) {
    std::vector<std::string> list;
    for (int i = 0; i < num_entries_; ++i)
      list.push_back(std::string(entries_[i]));
    is_sorted = base::STLIsSorted(list);
  }
  DCHECK(is_sorted);
#endif

  shorthash hash;
  crypto::SHA256HashString(url.host(), hash, sizeof(shorthash));
  std::string hex_encoded = base::HexEncode(hash, sizeof(shorthash));
  return std::binary_search(entries_,
                            entries_ + num_entries_,
                            hex_encoded.c_str(),
                            CompareEntries);
}

}  // namespace extensions
