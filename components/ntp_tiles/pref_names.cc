// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/pref_names.h"

namespace ntp_tiles {
namespace prefs {

// Ordered list of website suggestions shown on the new tab page that will allow
// retaining the order even if the suggestions change over time.
const char kNTPSuggestionsURL[] = "ntp.suggestions_url";

// Whether the suggestion was derived from personal data.
const char kNTPSuggestionsIsPersonal[] = "ntp.suggestions_is_personal";

}  // namespace prefs
}  // namespace ntp_tiles
