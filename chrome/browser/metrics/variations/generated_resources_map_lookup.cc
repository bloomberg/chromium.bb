// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/generated_resources_map.h"

#include <algorithm>

namespace chrome_variations {

int GetResourceIndex(uint32_t hash) {
  const uint32_t* kEnd = kResourceHashes + kNumResources;
  const uint32_t* element = std::lower_bound(kResourceHashes, kEnd, hash);

  if (element == kEnd || *element != hash)
    return -1;
  return kResourceIndices[element - kResourceHashes];
}

}  // namespace chrome_variations
