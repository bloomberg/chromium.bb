// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/ad_injection_constants.h"

#include "base/strings/string_util.h"

namespace extensions {
namespace ad_injection_constants {

namespace keys {

const char kType[] = "type";
const char kChildren[] = "children";
const char kSrc[] = "src";
const char kHref[] = "href";

}  // namespace keys

const char kHtmlIframeSrcApiName[] = "HTMLIFrameElement.src";
const char kHtmlEmbedSrcApiName[] = "HTMLEmbedElement.src";
const char kAppendChildApiSuffix[] = "appendChild";

// The maximum number of children to check when we examine a newly-added
// element.
extern const size_t kMaximumChildrenToCheck = 10u;

// The maximum depth to check when we examine a newly-added element.
extern const size_t kMaximumDepthToCheck = 5u;

bool ApiCanInjectAds(const char* api) {
  return api == kHtmlIframeSrcApiName ||
         api == kHtmlEmbedSrcApiName ||
         EndsWith(api, kAppendChildApiSuffix, true /* case sensitive */);
}

}  // namespace ad_injection_constants
}  // namespace extensions
