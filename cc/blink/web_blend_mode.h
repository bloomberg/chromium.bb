// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_BLEND_MODE_H_
#define CC_BLINK_WEB_BLEND_MODE_H_

#include "third_party/WebKit/public/platform/WebBlendMode.h"
#include "third_party/skia/include/core/SkBlendMode.h"

namespace cc_blink {

inline SkBlendMode BlendModeToSkia(blink::WebBlendMode blend_mode) {
  switch (blend_mode) {
    case blink::WebBlendMode::kNormal:
      return SkBlendMode::kSrcOver;
    case blink::WebBlendMode::kMultiply:
      return SkBlendMode::kMultiply;
    case blink::WebBlendMode::kScreen:
      return SkBlendMode::kScreen;
    case blink::WebBlendMode::kOverlay:
      return SkBlendMode::kOverlay;
    case blink::WebBlendMode::kDarken:
      return SkBlendMode::kDarken;
    case blink::WebBlendMode::kLighten:
      return SkBlendMode::kLighten;
    case blink::WebBlendMode::kColorDodge:
      return SkBlendMode::kColorDodge;
    case blink::WebBlendMode::kColorBurn:
      return SkBlendMode::kColorBurn;
    case blink::WebBlendMode::kHardLight:
      return SkBlendMode::kHardLight;
    case blink::WebBlendMode::kSoftLight:
      return SkBlendMode::kSoftLight;
    case blink::WebBlendMode::kDifference:
      return SkBlendMode::kDifference;
    case blink::WebBlendMode::kExclusion:
      return SkBlendMode::kExclusion;
    case blink::WebBlendMode::kHue:
      return SkBlendMode::kHue;
    case blink::WebBlendMode::kSaturation:
      return SkBlendMode::kSaturation;
    case blink::WebBlendMode::kColor:
      return SkBlendMode::kColor;
    case blink::WebBlendMode::kLuminosity:
      return SkBlendMode::kLuminosity;
  }
  return SkBlendMode::kSrcOver;
}

inline blink::WebBlendMode BlendModeFromSkia(SkBlendMode blend_mode) {
  switch (blend_mode) {
    case SkBlendMode::kSrcOver:
      return blink::WebBlendMode::kNormal;
    case SkBlendMode::kMultiply:
      return blink::WebBlendMode::kMultiply;
    case SkBlendMode::kScreen:
      return blink::WebBlendMode::kScreen;
    case SkBlendMode::kOverlay:
      return blink::WebBlendMode::kOverlay;
    case SkBlendMode::kDarken:
      return blink::WebBlendMode::kDarken;
    case SkBlendMode::kLighten:
      return blink::WebBlendMode::kLighten;
    case SkBlendMode::kColorDodge:
      return blink::WebBlendMode::kColorDodge;
    case SkBlendMode::kColorBurn:
      return blink::WebBlendMode::kColorBurn;
    case SkBlendMode::kHardLight:
      return blink::WebBlendMode::kHardLight;
    case SkBlendMode::kSoftLight:
      return blink::WebBlendMode::kSoftLight;
    case SkBlendMode::kDifference:
      return blink::WebBlendMode::kDifference;
    case SkBlendMode::kExclusion:
      return blink::WebBlendMode::kExclusion;
    case SkBlendMode::kHue:
      return blink::WebBlendMode::kHue;
    case SkBlendMode::kSaturation:
      return blink::WebBlendMode::kSaturation;
    case SkBlendMode::kColor:
      return blink::WebBlendMode::kColor;
    case SkBlendMode::kLuminosity:
      return blink::WebBlendMode::kLuminosity;

    // these value are SkBlendModes, but no blend modes.
    case SkBlendMode::kClear:
    case SkBlendMode::kSrc:
    case SkBlendMode::kDst:
    case SkBlendMode::kDstOver:
    case SkBlendMode::kSrcIn:
    case SkBlendMode::kDstIn:
    case SkBlendMode::kSrcOut:
    case SkBlendMode::kDstOut:
    case SkBlendMode::kSrcATop:
    case SkBlendMode::kDstATop:
    case SkBlendMode::kXor:
    case SkBlendMode::kPlus:
    case SkBlendMode::kModulate:
      NOTREACHED();
  }
  return blink::WebBlendMode::kNormal;
}

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_BLEND_MODE_H_
