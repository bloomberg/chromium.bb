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
    case blink::kWebBlendModeNormal:
      return SkBlendMode::kSrcOver;
    case blink::kWebBlendModeMultiply:
      return SkBlendMode::kMultiply;
    case blink::kWebBlendModeScreen:
      return SkBlendMode::kScreen;
    case blink::kWebBlendModeOverlay:
      return SkBlendMode::kOverlay;
    case blink::kWebBlendModeDarken:
      return SkBlendMode::kDarken;
    case blink::kWebBlendModeLighten:
      return SkBlendMode::kLighten;
    case blink::kWebBlendModeColorDodge:
      return SkBlendMode::kColorDodge;
    case blink::kWebBlendModeColorBurn:
      return SkBlendMode::kColorBurn;
    case blink::kWebBlendModeHardLight:
      return SkBlendMode::kHardLight;
    case blink::kWebBlendModeSoftLight:
      return SkBlendMode::kSoftLight;
    case blink::kWebBlendModeDifference:
      return SkBlendMode::kDifference;
    case blink::kWebBlendModeExclusion:
      return SkBlendMode::kExclusion;
    case blink::kWebBlendModeHue:
      return SkBlendMode::kHue;
    case blink::kWebBlendModeSaturation:
      return SkBlendMode::kSaturation;
    case blink::kWebBlendModeColor:
      return SkBlendMode::kColor;
    case blink::kWebBlendModeLuminosity:
      return SkBlendMode::kLuminosity;
  }
  return SkBlendMode::kSrcOver;
}

inline blink::WebBlendMode BlendModeFromSkia(SkBlendMode blend_mode) {
  switch (blend_mode) {
    case SkBlendMode::kSrcOver:
      return blink::kWebBlendModeNormal;
    case SkBlendMode::kMultiply:
      return blink::kWebBlendModeMultiply;
    case SkBlendMode::kScreen:
      return blink::kWebBlendModeScreen;
    case SkBlendMode::kOverlay:
      return blink::kWebBlendModeOverlay;
    case SkBlendMode::kDarken:
      return blink::kWebBlendModeDarken;
    case SkBlendMode::kLighten:
      return blink::kWebBlendModeLighten;
    case SkBlendMode::kColorDodge:
      return blink::kWebBlendModeColorDodge;
    case SkBlendMode::kColorBurn:
      return blink::kWebBlendModeColorBurn;
    case SkBlendMode::kHardLight:
      return blink::kWebBlendModeHardLight;
    case SkBlendMode::kSoftLight:
      return blink::kWebBlendModeSoftLight;
    case SkBlendMode::kDifference:
      return blink::kWebBlendModeDifference;
    case SkBlendMode::kExclusion:
      return blink::kWebBlendModeExclusion;
    case SkBlendMode::kHue:
      return blink::kWebBlendModeHue;
    case SkBlendMode::kSaturation:
      return blink::kWebBlendModeSaturation;
    case SkBlendMode::kColor:
      return blink::kWebBlendModeColor;
    case SkBlendMode::kLuminosity:
      return blink::kWebBlendModeLuminosity;

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
  return blink::kWebBlendModeNormal;
}

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_BLEND_MODE_H_
