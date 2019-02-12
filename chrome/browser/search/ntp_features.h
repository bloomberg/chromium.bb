// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_NTP_FEATURES_H_
#define CHROME_BROWSER_SEARCH_NTP_FEATURES_H_

#include "base/feature_list.h"

namespace features {

// The features should be documented alongside the definition of their values in
// the .cc file.

extern const base::Feature kDoodlesOnLocalNtp;
extern const base::Feature kPromosOnLocalNtp;
extern const base::Feature kRemoveNtpFakebox;
extern const base::Feature kSearchSuggestionsOnLocalNtp;
extern const base::Feature kUseGoogleLocalNtp;

}  // namespace features

#endif  // CHROME_BROWSER_SEARCH_NTP_FEATURES_H_
