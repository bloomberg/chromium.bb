// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/pref_names.h"

namespace metrics {
namespace prefs {

// A serialized PermutedEntropyCache protobuf, used as a cache to avoid
// recomputing permutations.
const char kVariationsPermutedEntropyCache[] =
    "user_experience_metrics.permuted_entropy_cache";

}  // namespace prefs
}  // namespace metrics
