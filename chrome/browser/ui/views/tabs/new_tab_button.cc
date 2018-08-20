// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/new_tab_button.h"

#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/feature_promos/new_tab_promo_bubble_view.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/grit/theme_resources.h"
#include "components/feature_engagement/buildflags.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkMaskFilter.h"
#include "third_party/skia/include/effects/SkLayerDrawLooper.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/base/default_theme_provider.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
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

using MD = ui::MaterialDesignController;

namespace {

constexpr int kDistanceBetweenIcons = 6;
constexpr int kStrokeThickness = 1;

// Returns the size of the button without any spacing/padding.
gfx::Size GetButtonSize(bool is_incognito) {
  const gfx::Size sizes[] = {
      {36, 18}, {39, 21}, {(is_incognito ? 42 : 24), 24}, {28, 28}, {28, 28}};
  return sizes[MD::GetMode()];
}

sk_sp<SkDrawLooper> CreateShadowDrawLooper(SkColor color) {
  SkLayerDrawLooper::Builder looper_builder;
  looper_builder.addLayer();

  SkLayerDrawLooper::LayerInfo layer_info;
  layer_info.fPaintBits |= SkLayerDrawLooper::kMaskFilter_Bit;
  layer_info.fPaintBits |= SkLayerDrawLooper::kColorFilter_Bit;
  layer_info.fColorMode = SkBlendMode::kDst;
  layer_info.fOffset.set(0, 1);
  SkPaint* layer_paint = looper_builder.addLayer(layer_info);
  layer_paint->setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, 0.5));
  layer_paint->setColorFilter(
      SkColorFilter::MakeModeFilter(color, SkBlendMode::kSrcIn));
  return looper_builder.detach();
}

// Returns the ID of the resource that should be used for the button fill if
// any. |has_custom_image| will be set to true if the images of either the
// tab, the frame background, (or the toolbar if |is_non_refresh_touch_ui| is
// true) have been customized.
int GetButtonFillResourceIdIfAny(const TabStrip* tab_strip,
                                 const ui::ThemeProvider* theme_provider,
                                 bool is_non_refresh_touch_ui,
                                 bool* has_custom_image) {
  if (!is_non_refresh_touch_ui)
    return tab_strip->GetBackgroundResourceId(has_custom_image);

  constexpr int kTouchBackgroundId = IDR_THEME_TOOLBAR;
  *has_custom_image = theme_provider->HasCustomImage(kTouchBackgroundId);
  return kTouchBackgroundId;
}

}  // namespace

NewTabButton::NewTabButton(TabStrip* tab_strip, views::ButtonListener* listener)
    : views::ImageButton(listener),
      tab_strip_(tab_strip),
      is_incognito_(tab_strip->IsIncognito()) {
  set_animate_on_state_change(true);
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  set_triggerable_event_flags(triggerable_event_flags() |
                              ui::EF_MIDDLE_MOUSE_BUTTON);
#endif

  if (MD::IsNewerMaterialUi()) {
    // Initialize the ink drop mode for a ripple highlight on button press.
    ink_drop_container_ = new views::InkDropContainerView();
    AddChildView(ink_drop_container_);
    ink_drop_container_->SetVisible(false);

    SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
    set_ink_drop_visible_opacity(0.08f);

    SetFocusPainter(nullptr);
    SetInstallFocusRingOnFocus(true);
  }

  // In newer material UI, the button is placed vertically exactly in the
  // center of the tabstrip.  In older UI, the new tab button is placed at a
  // fixed distance from the bottom of the tabstrip.
  const int extra_vertical_space = GetLayoutConstant(TAB_HEIGHT) -
                                   GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP) -
                                   GetButtonSize(is_incognito_).height();
  constexpr int kNewTabButtonBottomOffset = 4;
  const int top = MD::IsNewerMaterialUi()
                      ? (extra_vertical_space / 2)
                      : (extra_vertical_space - kNewTabButtonBottomOffset);
  const int horizontal = MD::IsRefreshUi() ? 8 : 0;
  SetBorder(
      views::CreateEmptyBorder(gfx::Insets(top, horizontal, 0, horizontal)));
}

NewTabButton::~NewTabButton() {
  if (destroyed_)
    *destroyed_ = true;
}

// static
void NewTabButton::ShowPromoForLastActiveBrowser() {
  BrowserView* browser = static_cast<BrowserView*>(
      BrowserList::GetInstance()->GetLastActive()->window());

  browser->tabstrip()->new_tab_button()->ShowPromo();
}

