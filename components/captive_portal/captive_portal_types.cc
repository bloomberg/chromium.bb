// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/captive_portal/captive_portal_types.h"

#include "base/logging.h"

namespace captive_portal {

namespace {

const char* const kCaptivePortalResultNames[] = {
  "InternetConnected",
  "NoResponse",
  "BehindCaptivePortal",
  "NumCaptivePortalResults",
};
COMPILE_ASSERT(arraysize(kCaptivePortalResultNames) == RESULT_COUNT + 1,
               captive_portal_result_name_count_mismatch);

}  // namespace


std::string CaptivePortalResultToString(CaptivePortalResult result) {
  DCHECK_GE(result, 0);
  DCHECK_LT(static_cast<unsigned int>(result),
            arraysize(kCaptivePortalResultNames));
  return kCaptivePortalResultNames[result];
}

}  // namespace captive_portal
