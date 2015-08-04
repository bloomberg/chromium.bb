// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_FEATURE_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_FEATURE_H_

#include "build/build_config.h"

#if defined(OS_ANDROID)

namespace offline_pages {

// Returns true if offline pages is enabled.
bool IsOfflinePagesEnabled();

}  // namespace offline_pages

#endif  // defined(OS_ANDROID)

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_FEATURE_H_
