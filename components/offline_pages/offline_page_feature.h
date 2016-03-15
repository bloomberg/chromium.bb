// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_FEATURE_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_FEATURE_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace offline_pages {

extern const base::Feature kOffliningRecentPagesFeature;
extern const base::Feature kOfflinePagesBackgroundLoadingFeature;

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offlinepages
enum class FeatureMode {
  // Offline pages feature is disabled.
  DISABLED,
  // Offline pages feature is enabled, showing bookmarks in UI strings.
  ENABLED_AS_BOOKMARKS,
  // Offline pages feature is enabled, showing saved pages in UI strings.
  ENABLED_AS_SAVED_PAGES
};

// Returns the mode where Offline Pages feature is running.
FeatureMode GetOfflinePageFeatureMode();

// Returns true if offline pages is enabled.
bool IsOfflinePagesEnabled();

// Returns true if offlining of recent pages (aka 'Last N pages') is enabled.
bool IsOffliningRecentPagesEnabled();

// Returns true if saving offline pages in the background is enabled.
bool IsOfflinePagesBackgroundLoadingEnabled();

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_FEATURE_H_
