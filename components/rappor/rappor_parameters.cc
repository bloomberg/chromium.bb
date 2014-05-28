// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/rappor_parameters.h"

#include "base/compiler_specific.h"
#include "base/strings/stringprintf.h"

namespace rappor {

std::string RapporParameters::ToString() const {
  return base::StringPrintf("{ %d, %d, %d, %d, %d, %d, %d }",
      num_cohorts,
      bloom_filter_size_bytes,
      bloom_filter_hash_function_count,
      fake_prob,
      fake_one_prob,
      one_coin_prob,
      zero_coin_prob);
}

const int RapporParameters::kMaxCohorts = 512;

}  // namespace rappor
