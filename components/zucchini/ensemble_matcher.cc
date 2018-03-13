// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/ensemble_matcher.h"

#include <limits>

#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace zucchini {

/******** EnsembleMatcher ********/

EnsembleMatcher::EnsembleMatcher() = default;

EnsembleMatcher::~EnsembleMatcher() = default;

void EnsembleMatcher::Trim() {
  // TODO(huangs): Add MultiDex handling logic when we add DEX support.
}

}  // namespace zucchini
