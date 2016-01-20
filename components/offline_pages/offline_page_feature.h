// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_FEATURE_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_FEATURE_H_

#include "build/build_config.h"

#if defined(OS_ANDROID)

namespace offline_pages {

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

}  // namespace offline_pages

#endif  // defined(OS_ANDROID)

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_FEATURE_H_
