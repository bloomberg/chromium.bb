// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl.h"

#include "content/public/common/renderer_preferences.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/linux/WebFontInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/linux/WebFontRendering.h"

using WebKit::WebFontInfo;
using WebKit::WebFontRendering;

static SkPaint::Hinting RendererPreferencesToSkiaHinting(
    const content::RendererPreferences& prefs) {
  if (!prefs.should_antialias_text) {
    // When anti-aliasing is off, GTK maps all non-zero hinting settings to
    // 'Normal' hinting so we do the same. Otherwise, folks who have 'Slight'
    // hinting selected will see readable text in everything expect Chromium.
    switch (prefs.hinting) {
      case content::RENDERER_PREFERENCES_HINTING_NONE:
        return SkPaint::kNo_Hinting;
      case content::RENDERER_PREFERENCES_HINTING_SYSTEM_DEFAULT:
      case content::RENDERER_PREFERENCES_HINTING_SLIGHT:
      case content::RENDERER_PREFERENCES_HINTING_MEDIUM:
      case content::RENDERER_PREFERENCES_HINTING_FULL:
        return SkPaint::kNormal_Hinting;
      default:
        NOTREACHED();
        return SkPaint::kNormal_Hinting;
    }
  }

  switch (prefs.hinting) {
  case content::RENDERER_PREFERENCES_HINTING_SYSTEM_DEFAULT:
    return SkPaint::kNormal_Hinting;
  case content::RENDERER_PREFERENCES_HINTING_NONE:
    return SkPaint::kNo_Hinting;
  case content::RENDERER_PREFERENCES_HINTING_SLIGHT:
    return SkPaint::kSlight_Hinting;
  case content::RENDERER_PREFERENCES_HINTING_MEDIUM:
    return SkPaint::kNormal_Hinting;
  case content::RENDERER_PREFERENCES_HINTING_FULL:
    return SkPaint::kFull_Hinting;
  default:
    NOTREACHED();
    return SkPaint::kNormal_Hinting;
  }
}

static SkFontHost::LCDOrder RendererPreferencesToSkiaLCDOrder(
    content::RendererPreferencesSubpixelRenderingEnum subpixel) {
  switch (subpixel) {
  case content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_SYSTEM_DEFAULT:
  case content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_NONE:
  case content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_RGB:
  case content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VRGB:
    return SkFontHost::kRGB_LCDOrder;
  case content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_BGR:
  case content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VBGR:
    return SkFontHost::kBGR_LCDOrder;
  default:
    NOTREACHED();
    return SkFontHost::kRGB_LCDOrder;
  }
}

static SkFontHost::LCDOrientation
    RendererPreferencesToSkiaLCDOrientation(
        content::RendererPreferencesSubpixelRenderingEnum subpixel) {
  switch (subpixel) {
  case content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_SYSTEM_DEFAULT:
  case content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_NONE:
  case content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_RGB:
  case content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_BGR:
    return SkFontHost::kHorizontal_LCDOrientation;
  case content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VRGB:
  case content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VBGR:
    return SkFontHost::kVertical_LCDOrientation;
  default:
    NOTREACHED();
    return SkFontHost::kHorizontal_LCDOrientation;
  }
}

static bool RendererPreferencesToAntiAliasFlag(
    const content::RendererPreferences& prefs) {
  return prefs.should_antialias_text;
}

static bool RendererPreferencesToSubpixelRenderingFlag(
    const content::RendererPreferences& prefs) {
  if (prefs.subpixel_rendering !=
        content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_SYSTEM_DEFAULT &&
      prefs.subpixel_rendering !=
        content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_NONE) {
    return true;
  }
  return false;
}

void RenderViewImpl::UpdateFontRenderingFromRendererPrefs() {
  const content::RendererPreferences& prefs = renderer_preferences_;
  WebFontRendering::setHinting(RendererPreferencesToSkiaHinting(prefs));
  WebFontRendering::setLCDOrder(
      RendererPreferencesToSkiaLCDOrder(prefs.subpixel_rendering));
  WebFontRendering::setLCDOrientation(
      RendererPreferencesToSkiaLCDOrientation(prefs.subpixel_rendering));
  WebFontRendering::setAntiAlias(RendererPreferencesToAntiAliasFlag(prefs));
  WebFontRendering::setSubpixelRendering(
      RendererPreferencesToSubpixelRenderingFlag(prefs));
  WebFontRendering::setSubpixelPositioning(prefs.use_subpixel_positioning);
  WebFontInfo::setSubpixelPositioning(prefs.use_subpixel_positioning);
}
