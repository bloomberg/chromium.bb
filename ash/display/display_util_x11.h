// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_UTIL_X11_H_
#define ASH_DISPLAY_DISPLAY_UTIL_X11_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/display/display_info.h"

struct _XRRScreenResources;
typedef _XRRScreenResources XRRScreenResources;
struct _XRROutputInfo;
typedef _XRROutputInfo XRROutputInfo;

namespace ash {
namespace internal {
struct Resolution;

// Returns true if the size info in the output_info isn't valid
// and should be ignored. This is exposed for testing.
// |mm_width| and |mm_height| are given in millimeters.
ASH_EXPORT bool ShouldIgnoreSize(unsigned long mm_width,
                                 unsigned long mm_height);

// Returns the resolution list.
ASH_EXPORT std::vector<Resolution> GetResolutionList(
    XRRScreenResources* screen_resources,
    XRROutputInfo* output_info);

}  // namespace internal
}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_UTIL_X11_H_
