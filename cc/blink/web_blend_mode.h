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
    case blink::WebBlendModeNormal:
      return SkBlendMode::kSrcOver;
    case blink::WebBlendModeMultiply:
      return SkBlendMode::kMultiply;
    case blink::WebBlendModeScreen:
      return SkBlendMode::kScreen;
    case blink::WebBlendModeOverlay:
      return SkBlendMode::kOverlay;
    case blink::WebBlendModeDarken:
      return SkBlendMode::kDarken;
    case blink::WebBlendModeLighten:
      return SkBlendMode::kLighten;
    case blink::WebBlendModeColorDodge:
      return SkBlendMode::kColorDodge;
    case blink::WebBlendModeColorBurn:
      return SkBlendMode::kColorBurn;
    case blink::WebBlendModeHardLight:
      return SkBlendMode::kHardLight;
    case blink::WebBlendModeSoftLight:
      return SkBlendMode::kSoftLight;
    case blink::WebBlendModeDifference:
      return SkBlendMode::kDifference;
    case blink::WebBlendModeExclusion:
      return SkBlendMode::kExclusion;
    case blink::WebBlendModeHue:
      return SkBlendMode::kHue;
    case blink::WebBlendModeSaturation:
      return SkBlendMode::kSaturation;
    case blink::WebBlendModeColor:
      return SkBlendMode::kColor;
    case blink::WebBlendModeLuminosity:
      return SkBlendMode::kLuminosity;
  }
  return SkBlendMode::kSrcOver;
}

inline blink::WebBlendMode BlendModeFromSkia(SkBlendMode blend_mode) {
  switch (blend_mode) {
    case SkBlendMode::kSrcOver:
      return blink::WebBlendModeNormal;
    case SkBlendMode::kMultiply:
      return blink::WebBlendModeMultiply;
    case SkBlendMode::kScreen:
      return blink::WebBlendModeScreen;
    case SkBlendMode::kOverlay:
      return blink::WebBlendModeOverlay;
    case SkBlendMode::kDarken:
      return blink::WebBlendModeDarken;
    case SkBlendMode::kLighten:
      return blink::WebBlendModeLighten;
    case SkBlendMode::kColorDodge:
      return blink::WebBlendModeColorDodge;
    case SkBlendMode::kColorBurn:
      return blink::WebBlendModeColorBurn;
    case SkBlendMode::kHardLight:
      return blink::WebBlendModeHardLight;
    case SkBlendMode::kSoftLight:
      return blink::WebBlendModeSoftLight;
    case SkBlendMode::kDifference:
      return blink::WebBlendModeDifference;
    case SkBlendMode::kExclusion:
      return blink::WebBlendModeExclusion;
    case SkBlendMode::kHue:
      return blink::WebBlendModeHue;
    case SkBlendMode::kSaturation:
      return blink::WebBlendModeSaturation;
    case SkBlendMode::kColor:
      return blink::WebBlendModeColor;
    case SkBlendMode::kLuminosity:
      return blink::WebBlendModeLuminosity;

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
  return blink::WebBlendModeNormal;
}

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_BLEND_MODE_H_
