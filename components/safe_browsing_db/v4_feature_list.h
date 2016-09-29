// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_FEATURE_LIST_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_FEATURE_LIST_H_

namespace safe_browsing {

// Exposes methods to check whether a particular feature has been enabled
// through Finch.
namespace V4FeatureList {

// Is the Pver4 database manager enabled?
bool IsLocalDatabaseManagerEnabled();

// Is the Pver4 database manager doing resource checks in paralled with PVer3?
bool IsParallelCheckEnabled();

}  // namespace V4FeatureList

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_FEATURE_LIST_H_
