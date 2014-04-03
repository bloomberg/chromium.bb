// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Use this file to assert that *_list.h enums that are meant to do the bridge
// from Blink are valid.

#include "base/macros.h"
#include "content/public/common/screen_orientation_values.h"
#include "third_party/WebKit/public/platform/WebScreenOrientation.h"

namespace content {

#define COMPILE_ASSERT_MATCHING_ENUM(expected, actual) \
  COMPILE_ASSERT(int(expected) == int(actual), mismatching_enums)

// TODO(mlamouri): this is temporary to allow to change the enum in Blink.
// COMPILE_ASSERT_MATCHING_ENUM(blink::WebScreenOrientationPortraitPrimary,
//     PORTRAIT_PRIMARY);
// COMPILE_ASSERT_MATCHING_ENUM(blink::WebScreenOrientationLandscapePrimary,
//     LANDSCAPE_PRIMARY);
// COMPILE_ASSERT_MATCHING_ENUM(blink::WebScreenOrientationPortraitSecondary,
//     PORTRAIT_SECONDARY);
// COMPILE_ASSERT_MATCHING_ENUM(blink::WebScreenOrientationLandscapeSecondary,
//     LANDSCAPE_SECONDARY);

} // namespace content
