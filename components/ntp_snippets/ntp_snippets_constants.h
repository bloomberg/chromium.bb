// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONSTANTS_H_
#define COMPONENTS_NTP_SNIPPETS_CONSTANTS_H_

#include "base/files/file_path.h"

namespace ntp_snippets {

// Name of the folder where the snippets database should be stored. This is only
// the name of the folder, not a full path - it must be appended to e.g. the
// profile path.
extern const base::FilePath::CharType kDatabaseFolder[];
// TODO(mamir): Check if the same DB can be used.
extern const base::FilePath::CharType kBreakingNewsDatabaseFolder[];

// Server endpoints for fetching snippets.
extern const char kContentSuggestionsServer[];         // used on stable/beta
extern const char kContentSuggestionsStagingServer[];  // used on dev/canary
extern const char kContentSuggestionsAlphaServer[];    // for testing

// Server endpoints for push updates subscription.
extern const char kPushUpdatesSubscriptionServer[];  // used on stable/beta
extern const char
    kPushUpdatesSubscriptionStagingServer[];              // used on dev/canary
extern const char kPushUpdatesSubscriptionAlphaServer[];  // for testing

// Server endpoints for push updates unsubscription.
extern const char kPushUpdatesUnsubscriptionServer[];  // used on stable/beta
extern const char
    kPushUpdatesUnsubscriptionStagingServer[];  // used on dev/canary
extern const char kPushUpdatesUnsubscriptionAlphaServer[];  // for testing

}  // namespace ntp_snippets

#endif
