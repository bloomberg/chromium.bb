// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webfallbackthemeengine_impl.h"

#include "base/macros.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_size.h"
#include "ui/native_theme/native_theme_base.h"

using blink::WebCanvas;
using blink::WebColor;
using blink::WebRect;
using blink::WebFallbackThemeEngine;

namespace content {

class WebFallbackThemeEngineImpl::WebFallbackNativeTheme
    : public ui::NativeThemeBase {
 public:
  WebFallbackNativeTheme() {}
  ~WebFallbackNativeTheme() override {}

  // NativeTheme:
  SkColor GetSystemColor(ColorId color_id) const override {
    // The paint routines in NativeThemeBase only use GetSystemColor for
    // button focus colors and the fallback theme is not used for buttons.
    NOTREACHED();
    return SK_ColorRED;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebFallbackNativeTheme);
};

static ui::NativeTheme::Part NativeThemePart(
    WebFallbackThemeEngine::Part part) {
  switch (part) {
    case WebFallbackThemeEngine::kPartScrollbarDownArrow:
      return ui::NativeTheme::kScrollbarDownArrow;
    case WebFallbackThemeEngine::kPartScrollbarLeftArrow:
      return ui::NativeTheme::kScrollbarLeftArrow;
    case WebFallbackThemeEngine::kPartScrollbarRightArrow:
      return ui::NativeTheme::kScrollbarRightArrow;
    case WebFallbackThemeEngine::kPartScrollbarUpArrow:
      return ui::NativeTheme::kScrollbarUpArrow;
    case WebFallbackThemeEngine::kPartScrollbarHorizontalThumb:
      return ui::NativeTheme::kScrollbarHorizontalThumb;
    case WebFallbackThemeEngine::kPartScrollbarVerticalThumb:
      return ui::NativeTheme::kScrollbarVerticalThumb;
    case WebFallbackThemeEngine::kPartScrollbarHorizontalTrack:
      return ui::NativeTheme::kScrollbarHorizontalTrack;
    case WebFallbackThemeEngine::kPartScrollbarVerticalTrack:
      return ui::NativeTheme::kScrollbarVerticalTrack;
    case WebFallbackThemeEngine::kPartScrollbarCorner:
      return ui::NativeTheme::kScrollbarCorner;
    case WebFallbackThemeEngine::kPartCheckbox:
      return ui::NativeTheme::kCheckbox;
    case WebFallbackThemeEngine::kPartRadio:
      return ui::NativeTheme::kRadio;
    case WebFallbackThemeEngine::kPartButton:
      return ui::NativeTheme::kPushButton;
    case WebFallbackThemeEngine::kPartTextField:
      return ui::NativeTheme::kTextField;
    case WebFallbackThemeEngine::kPartMenuList:
      return ui::NativeTheme::kMenuList;
    case WebFallbackThemeEngine::kPartSliderTrack:
      return ui::NativeTheme::kSliderTrack;
    case WebFallbackThemeEngine::kPartSliderThumb:
      return ui::NativeTheme::kSliderThumb;
    case WebFallbackThemeEngine::kPartInnerSpinButton:
      return ui::NativeTheme::kInnerSpinButton;
    case WebFallbackThemeEngine::kPartProgressBar:
      return ui::NativeTheme::kProgressBar;
    default:
      return ui::NativeTheme::kScrollbarDownArrow;
  }
}

static ui::NativeTheme::State NativeThemeState(
    WebFallbackThemeEngine::State state) {
  switch (state) {
    case WebFallbackThemeEngine::kStateDisabled:
      return ui::NativeTheme::kDisabled;
    case WebFallbackThemeEngine::kStateHover:
      return ui::NativeTheme::kHovered;
    case WebFallbackThemeEngine::kStateNormal:
      return ui::NativeTheme::kNormal;
    case WebFallbackThemeEngine::kStatePressed:
      return ui::NativeTheme::kPressed;
    default:
      return ui::NativeTheme::kDisabled;
  }
}

static void GetNativeThemeExtraParams(
    WebFallbackThemeEngine::Part part,
    WebFallbackThemeEngine::State state,
    const WebFallbackThemeEngine::ExtraParams* extra_params,
    ui::NativeTheme::ExtraParams* native_theme_extra_params) {
  switch (part) {
    case WebFallbackThemeEngine::kPartScrollbarHorizontalTrack:
    case WebFallbackThemeEngine::kPartScrollbarVerticalTrack:
      native_theme_extra_params->scrollbar_track.track_x =
          extra_params->scrollbar_track.track_x;
      native_theme_extra_params->scrollbar_track.track_y =
          extra_params->scrollbar_track.track_y;
      native_theme_extra_params->scrollbar_track.track_width =
          extra_params->scrollbar_track.track_width;
      native_theme_extra_params->scrollbar_track.track_height =
          extra_params->scrollbar_track.track_height;
      break;
    case WebFallbackThemeEngine::kPartCheckbox:
      native_theme_extra_params->button.checked = extra_params->button.checked;
      native_theme_extra_params->button.indeterminate =
          extra_params->button.indeterminate;
      break;
    case WebFallbackThemeEngine::kPartRadio:
      native_theme_extra_params->button.checked = extra_params->button.checked;
      break;
    case WebFallbackThemeEngine::kPartButton:
      native_theme_extra_params->button.is_default =
          extra_params->button.is_default;
      native_theme_extra_params->button.has_border =
          extra_params->button.has_border;
      // Native buttons have a different focus style.
      native_theme_extra_params->button.is_focused = false;
      native_theme_extra_params->button.background_color =
          extra_params->button.background_color;
      break;
    case WebFallbackThemeEngine::kPartTextField:
      native_theme_extra_params->text_field.is_text_area =
          extra_params->text_field.is_text_area;
      native_theme_extra_params->text_field.is_listbox =
          extra_params->text_field.is_listbox;
      native_theme_extra_params->text_field.background_color =
          extra_params->text_field.background_color;
      break;
    case WebFallbackThemeEngine::kPartMenuList:
      native_theme_extra_params->menu_list.has_border =
          extra_params->menu_list.has_border;
      native_theme_extra_params->menu_list.has_border_radius =
          extra_params->menu_list.has_border_radius;
      native_theme_extra_params->menu_list.arrow_x =
          extra_params->menu_list.arrow_x;
      native_theme_extra_params->menu_list.arrow_y =
          extra_params->menu_list.arrow_y;
      native_theme_extra_params->menu_list.arrow_size =
          extra_params->menu_list.arrow_size;
      native_theme_extra_params->menu_list.arrow_color =
          extra_params->menu_list.arrow_color;
      native_theme_extra_params->menu_list.background_color =
          extra_params->menu_list.background_color;
      break;
    case WebFallbackThemeEngine::kPartSliderTrack:
    case WebFallbackThemeEngine::kPartSliderThumb:
      native_theme_extra_params->slider.vertical =
          extra_params->slider.vertical;
      native_theme_extra_params->slider.in_drag = extra_params->slider.in_drag;
      break;
    case WebFallbackThemeEngine::kPartInnerSpinButton:
      native_theme_extra_params->inner_spin.spin_up =
          extra_params->inner_spin.spin_up;
      native_theme_extra_params->inner_spin.read_only =
          extra_params->inner_spin.read_only;
      break;
    case WebFallbackThemeEngine::kPartProgressBar:
      native_theme_extra_params->progress_bar.determinate =
          extra_params->progress_bar.determinate;
      native_theme_extra_params->progress_bar.value_rect_x =
          extra_params->progress_bar.value_rect_x;
      native_theme_extra_params->progress_bar.value_rect_y =
          extra_params->progress_bar.value_rect_y;
      native_theme_extra_params->progress_bar.value_rect_width =
          extra_params->progress_bar.value_rect_width;
      native_theme_extra_params->progress_bar.value_rect_height =
          extra_params->progress_bar.value_rect_height;
      break;
    default:
      break;  // Parts that have no extra params get here.
  }
}

WebFallbackThemeEngineImpl::WebFallbackThemeEngineImpl()
    : theme_(new WebFallbackNativeTheme()) {}

WebFallbackThemeEngineImpl::~WebFallbackThemeEngineImpl() {}

blink::WebSize WebFallbackThemeEngineImpl::GetSize(
    WebFallbackThemeEngine::Part part) {
  ui::NativeTheme::ExtraParams extra;
  return theme_->GetPartSize(NativeThemePart(part),
                             ui::NativeTheme::kNormal,
                             extra);
}

void WebFallbackThemeEngineImpl::Paint(
    blink::WebCanvas* canvas,
    WebFallbackThemeEngine::Part part,
    WebFallbackThemeEngine::State state,
    const blink::WebRect& rect,
    const WebFallbackThemeEngine::ExtraParams* extra_params) {
  ui::NativeTheme::ExtraParams native_theme_extra_params;
  GetNativeThemeExtraParams(
      part, state, extra_params, &native_theme_extra_params);
  theme_->Paint(canvas,
                NativeThemePart(part),
                NativeThemeState(state),
                gfx::Rect(rect),
                native_theme_extra_params);
}

}  // namespace content
