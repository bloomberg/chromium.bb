// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/renderer_preferences_util.h"

#include "base/no_destructor.h"
#include "content/public/common/renderer_preferences.h"
#include "ui/gfx/font_render_params.h"

namespace content {

void UpdateFontRendererPreferencesFromSystemSettings(
    content::RendererPreferences* prefs) {
  static const base::NoDestructor<gfx::FontRenderParams> params(
      gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(), nullptr));
  prefs->should_antialias_text = params->antialiasing;
  prefs->use_subpixel_positioning = params->subpixel_positioning;
  prefs->hinting = params->hinting;
  prefs->use_autohinter = params->autohinter;
  prefs->use_bitmaps = params->use_bitmaps;
  prefs->subpixel_rendering = params->subpixel_rendering;
}

}  // namespace content
