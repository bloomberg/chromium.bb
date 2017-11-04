// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "build/build_config.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_features.h"
#include "chrome/browser/ui/views/feature_promos/new_tab_promo_bubble_view.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_impl.h"
#include "components/feature_engagement/features.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/effects/SkBlurMaskFilter.h"
#include "third_party/skia/include/effects/SkLayerDrawLooper.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/base/default_theme_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/display/win/screen_win.h"
#include "ui/gfx/win/hwnd_util.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker.h"
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker_factory.h"
#include "chrome/browser/ui/views/feature_promos/new_tab_promo_bubble_view.h"
#endif

namespace {

sk_sp<SkDrawLooper> CreateShadowDrawLooper(SkColor color) {
  SkLayerDrawLooper::Builder looper_builder;
  looper_builder.addLayer();

  SkLayerDrawLooper::LayerInfo layer_info;
  layer_info.fPaintBits |= SkLayerDrawLooper::kMaskFilter_Bit;
  layer_info.fPaintBits |= SkLayerDrawLooper::kColorFilter_Bit;
  layer_info.fColorMode = SkBlendMode::kDst;
  layer_info.fOffset.set(0, 1);
  SkPaint* layer_paint = looper_builder.addLayer(layer_info);
  layer_paint->setMaskFilter(SkBlurMaskFilter::Make(
      kNormal_SkBlurStyle, 0.5, SkBlurMaskFilter::kHighQuality_BlurFlag));
  layer_paint->setColorFilter(
      SkColorFilter::MakeModeFilter(color, SkBlendMode::kSrcIn));
  return looper_builder.detach();
}

}  // namespace

NewTabButton::NewTabButton(TabStripImpl* tab_strip,
                           views::ButtonListener* listener)
    : views::ImageButton(listener),
      tab_strip_(tab_strip),
      new_tab_promo_(nullptr),
      destroyed_(nullptr),
      new_tab_promo_observer_(this) {
  set_animate_on_state_change(true);
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  set_triggerable_event_flags(triggerable_event_flags() |
                              ui::EF_MIDDLE_MOUSE_BUTTON);
#endif
}

NewTabButton::~NewTabButton() {
  if (destroyed_)
    *destroyed_ = true;
}

// static
int NewTabButton::GetTopOffset() {
  // The vertical distance between the bottom of the new tab button and the
  // bottom of the tabstrip.
  const int kNewTabButtonBottomOffset = 4;
  return Tab::GetMinimumInactiveSize().height() - kNewTabButtonBottomOffset -
         GetLayoutSize(NEW_TAB_BUTTON).height();
}

// static
void NewTabButton::ShowPromoForLastActiveBrowser() {
  BrowserView* browser = static_cast<BrowserView*>(
      BrowserList::GetInstance()->GetLastActive()->window());

  // The promo depends on the new tab button which only exists on the
  // TabStripImpl.
  TabStripImpl* tab_strip_impl = browser->tabstrip()->AsTabStripImpl();
  if (tab_strip_impl)
    tab_strip_impl->new_tab_button()->ShowPromo();
}

// static
void NewTabButton::CloseBubbleForLastActiveBrowser() {
  BrowserView* browser = static_cast<BrowserView*>(
      BrowserList::GetInstance()->GetLastActive()->window());
  TabStripImpl* tab_strip_impl = browser->tabstrip()->AsTabStripImpl();
  if (tab_strip_impl)
    tab_strip_impl->new_tab_button()->CloseBubble();
}

void NewTabButton::ShowPromo() {
  DCHECK(!new_tab_promo_);
  // Owned by its native widget. Will be destroyed as its widget is destroyed.
  new_tab_promo_ = NewTabPromoBubbleView::CreateOwned(this);
  new_tab_promo_observer_.Add(new_tab_promo_->GetWidget());
  SchedulePaint();
}

void NewTabButton::CloseBubble() {
  if (new_tab_promo_)
    new_tab_promo_->CloseBubble();
}

#if defined(OS_WIN)
void NewTabButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyRightMouseButton()) {
    gfx::Point point = event.location();
    views::View::ConvertPointToScreen(this, &point);
    point = display::win::ScreenWin::DIPToScreenPoint(point);
    bool destroyed = false;
    destroyed_ = &destroyed;
    gfx::ShowSystemMenuAtPoint(views::HWNDForView(this), point);
    if (destroyed)
      return;

    destroyed_ = NULL;
    SetState(views::Button::STATE_NORMAL);
    return;
  }
  views::ImageButton::OnMouseReleased(event);
}
#endif

void NewTabButton::OnGestureEvent(ui::GestureEvent* event) {
  // Consume all gesture events here so that the parent (Tab) does not
  // start consuming gestures.
  views::ImageButton::OnGestureEvent(event);
  event->SetHandled();
}

void NewTabButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  const int visible_height = GetLayoutSize(NEW_TAB_BUTTON).height();
  canvas->Translate(gfx::Vector2d(0, height() - visible_height));

  const bool pressed = state() == views::Button::STATE_PRESSED;
  const float scale = canvas->image_scale();

  // Fill.
  SkPath fill;
  const float fill_bottom = (visible_height - 2) * scale;
  const float diag_height = fill_bottom - 3.5 * scale;
  const float diag_width = diag_height * Tab::GetInverseDiagonalSlope();
  fill.moveTo(diag_width + 4 * scale, fill_bottom);
  fill.rCubicTo(-0.75 * scale, 0, -1.625 * scale, -0.5 * scale, -2 * scale,
                -1.5 * scale);
  fill.rLineTo(-diag_width, -diag_height);
  fill.rCubicTo(0, -0.5 * scale, 0.25 * scale, -scale, scale, -scale);
  fill.lineTo((width() - 4) * scale - diag_width, scale);
  fill.rCubicTo(0.75 * scale, 0, 1.625 * scale, 0.5 * scale, 2 * scale,
                1.5 * scale);
  fill.rLineTo(diag_width, diag_height);
  fill.rCubicTo(0, 0.5 * scale, -0.25 * scale, scale, -scale, scale);
  fill.close();
  PaintFill(pressed, scale, fill, canvas);

  // Stroke.
  canvas->UndoDeviceScaleFactor();
  SkPath stroke;
  GetBorderPath(0, scale, false, &stroke);
  // We want to draw a drop shadow either inside or outside the stroke,
  // depending on whether we're pressed; so, either clip out what's outside
  // the stroke, or clip out the fill inside it.
  if (pressed)
    canvas->ClipPath(stroke, true);
  Op(stroke, fill, kDifference_SkPathOp, &stroke);
  if (!pressed)
    canvas->sk_canvas()->clipPath(fill, SkClipOp::kDifference, true);
  // Now draw the stroke and shadow; the stroke will always be visible, while
  // the shadow will be affected by the clip we set above.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  const SkColor stroke_color =
      new_tab_promo_observer_.IsObservingSources()
          ? color_utils::AlphaBlend(
                SK_ColorBLACK,
                GetNativeTheme()->GetSystemColor(
                    ui::NativeTheme::kColorId_ProminentButtonColor),
                0x70)
          : tab_strip_->GetToolbarTopSeparatorColor();
  const float alpha = SkColorGetA(stroke_color);
  const SkAlpha shadow_alpha =
      base::saturated_cast<SkAlpha>(std::round(2.1875f * alpha));
  flags.setLooper(
      CreateShadowDrawLooper(SkColorSetA(stroke_color, shadow_alpha)));
  const SkAlpha path_alpha =
      static_cast<SkAlpha>(std::round((pressed ? 0.875f : 0.609375f) * alpha));
  const SkColor color = SkColorSetA(stroke_color, path_alpha);
  flags.setColor(color);
  canvas->DrawPath(stroke, flags);
}

bool NewTabButton::GetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);

  SkPath border;
  const float scale = GetWidget()->GetCompositor()->device_scale_factor();
  GetBorderPath(GetTopOffset() * scale, scale,
                tab_strip_->SizeTabButtonToTopOfTabStrip(), &border);
  mask->addPath(border, SkMatrix::MakeScale(1 / scale));
  return true;
}

void NewTabButton::OnWidgetDestroying(views::Widget* widget) {
#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
  feature_engagement::NewTabTrackerFactory::GetInstance()
      ->GetForProfile(tab_strip_->controller()->GetProfile())
      ->OnPromoClosed();
#endif
  new_tab_promo_observer_.Remove(widget);
  new_tab_promo_ = nullptr;
  // When the promo widget is destroyed, the NewTabButton needs to be
  // recolored.
  SchedulePaint();
}

gfx::Rect NewTabButton::GetVisibleBounds() {
  const float scale = GetWidget()->GetCompositor()->device_scale_factor();
  SkPath border;
  GetBorderPath(GetTopOffset() * scale, scale, false, &border);
  gfx::Rect rect = gfx::ToEnclosingRect(gfx::SkRectToRectF(border.getBounds()));
  ConvertRectToScreen(this, &rect);
  return rect;
}

