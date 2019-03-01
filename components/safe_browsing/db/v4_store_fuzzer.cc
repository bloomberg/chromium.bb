// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/test/test_simple_task_runner.h"
#include "components/safe_browsing/db/v4_store.h"
#include "components/safe_browsing/db/v4_test_util.h"

namespace safe_browsing {

class V4StoreFuzzer {
 public:
  static int FuzzMergeUpdate(const uint8_t* data, size_t size) {
    // Empty update, not interesting.
    if (size == 0)
      return 0;

    size_t num_prefixes_first_half = size / (2 * kMinHashPrefixLength);
    size_t first_half_size = num_prefixes_first_half * kMinHashPrefixLength;

    std::string first_half(data, data + first_half_size);
    HashPrefixMap prefix_map_old;
    V4Store::AddUnlumpedHashes(kMinHashPrefixLength, first_half,
                               &prefix_map_old);

    std::string second_half(data + first_half_size, data + size);
    HashPrefixMap prefix_map_additions;
    V4Store::AddUnlumpedHashes(kMinHashPrefixLength, second_half,
                               &prefix_map_additions);

    auto store = std::make_unique<TestV4Store>(
        base::MakeRefCounted<base::TestSimpleTaskRunner>(), base::FilePath());
    google::protobuf::RepeatedField<google::protobuf::int32> raw_removals;
    std::string empty_checksum;
    store->MergeUpdate(prefix_map_old, prefix_map_additions, &raw_removals,
                       empty_checksum);

    return 0;
  }
};

}  // namespace safe_browsing

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return safe_browsing::V4StoreFuzzer::FuzzMergeUpdate(data, size);
}
