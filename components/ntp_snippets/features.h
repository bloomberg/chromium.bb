// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_FEATURES_H_
#define COMPONENTS_NTP_SNIPPETS_FEATURES_H_

#include "base/feature_list.h"

namespace ntp_snippets {

// Features to turn individual providers/categories on/off.
extern const base::Feature kArticleSuggestionsFeature;
extern const base::Feature kBookmarkSuggestionsFeature;
extern const base::Feature kRecentOfflineTabSuggestionsFeature;
extern const base::Feature kDownloadSuggestionsFeature;
extern const base::Feature kPhysicalWebPageSuggestionsFeature;

// Feature to allow the 'save to offline' option to appear in the snippets
// context menu.
extern const base::Feature kSaveToOfflineFeature;

// Global toggle for the whole content suggestions feature. If this is set to
// false, all the per-provider features are ignored.
extern const base::Feature kContentSuggestionsFeature;

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_FEATURES_H_
