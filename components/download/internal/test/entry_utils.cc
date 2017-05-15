// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "components/download/internal/test/entry_utils.h"

namespace download {
namespace test {

bool SuperficialEntryCompare(const Entry* const& expected,
                             const Entry* const& actual) {
  if (expected == nullptr || actual == nullptr)
    return expected == actual;

  return expected->client == actual->client && expected->guid == actual->guid &&
         expected->state == actual->state;
}

bool SuperficialEntryListCompare(const std::vector<Entry*>& expected,
                                 const std::vector<Entry*>& actual) {
  return std::is_permutation(actual.cbegin(), actual.cend(), expected.cbegin(),
                             SuperficialEntryCompare);
}

Entry BuildEntry(DownloadClient client, const std::string& guid) {
  Entry entry;
  entry.client = client;
  entry.guid = guid;
  return entry;
}

}  // namespace test
}  // namespace download