void NewTabButton::GetBorderPath(float button_y,
                                 float scale,
                                 bool extend_to_top,
                                 SkPath* path) const {
  const float inverse_slope = Tab::GetInverseDiagonalSlope();
  const float fill_bottom =
      (GetLayoutSize(NEW_TAB_BUTTON).height() - 2) * scale;
  const float stroke_bottom = button_y + fill_bottom + 1;
  const float diag_height = fill_bottom - 3.5 * scale;
  const float diag_width = diag_height * inverse_slope;
  path->moveTo(diag_width + 4 * scale - 1, stroke_bottom);
  path->rCubicTo(-0.75 * scale, 0, -1.625 * scale, -0.5 * scale, -2 * scale,
                 -1.5 * scale);
  path->rLineTo(-diag_width, -diag_height);
  if (extend_to_top) {
    // Create the vertical extension by extending the side diagonals at the
    // upper left and lower right corners until they reach the top and bottom of
    // the border, respectively (in other words, "un-round-off" those corners
    // and turn them into sharp points).  Then extend upward from the corner
    // points to the top of the bounds.
    const float dy = scale + 2;
    const float dx = inverse_slope * dy;
    path->rLineTo(-dx, -dy);
    path->rLineTo(0, -button_y - scale + 1);
    path->lineTo((width() - 2) * scale + 1 + dx, 0);
    path->rLineTo(0, stroke_bottom);
  } else {
    path->rCubicTo(-0.5 * scale, -1.125 * scale, 0.5 * scale, -scale - 2, scale,
                   -scale - 2);
    path->lineTo((width() - 4) * scale - diag_width + 1, button_y + scale - 1);
    path->rCubicTo(0.75 * scale, 0, 1.625 * scale, 0.5 * scale, 2 * scale,
                   1.5 * scale);
    path->rLineTo(diag_width, diag_height);
    path->rCubicTo(0.5 * scale, 1.125 * scale, -0.5 * scale, scale + 2, -scale,
                   scale + 2);
  }
  path->close();
}

void NewTabButton::PaintFill(bool pressed,
                             float scale,
                             const SkPath& fill,
                             gfx::Canvas* canvas) const {
  gfx::ScopedCanvas scoped_canvas(canvas);
  canvas->UndoDeviceScaleFactor();
  cc::PaintFlags flags;
  flags.setAntiAlias(true);

  // For unpressed buttons, draw the fill and its shadow.
  if (!pressed) {
    // First we compute the background image coordinates and scale, in case we
    // need to draw a custom background image.
    const ui::ThemeProvider* tp = GetThemeProvider();
    bool custom_image;
    const int bg_id = tab_strip_->GetBackgroundResourceId(&custom_image);
    if (custom_image && !new_tab_promo_observer_.IsObservingSources()) {
      // For custom tab backgrounds the background starts at the top of the tab
      // strip. Otherwise the background starts at the top of the frame.
      const int offset_y =
          tp->HasCustomImage(bg_id) ? 0 : background_offset_.y();
      // The new tab background is mirrored in RTL mode, but the theme
      // background should never be mirrored. Mirror it here to compensate.
      float x_scale = 1.0f;
      int x = GetMirroredX() + background_offset_.x();
      const gfx::Size size(GetLayoutSize(NEW_TAB_BUTTON));
      if (base::i18n::IsRTL()) {
        x_scale = -1.0f;
        // Offset by |width| such that the same region is painted as if there
        // was no flip.
        x += size.width();
      }

      const bool succeeded = canvas->InitPaintFlagsForTiling(
          *tp->GetImageSkiaNamed(bg_id), x, GetTopOffset() + offset_y,
          x_scale * scale, scale, 0, 0, &flags);
      DCHECK(succeeded);
    } else {
      const SkColor color =
          new_tab_promo_observer_.IsObservingSources()
              ? GetNativeTheme()->GetSystemColor(
                    ui::NativeTheme::kColorId_ProminentButtonColor)
              : tp->GetColor(ThemeProperties::COLOR_BACKGROUND_TAB);
      flags.setColor(color);
    }
    const SkColor stroke_color = tab_strip_->GetToolbarTopSeparatorColor();
    const SkAlpha alpha =
        static_cast<SkAlpha>(std::round(SkColorGetA(stroke_color) * 0.59375f));
    cc::PaintFlags shadow_flags = flags;
    shadow_flags.setLooper(
        CreateShadowDrawLooper(SkColorSetA(stroke_color, alpha)));
    canvas->DrawPath(fill, shadow_flags);
  }

  // Draw a white highlight on hover.
  const SkAlpha hover_alpha =
      static_cast<SkAlpha>(hover_animation().CurrentValueBetween(0x00, 0x4D));
  if (hover_alpha != SK_AlphaTRANSPARENT) {
    flags.setColor(SkColorSetA(SK_ColorWHITE, hover_alpha));
    canvas->DrawPath(fill, flags);
  }

  // Most states' opacities are adjusted using an opacity recorder in
  // TabStrip::PaintChildren(), but the pressed state is excluded there and
  // instead rendered using a dark overlay here.  Avoiding the use of the
  // opacity recorder keeps the stroke more visible in this state.
  if (pressed) {
    flags.setColor(SkColorSetA(SK_ColorBLACK, 0x14));
    canvas->DrawPath(fill, flags);
  }
}