// static
void NewTabButton::CloseBubbleForLastActiveBrowser() {
  BrowserView* browser = static_cast<BrowserView*>(
      BrowserList::GetInstance()->GetLastActive()->window());
  browser->tabstrip()->new_tab_button()->CloseBubble();
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

void NewTabButton::FrameColorsChanged() {
  if (MD::IsRefreshUi())
    UpdateInkDropBaseColor();
}

void NewTabButton::AnimateInkDropToStateForTesting(views::InkDropState state) {
  GetInkDrop()->AnimateToState(state);
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

void NewTabButton::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  DCHECK(ink_drop_layer->bounds().size() == GetContentsBounds().size());
  DCHECK(ink_drop_container_->bounds().size() == GetContentsBounds().size());
  ink_drop_container_->AddInkDropLayer(ink_drop_layer);
  InstallInkDropMask(ink_drop_layer);
}

void NewTabButton::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  ResetInkDropMask();
  ink_drop_container_->RemoveInkDropLayer(ink_drop_layer);
}

std::unique_ptr<views::InkDropRipple> NewTabButton::CreateInkDropRipple()
    const {
  const gfx::Rect contents_bounds = GetContentsBounds();
  return std::make_unique<views::FloodFillInkDropRipple>(
      contents_bounds.size(), gfx::Insets(),
      GetInkDropCenterBasedOnLastEvent() - contents_bounds.OffsetFromOrigin(),
      GetInkDropBaseColor(), ink_drop_visible_opacity());
}

std::unique_ptr<views::InkDropHighlight> NewTabButton::CreateInkDropHighlight()
    const {
  const gfx::Rect bounds(GetContentsBounds().size());
  auto highlight = CreateDefaultInkDropHighlight(
      gfx::RectF(bounds).CenterPoint(), bounds.size());
  highlight->set_visible_opacity(0.1f);
  return highlight;
}

void NewTabButton::NotifyClick(const ui::Event& event) {
  ImageButton::NotifyClick(event);
  GetInkDrop()->AnimateToState(views::InkDropState::ACTION_TRIGGERED);
}

std::unique_ptr<views::InkDrop> NewTabButton::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      std::make_unique<views::InkDropImpl>(this, GetContentsBounds().size());
  ink_drop->SetAutoHighlightMode(views::InkDropImpl::AutoHighlightMode::NONE);
  ink_drop->SetShowHighlightOnHover(true);
  UpdateInkDropBaseColor();
  return ink_drop;
}

std::unique_ptr<views::InkDropMask> NewTabButton::CreateInkDropMask() const {
  return std::make_unique<views::RoundRectInkDropMask>(
      GetContentsBounds().size(), gfx::Insets(), GetCornerRadius());
}

