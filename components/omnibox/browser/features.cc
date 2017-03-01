// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/features.h"

namespace features {

// Feature used for the Zero Suggest Redirect to Chrome Field Trial.
const base::Feature kZeroSuggestRedirectToChrome{
    "ZeroSuggestRedirectToChrome", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to swap the title and URL when providing zero suggest
// suggestions.
const base::Feature kZeroSuggestSwapTitleAndUrl{
    "ZeroSuggestSwapTitleAndUrl", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
