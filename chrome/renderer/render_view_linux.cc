// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/render_view.h"

#include "chrome/common/renderer_preferences.h"
#include "third_party/WebKit/WebKit/chromium/public/linux/WebFontRendering.h"

using WebKit::WebFontRendering;

static SkPaint::Hinting RendererPreferencesToSkiaHinting(
    const RendererPreferences& prefs) {
  if (!prefs.should_antialias_text) {
    // When anti-aliasing is off, GTK maps all non-zero hinting settings to
    // 'Normal' hinting so we do the same. Otherwise, folks who have 'Slight'
    // hinting selected will see readable text in everything expect Chromium.
    switch (prefs.hinting) {
      case RENDERER_PREFERENCES_HINTING_NONE:
        return SkPaint::kNo_Hinting;
      case RENDERER_PREFERENCES_HINTING_SYSTEM_DEFAULT:
      case RENDERER_PREFERENCES_HINTING_SLIGHT:
      case RENDERER_PREFERENCES_HINTING_MEDIUM:
      case RENDERER_PREFERENCES_HINTING_FULL:
        return SkPaint::kNormal_Hinting;
      default:
        NOTREACHED();
        return SkPaint::kNormal_Hinting;
    }
  }

  switch (prefs.hinting) {
  case RENDERER_PREFERENCES_HINTING_SYSTEM_DEFAULT:
    return SkPaint::kNormal_Hinting;
  case RENDERER_PREFERENCES_HINTING_NONE:
    return SkPaint::kNo_Hinting;
  case RENDERER_PREFERENCES_HINTING_SLIGHT:
    return SkPaint::kSlight_Hinting;
  case RENDERER_PREFERENCES_HINTING_MEDIUM:
    return SkPaint::kNormal_Hinting;
  case RENDERER_PREFERENCES_HINTING_FULL:
    return SkPaint::kFull_Hinting;
  default:
    NOTREACHED();
    return SkPaint::kNormal_Hinting;
  }
}

static SkFontHost::LCDOrder RendererPreferencesToSkiaLCDOrder(
    RendererPreferencesSubpixelRenderingEnum subpixel) {
  switch (subpixel) {
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_SYSTEM_DEFAULT:
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_NONE:
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_RGB:
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VRGB:
    return SkFontHost::kRGB_LCDOrder;
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_BGR:
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VBGR:
    return SkFontHost::kBGR_LCDOrder;
  default:
    NOTREACHED();
    return SkFontHost::kRGB_LCDOrder;
  }
}

static SkFontHost::LCDOrientation
    RendererPreferencesToSkiaLCDOrientation(
        RendererPreferencesSubpixelRenderingEnum subpixel) {
  switch (subpixel) {
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_SYSTEM_DEFAULT:
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_NONE:
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_RGB:
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_BGR:
    return SkFontHost::kHorizontal_LCDOrientation;
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VRGB:
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VBGR:
    return SkFontHost::kVertical_LCDOrientation;
  default:
    NOTREACHED();
    return SkFontHost::kHorizontal_LCDOrientation;
  }
}

static bool RendererPreferencesToAntiAliasFlag(
    const RendererPreferences& prefs) {
  return prefs.should_antialias_text;
}

static bool RendererPreferencesToSubpixelGlyphsFlag(
    const RendererPreferences& prefs) {
  if (prefs.subpixel_rendering !=
        RENDERER_PREFERENCES_SUBPIXEL_RENDERING_SYSTEM_DEFAULT &&
      prefs.subpixel_rendering !=
        RENDERER_PREFERENCES_SUBPIXEL_RENDERING_NONE) {
    return true;
  }

  return false;
}

void RenderView::UpdateFontRenderingFromRendererPrefs() {
  const RendererPreferences& prefs = renderer_preferences_;
  WebFontRendering::setHinting(
      RendererPreferencesToSkiaHinting(prefs));
  WebFontRendering::setLCDOrder(
      RendererPreferencesToSkiaLCDOrder(prefs.subpixel_rendering));
  WebFontRendering::setLCDOrientation(
      RendererPreferencesToSkiaLCDOrientation(prefs.subpixel_rendering));
  WebFontRendering::setAntiAlias(RendererPreferencesToAntiAliasFlag(prefs));
  WebFontRendering::setSubpixelGlyphs(
      RendererPreferencesToSubpixelGlyphsFlag(prefs));
}