void NewTabButton::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  const gfx::Rect contents_bounds = GetContentsBounds();
  canvas->Translate(contents_bounds.OffsetFromOrigin());
  const float scale = canvas->image_scale();
  const bool pressed = state() == views::Button::STATE_PRESSED;
  const SkColor stroke_color =
      new_tab_promo_observer_.IsObservingSources()
          ? color_utils::AlphaBlend(
                SK_ColorBLACK,
                GetNativeTheme()->GetSystemColor(
                    ui::NativeTheme::kColorId_ProminentButtonColor),
                0x70)
          : tab_strip_->GetToolbarTopSeparatorColor();

  // Fill.
  SkPath fill, stroke;
  const bool non_refresh_touch_ui =
      MD::GetMode() == ui::MaterialDesignController::MATERIAL_TOUCH_OPTIMIZED;
  fill =
      MD::IsNewerMaterialUi()
          ? GetNewerMaterialUiButtonPath(0, scale, false, non_refresh_touch_ui)
          : GetMaterialUiButtonPath(0, contents_bounds.height(), scale, false,
                                    true);
  // The ink drop is used to represent the pressed state under Refresh.
  PaintFill(!MD::IsRefreshUi() && pressed, scale, fill, canvas);

  // Stroke.
  if (!MD::IsRefreshUi())
    GetBorderPath(0, scale, false, &stroke);

  if (MD::IsNewerMaterialUi()) {
    const int plus_icon_size = MD::IsTouchOptimizedUiEnabled() ? 14 : 12;
    const int plus_icon_offset = GetCornerRadius() - (plus_icon_size / 2);

    PaintPlusIcon(canvas, plus_icon_offset, plus_icon_size);

    cc::PaintFlags paint_flags;
    paint_flags.setAntiAlias(true);
    if (ShouldDrawIncognitoIcon()) {
      DCHECK(!incognito_icon_.isNull());
      canvas->DrawImageInt(
          incognito_icon_,
          plus_icon_offset + plus_icon_size + kDistanceBetweenIcons,
          plus_icon_offset, paint_flags);
    }

    if (!MD::IsRefreshUi()) {
      // Draw stroke.
      // In the touch-optimized UI design, the new tab button is rendered flat,
      // regardless of whether pressed or not (i.e. we don't emulate a pushed
      // button by drawing a drop shadow). Instead, we're using an ink drop
      // ripple effect.
      // Here we want to make sure the stroke width is 1px regardless of the
      // device scale factor, so undo the scale.
      canvas->UndoDeviceScaleFactor();
      Op(stroke, fill, kDifference_SkPathOp, &stroke);
      paint_flags.setColor(stroke_color);
      canvas->DrawPath(stroke, paint_flags);
    }
  } else {
    // We want to draw a drop shadow either inside or outside the stroke,
    // depending on whether we're pressed; so, either clip out what's outside
    // the stroke, or clip out the fill inside it.
    canvas->UndoDeviceScaleFactor();
    if (pressed)
      canvas->ClipPath(stroke, true);
    Op(stroke, fill, kDifference_SkPathOp, &stroke);
    if (!pressed)
      canvas->sk_canvas()->clipPath(fill, SkClipOp::kDifference, true);
    // Now draw the stroke and shadow; the stroke will always be visible, while
    // the shadow will be affected by the clip we set above.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    const float alpha = SkColorGetA(stroke_color);
    const SkAlpha shadow_alpha =
        base::saturated_cast<SkAlpha>(std::round(2.1875f * alpha));
    flags.setLooper(
        CreateShadowDrawLooper(SkColorSetA(stroke_color, shadow_alpha)));
    const SkAlpha path_alpha = static_cast<SkAlpha>(
        std::round((pressed ? 0.875f : 0.609375f) * alpha));
    flags.setColor(SkColorSetA(stroke_color, path_alpha));
    canvas->DrawPath(stroke, flags);
  }
}

void NewTabButton::Layout() {
  ImageButton::Layout();

  if (MD::IsNewerMaterialUi()) {
    if (ShouldDrawIncognitoIcon() && incognito_icon_.isNull())
      InitIncognitoIcon();

    // TODO(pkasting): Instead of setting this bounds rect, maybe have the
    // container match the view bounds, then undo the coordinate transforms in
    // the ink drop creation methods above.
    const gfx::Rect contents_bounds = GetContentsBounds();
    ink_drop_container_->SetBoundsRect(contents_bounds);

    SkPath path;
    path.addOval(gfx::RectToSkRect(contents_bounds));
    focus_ring()->SetPath(path);
  }
}

void NewTabButton::OnThemeChanged() {
  ImageButton::OnThemeChanged();

  if (!MD::IsNewerMaterialUi())
    return;

  if (ShouldDrawIncognitoIcon())
    InitIncognitoIcon();
  UpdateInkDropBaseColor();
}

gfx::Size NewTabButton::CalculatePreferredSize() const {
  gfx::Size size = GetButtonSize(is_incognito_);
  const auto insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

void NewTabButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  const gfx::Size ink_drop_size = GetContentsBounds().size();
  GetInkDrop()->HostSizeChanged(ink_drop_size);
  UpdateInkDropMaskLayerSize(ink_drop_size);
}

bool NewTabButton::GetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);

  SkPath border;
  const gfx::Point contents_origin = GetContentsBounds().origin();
  const float scale = GetWidget()->GetCompositor()->device_scale_factor();
  // TODO(pkasting): Fitts' Law horizontally when appropriate.
  GetBorderPath(contents_origin.y() * scale, scale,
                tab_strip_->SizeTabButtonToTopOfTabStrip(), &border);
  border.offset(contents_origin.x(), 0);
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

bool NewTabButton::ShouldDrawIncognitoIcon() const {
  return is_incognito_ && MD::GetMode() == MD::MATERIAL_TOUCH_OPTIMIZED;
}

int NewTabButton::GetCornerRadius() const {
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::EMPHASIS_MAXIMUM, GetContentsBounds().size());
}

void NewTabButton::GetBorderPath(float button_y,
                                 float scale,
                                 bool extend_to_top,
                                 SkPath* path) const {
  *path =
      MD::IsNewerMaterialUi()
          ? GetNewerMaterialUiButtonPath(button_y, scale, extend_to_top, false)
          : GetMaterialUiButtonPath(button_y, GetContentsBounds().height(),
                                    scale, extend_to_top, false);
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
  // Note that for newer UI, we always draw the fill since the button
  // has a flat design. Hover highlights are handled by the ink drop.
  const bool is_newer_ui = MD::IsNewerMaterialUi();
  if (is_newer_ui || !pressed) {
    const bool non_refresh_touch_ui =
        MD::GetMode() == ui::MaterialDesignController::MATERIAL_TOUCH_OPTIMIZED;
    // First we compute the background image coordinates and scale, in case we
    // need to draw a custom background image.
    const ui::ThemeProvider* tp = GetThemeProvider();
    bool has_custom_image;
    const int bg_id = GetButtonFillResourceIdIfAny(
        tab_strip_, tp, non_refresh_touch_ui, &has_custom_image);
    if (has_custom_image && !new_tab_promo_observer_.IsObservingSources()) {
      // For non-refresh touch UI, the background is that of the active tab, so
      // the positioning must match that in Tab::PaintTab().
      const int offset_y =
          non_refresh_touch_ui ? -tab_strip_->GetStrokeThickness() : 0;
      // The new tab background is mirrored in RTL mode, but the theme
      // background should never be mirrored. Mirror it here to compensate.
      float x_scale = 1.0f;
      const gfx::Rect& contents_bounds = GetContentsBounds();
      int x = GetMirroredX() + contents_bounds.x() + background_offset_;
      if (base::i18n::IsRTL()) {
        x_scale = -1.0f;
        // Offset by |width| such that the same region is painted as if there
        // was no flip.
        x += contents_bounds.width();
      }

      const bool succeeded = canvas->InitPaintFlagsForTiling(
          *tp->GetImageSkiaNamed(bg_id), x, contents_bounds.y() + offset_y,
          x_scale * scale, scale, 0, 0, SkShader::kRepeat_TileMode,
          SkShader::kRepeat_TileMode, &flags);
      DCHECK(succeeded);
    } else {
      flags.setColor(GetButtonFillColor(false));
    }

    cc::PaintFlags shadow_flags = flags;
    // For Refresh, don't draw a shadow.
    if (!MD::IsRefreshUi()) {
      const SkColor stroke_color = tab_strip_->GetToolbarTopSeparatorColor();
      const SkAlpha alpha = static_cast<SkAlpha>(
          std::round(SkColorGetA(stroke_color) * 0.59375f));
      shadow_flags.setLooper(
          CreateShadowDrawLooper(SkColorSetA(stroke_color, alpha)));
    }
    canvas->DrawPath(fill, shadow_flags);

    if (is_newer_ui) {
      // We don't have hover/pressed states in the newer UI design.
      // Instead we are using an ink drop effect.
      return;
    }
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

void NewTabButton::PaintPlusIcon(gfx::Canvas* canvas, int offset, int size) {
  constexpr int kStrokeWidth = 2;
  constexpr int kCapRadius = kStrokeWidth / 2;

  cc::PaintFlags paint_flags;
  paint_flags.setAntiAlias(true);
  paint_flags.setColor(GetIconColor());
  paint_flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
  paint_flags.setStrokeWidth(kStrokeWidth);

  // With a round end-cap, the apparent line length will extend past the end
  // points by one radius of the cap. Reduce the specified length to take this
  // into account.
  const int start = offset + kCapRadius;
  const int end = offset + size - kCapRadius;
  const int center = offset + size / 2;
  // Draw the horizontal leg of the plus (+) icon
  canvas->DrawLine(gfx::PointF(start, center), gfx::PointF(end, center),
                   paint_flags);
  // Draw the vertical leg of the plus (+) icon
  canvas->DrawLine(gfx::PointF(center, start), gfx::PointF(center, end),
                   paint_flags);
}

SkColor NewTabButton::GetButtonFillColor(bool opaque) const {
  if (new_tab_promo_observer_.IsObservingSources()) {
    return GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_ProminentButtonColor);
  }

  return tab_strip_->GetTabBackgroundColor(TAB_INACTIVE, opaque);
}

SkColor NewTabButton::GetIconColor() const {
  return tab_strip_->GetTabForegroundColor(MD::IsRefreshUi() ? TAB_INACTIVE
                                                             : TAB_ACTIVE);
}

void NewTabButton::InitIncognitoIcon() {
  DCHECK(ShouldDrawIncognitoIcon());
  incognito_icon_ =
      gfx::CreateVectorIcon(kNewTabButtonIncognitoIcon, GetIconColor());
}

SkPath NewTabButton::GetNewerMaterialUiButtonPath(float button_y,
                                                  float scale,
                                                  bool extend_to_top,
                                                  bool for_fill) const {
  DCHECK(MD::IsNewerMaterialUi());

  const float radius = GetCornerRadius() * scale;
  const float rect_width =
      2 * radius +
      (ShouldDrawIncognitoIcon()
           ? scale * (incognito_icon_.width() + kDistanceBetweenIcons)
           : 0);

  const SkRect button_rect =
      SkRect::MakeXYWH(0, button_y, rect_width, 2 * radius);
  SkRRect rrect = SkRRect::MakeRectXY(button_rect, radius, radius);
  // Inset by 1px for a fill path to give room for the stroke to show up. The
  // stroke width is 1px regardless of the device scale factor.
  if (for_fill)
    rrect.inset(kStrokeThickness, kStrokeThickness);

  SkPath path;
  path.addRRect(rrect, SkPath::kCW_Direction);

  if (extend_to_top) {
    SkPath extension_path;
    extension_path.addRect(
        SkRect::MakeXYWH(0, 0, rect_width, button_y + radius),
        SkPath::kCW_Direction);
    Op(path, extension_path, kUnion_SkPathOp, &path);
  }

  path.close();
  return path;
}

SkPath NewTabButton::GetMaterialUiButtonPath(int button_y,
                                             int button_height,
                                             float scale,
                                             bool extend_to_top,
                                             bool for_fill) const {
  const float inverse_slope = Tab::GetInverseDiagonalSlope();
  float bottom = (button_height - 2) * scale;
  const float diag_height = bottom - 3.5 * scale;
  const float diag_width = diag_height * inverse_slope;
  const float right = diag_width + 4 * scale;
  const int stroke_thickness = for_fill ? 0 : kStrokeThickness;
  bottom += button_y + stroke_thickness;

  SkPath path;
  path.moveTo(right - stroke_thickness, bottom);
  path.rCubicTo(-0.75 * scale, 0, -1.625 * scale, -0.5 * scale, -2 * scale,
                -1.5 * scale);
  path.rLineTo(-diag_width, -diag_height);
  if (extend_to_top) {
    // Create the vertical extension by extending the side diagonals at the
    // upper left and lower right corners until they reach the top and bottom of
    // the border, respectively (in other words, "un-round-off" those corners
    // and turn them into sharp points).  Then extend upward from the corner
    // points to the top of the bounds.
    const float dy = scale + stroke_thickness * 2;
    const float dx = inverse_slope * dy;
    path.rLineTo(-dx, -dy);
    path.rLineTo(0, -button_y - scale + stroke_thickness);
    path.lineTo((width() - 2) * scale + stroke_thickness + dx, 0);
    path.rLineTo(0, bottom);
  } else {
    if (for_fill) {
      path.rCubicTo(0, -0.5 * scale, 0.25 * scale, -scale, scale, -scale);
    } else {
      path.rCubicTo(-0.5 * scale, -1.125 * scale, 0.5 * scale,
                    -scale - 2 * stroke_thickness, scale,
                    -scale - 2 * stroke_thickness);
    }
    path.lineTo((width() - 4) * scale - diag_width + stroke_thickness,
                button_y + scale - stroke_thickness);
    path.rCubicTo(0.75 * scale, 0, 1.625 * scale, 0.5 * scale, 2 * scale,
                  1.5 * scale);
    path.rLineTo(diag_width, diag_height);
    if (for_fill) {
      path.rCubicTo(0, 0.5 * scale, -0.25 * scale, scale, -scale, scale);
    } else {
      path.rCubicTo(0.5 * scale, 1.125 * scale, -0.5 * scale,
                    scale + 2 * stroke_thickness, -scale,
                    scale + 2 * stroke_thickness);
    }
  }
  path.close();

  return path;
}

void NewTabButton::UpdateInkDropBaseColor() {
  DCHECK(MD::IsNewerMaterialUi());

  set_ink_drop_base_color(color_utils::BlendTowardOppositeLuma(
      GetButtonFillColor(true), SK_AlphaOPAQUE));
}
