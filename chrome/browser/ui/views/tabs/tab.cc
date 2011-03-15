// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab.h"

#include <limits>

#include "base/utf_string_conversions.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/animation/multi_animation.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/font.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skbitmap_operations.h"
#include "views/controls/button/image_button.h"
#include "views/widget/tooltip_manager.h"
#include "views/widget/widget.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"

static const int kLeftPadding = 16;
static const int kTopPadding = 6;
static const int kRightPadding = 15;
static const int kBottomPadding = 5;
static const int kDropShadowHeight = 2;
static const int kToolbarOverlap = 1;
static const int kFavIconTitleSpacing = 4;
static const int kTitleCloseButtonSpacing = 5;
static const int kStandardTitleWidth = 175;
static const int kCloseButtonVertFuzz = 0;
static const int kCloseButtonHorzFuzz = 5;

// Vertical adjustment to the favicon when the tab has a large icon.
static const int kAppTapFaviconVerticalAdjustment = 2;

// When a non-mini-tab becomes a mini-tab the width of the tab animates. If
// the width of a mini-tab is >= kMiniTabRendererAsNormalTabWidth then the tab
// is rendered as a normal tab. This is done to avoid having the title
// immediately disappear when transitioning a tab from normal to mini-tab.
static const int kMiniTabRendererAsNormalTabWidth =
    browser_defaults::kMiniTabWidth + 30;

// How opaque to make the hover state (out of 1).
static const double kHoverOpacity = 0.33;
static const double kHoverSlideOpacity = 0.5;

Tab::TabImage Tab::tab_alpha_ = {0};
Tab::TabImage Tab::tab_active_ = {0};
Tab::TabImage Tab::tab_inactive_ = {0};

// Durations for the various parts of the mini tab title animation.
static const int kMiniTitleChangeAnimationDuration1MS = 1600;
static const int kMiniTitleChangeAnimationStart1MS = 0;
static const int kMiniTitleChangeAnimationEnd1MS = 1900;
static const int kMiniTitleChangeAnimationDuration2MS = 0;
static const int kMiniTitleChangeAnimationDuration3MS = 550;
static const int kMiniTitleChangeAnimationStart3MS = 150;
static const int kMiniTitleChangeAnimationEnd3MS = 800;

// Offset from the right edge for the start of the mini title change animation.
static const int kMiniTitleChangeInitialXOffset = 6;

// Radius of the radial gradient used for mini title change animation.
static const int kMiniTitleChangeGradientRadius = 20;

// Colors of the gradient used during the mini title change animation.
static const SkColor kMiniTitleChangeGradientColor1 = SK_ColorWHITE;
static const SkColor kMiniTitleChangeGradientColor2 =
    SkColorSetARGB(0, 255, 255, 255);

// Hit mask constants.
static const SkScalar kTabCapWidth = 15;
static const SkScalar kTabTopCurveWidth = 4;
static const SkScalar kTabBottomCurveWidth = 3;

// static
const char Tab::kViewClassName[] = "browser/tabs/Tab";

////////////////////////////////////////////////////////////////////////////////
// Tab, public:

Tab::Tab(TabController* controller)
    : BaseTab(controller),
      showing_icon_(false),
      showing_close_button_(false),
      close_button_color_(NULL) {
  InitTabResources();
}

Tab::~Tab() {
}

void Tab::StartMiniTabTitleAnimation() {
  if (!mini_title_animation_.get()) {
    ui::MultiAnimation::Parts parts;
    parts.push_back(
        ui::MultiAnimation::Part(kMiniTitleChangeAnimationDuration1MS,
                                 ui::Tween::EASE_OUT));
    parts.push_back(
        ui::MultiAnimation::Part(kMiniTitleChangeAnimationDuration2MS,
                                 ui::Tween::ZERO));
    parts.push_back(
        ui::MultiAnimation::Part(kMiniTitleChangeAnimationDuration3MS,
                                 ui::Tween::EASE_IN));
    parts[0].start_time_ms = kMiniTitleChangeAnimationStart1MS;
    parts[0].end_time_ms = kMiniTitleChangeAnimationEnd1MS;
    parts[2].start_time_ms = kMiniTitleChangeAnimationStart3MS;
    parts[2].end_time_ms = kMiniTitleChangeAnimationEnd3MS;
    mini_title_animation_.reset(new ui::MultiAnimation(parts));
    mini_title_animation_->SetContainer(animation_container());
    mini_title_animation_->set_delegate(this);
  }
  mini_title_animation_->Start();
}

void Tab::StopMiniTabTitleAnimation() {
  if (mini_title_animation_.get())
    mini_title_animation_->Stop();
}

// static
gfx::Size Tab::GetMinimumUnselectedSize() {
  InitTabResources();

  gfx::Size minimum_size;
  minimum_size.set_width(kLeftPadding + kRightPadding);
  // Since we use bitmap images, the real minimum height of the image is
  // defined most accurately by the height of the end cap images.
  minimum_size.set_height(tab_active_.image_l->height());
  return minimum_size;
}

// static
gfx::Size Tab::GetMinimumSelectedSize() {
  gfx::Size minimum_size = GetMinimumUnselectedSize();
  minimum_size.set_width(kLeftPadding + kFavIconSize + kRightPadding);
  return minimum_size;
}

// static
gfx::Size Tab::GetStandardSize() {
  gfx::Size standard_size = GetMinimumUnselectedSize();
  standard_size.set_width(
      standard_size.width() + kFavIconTitleSpacing + kStandardTitleWidth);
  return standard_size;
}

// static
int Tab::GetMiniWidth() {
  return browser_defaults::kMiniTabWidth;
}

////////////////////////////////////////////////////////////////////////////////
// Tab, protected:

const gfx::Rect& Tab::GetTitleBounds() const {
  return title_bounds_;
}

const gfx::Rect& Tab::GetIconBounds() const {
  return favicon_bounds_;
}

void Tab::DataChanged(const TabRendererData& old) {
  if (data().blocked == old.blocked)
    return;

  if (data().blocked)
    StartPulse();
  else
    StopPulse();
}

////////////////////////////////////////////////////////////////////////////////
// Tab, views::View overrides:

void Tab::OnPaint(gfx::Canvas* canvas) {
  // Don't paint if we're narrower than we can render correctly. (This should
  // only happen during animations).
  if (width() < GetMinimumUnselectedSize().width() && !data().mini)
    return;

  // See if the model changes whether the icons should be painted.
  const bool show_icon = ShouldShowIcon();
  const bool show_close_button = ShouldShowCloseBox();
  if (show_icon != showing_icon_ || show_close_button != showing_close_button_)
    Layout();

  PaintTabBackground(canvas);

  SkColor title_color = GetThemeProvider()->
      GetColor(IsSelected() ?
          BrowserThemeProvider::COLOR_TAB_TEXT :
          BrowserThemeProvider::COLOR_BACKGROUND_TAB_TEXT);

  if (!data().mini || width() > kMiniTabRendererAsNormalTabWidth)
    PaintTitle(canvas, title_color);

  if (show_icon)
    PaintIcon(canvas);

  // If the close button color has changed, generate a new one.
  if (!close_button_color_ || title_color != close_button_color_) {
    close_button_color_ = title_color;
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    close_button()->SetBackground(close_button_color_,
        rb.GetBitmapNamed(IDR_TAB_CLOSE),
        rb.GetBitmapNamed(IDR_TAB_CLOSE_MASK));
  }
}

void Tab::Layout() {
  gfx::Rect lb = GetContentsBounds();
  if (lb.IsEmpty())
    return;
  lb.Inset(kLeftPadding, kTopPadding, kRightPadding, kBottomPadding);

  // The height of the content of the Tab is the largest of the favicon,
  // the title text and the close button graphic.
  int content_height = std::max(kFavIconSize, font_height());
  gfx::Size close_button_size(close_button()->GetPreferredSize());
  content_height = std::max(content_height, close_button_size.height());

  // Size the Favicon.
  showing_icon_ = ShouldShowIcon();
  if (showing_icon_) {
    // Use the size of the favicon as apps use a bigger favicon size.
    int favicon_top = kTopPadding + content_height / 2 - kFavIconSize / 2;
    int favicon_left = lb.x();
    favicon_bounds_.SetRect(favicon_left, favicon_top,
                            kFavIconSize, kFavIconSize);
    if (data().mini && width() < kMiniTabRendererAsNormalTabWidth) {
      // Adjust the location of the favicon when transitioning from a normal
      // tab to a mini-tab.
      int mini_delta = kMiniTabRendererAsNormalTabWidth - GetMiniWidth();
      int ideal_delta = width() - GetMiniWidth();
      if (ideal_delta < mini_delta) {
        int ideal_x = (GetMiniWidth() - kFavIconSize) / 2;
        int x = favicon_bounds_.x() + static_cast<int>(
            (1 - static_cast<float>(ideal_delta) /
             static_cast<float>(mini_delta)) *
            (ideal_x - favicon_bounds_.x()));
        favicon_bounds_.set_x(x);
      }
    }
  } else {
    favicon_bounds_.SetRect(lb.x(), lb.y(), 0, 0);
  }

  // Size the Close button.
  showing_close_button_ = ShouldShowCloseBox();
  if (showing_close_button_) {
    int close_button_top = kTopPadding + kCloseButtonVertFuzz +
        (content_height - close_button_size.height()) / 2;
    // If the ratio of the close button size to tab width exceeds the maximum.
    close_button()->SetBounds(lb.width() + kCloseButtonHorzFuzz,
                              close_button_top, close_button_size.width(),
                              close_button_size.height());
    close_button()->SetVisible(true);
  } else {
    close_button()->SetBounds(0, 0, 0, 0);
    close_button()->SetVisible(false);
  }

  int title_left = favicon_bounds_.right() + kFavIconTitleSpacing;
  int title_top = kTopPadding + (content_height - font_height()) / 2;
  // Size the Title text to fill the remaining space.
  if (!data().mini || width() >= kMiniTabRendererAsNormalTabWidth) {
    // If the user has big fonts, the title will appear rendered too far down
    // on the y-axis if we use the regular top padding, so we need to adjust it
    // so that the text appears centered.
    gfx::Size minimum_size = GetMinimumUnselectedSize();
    int text_height = title_top + font_height() + kBottomPadding;
    if (text_height > minimum_size.height())
      title_top -= (text_height - minimum_size.height()) / 2;

    int title_width;
    if (close_button()->IsVisible()) {
      title_width = std::max(close_button()->x() -
                             kTitleCloseButtonSpacing - title_left, 0);
    } else {
      title_width = std::max(lb.width() - title_left, 0);
    }
    title_bounds_.SetRect(title_left, title_top, title_width, font_height());
  } else {
    title_bounds_.SetRect(title_left, title_top, 0, 0);
  }

  // Certain UI elements within the Tab (the favicon, etc.) are not represented
  // as child Views (which is the preferred method).  Instead, these UI elements
  // are drawn directly on the canvas from within Tab::OnPaint(). The Tab's
  // child Views (for example, the Tab's close button which is a views::Button
  // instance) are automatically mirrored by the mirroring infrastructure in
  // views. The elements Tab draws directly on the canvas need to be manually
  // mirrored if the View's layout is right-to-left.
  title_bounds_.set_x(GetMirroredXForRect(title_bounds_));
}

void Tab::OnThemeChanged() {
  LoadTabImages();
}

std::string Tab::GetClassName() const {
  return kViewClassName;
}

bool Tab::HasHitTestMask() const {
  return true;
}

void Tab::GetHitTestMask(gfx::Path* path) const {
  DCHECK(path);

  SkScalar h = SkIntToScalar(height());
  SkScalar w = SkIntToScalar(width());

  path->moveTo(0, h);

  // Left end cap.
  path->lineTo(kTabBottomCurveWidth, h - kTabBottomCurveWidth);
  path->lineTo(kTabCapWidth - kTabTopCurveWidth, kTabTopCurveWidth);
  path->lineTo(kTabCapWidth, 0);

  // Connect to the right cap.
  path->lineTo(w - kTabCapWidth, 0);

  // Right end cap.
  path->lineTo(w - kTabCapWidth + kTabTopCurveWidth, kTabTopCurveWidth);
  path->lineTo(w - kTabBottomCurveWidth, h - kTabBottomCurveWidth);
  path->lineTo(w, h);

  // Close out the path.
  path->lineTo(0, h);
  path->close();
}

bool Tab::GetTooltipTextOrigin(const gfx::Point& p, gfx::Point* origin) {
  origin->set_x(title_bounds_.x() + 10);
  origin->set_y(-views::TooltipManager::GetTooltipHeight() - 4);
  return true;
}

void Tab::OnMouseMoved(const views::MouseEvent& event) {
  hover_point_ = event.location();
  // We need to redraw here because otherwise the hover glow does not update
  // and follow the new mouse position.
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// Tab, private

void Tab::PaintTabBackground(gfx::Canvas* canvas) {
  if (IsSelected()) {
    PaintActiveTabBackground(canvas);
  } else {
    if (mini_title_animation_.get() && mini_title_animation_->is_animating())
      PaintInactiveTabBackgroundWithTitleChange(canvas);
    else
      PaintInactiveTabBackground(canvas);

    double throb_value = GetThrobValue();
    if (throb_value > 0) {
      canvas->SaveLayerAlpha(static_cast<int>(throb_value * 0xff),
                             gfx::Rect(width(), height()));
      canvas->AsCanvasSkia()->drawARGB(0, 255, 255, 255,
                                       SkXfermode::kClear_Mode);
      PaintActiveTabBackground(canvas);
      canvas->Restore();
    }
  }
}

void Tab::PaintInactiveTabBackgroundWithTitleChange(gfx::Canvas* canvas) {
  // Render the inactive tab background. We'll use this for clipping.
  gfx::CanvasSkia background_canvas(width(), height(), false);
  PaintInactiveTabBackground(&background_canvas);

  SkBitmap background_image = background_canvas.ExtractBitmap();

  // Draw a radial gradient to hover_canvas.
  gfx::CanvasSkia hover_canvas(width(), height(), false);
  int radius = kMiniTitleChangeGradientRadius;
  int x0 = width() + radius - kMiniTitleChangeInitialXOffset;
  int x1 = radius;
  int x2 = -radius;
  int x;
  if (mini_title_animation_->current_part_index() == 0) {
    x = mini_title_animation_->CurrentValueBetween(x0, x1);
  } else if (mini_title_animation_->current_part_index() == 1) {
    x = x1;
  } else {
    x = mini_title_animation_->CurrentValueBetween(x1, x2);
  }
  SkPaint paint;
  SkPoint loc = { SkIntToScalar(x), SkIntToScalar(0) };
  SkColor colors[2];
  colors[0] = kMiniTitleChangeGradientColor1;
  colors[1] = kMiniTitleChangeGradientColor2;
  SkShader* shader = SkGradientShader::CreateRadial(
      loc,
      SkIntToScalar(radius),
      colors,
      NULL,
      2,
      SkShader::kClamp_TileMode);
  paint.setShader(shader);
  shader->unref();
  hover_canvas.DrawRectInt(x - radius, -radius, radius * 2, radius * 2, paint);

  // Draw the radial gradient clipped to the background into hover_image.
  SkBitmap hover_image = SkBitmapOperations::CreateMaskedBitmap(
      hover_canvas.ExtractBitmap(), background_image);

  // Draw the tab background to the canvas.
  canvas->DrawBitmapInt(background_image, 0, 0);

  // And then the gradient on top of that.
  if (mini_title_animation_->current_part_index() == 2) {
    canvas->SaveLayerAlpha(mini_title_animation_->CurrentValueBetween(255, 0));
    canvas->DrawBitmapInt(hover_image, 0, 0);
    canvas->Restore();
  } else {
    canvas->DrawBitmapInt(hover_image, 0, 0);
  }
}

void Tab::PaintInactiveTabBackground(gfx::Canvas* canvas) {
  // The tab image needs to be lined up with the background image
  // so that it feels partially transparent.  These offsets represent the tab
  // position within the frame background image.
  int offset = GetMirroredX() + background_offset_.x();

  int tab_id;
  if (GetWidget() &&
      GetWidget()->GetWindow()->non_client_view()->UseNativeFrame()) {
    tab_id = IDR_THEME_TAB_BACKGROUND_V;
  } else {
    tab_id = data().incognito ? IDR_THEME_TAB_BACKGROUND_INCOGNITO :
                                IDR_THEME_TAB_BACKGROUND;
  }

  SkBitmap* tab_bg = GetThemeProvider()->GetBitmapNamed(tab_id);

  TabImage* tab_image = &tab_active_;
  TabImage* tab_inactive_image = &tab_inactive_;
  TabImage* alpha = &tab_alpha_;

  // If the theme is providing a custom background image, then its top edge
  // should be at the top of the tab. Otherwise, we assume that the background
  // image is a composited foreground + frame image.
  int bg_offset_y = GetThemeProvider()->HasCustomImage(tab_id) ?
      0 : background_offset_.y();

  // We need a CanvasSkia object to be able to extract the bitmap from.
  // We draw everything to this canvas and then output it to the canvas
  // parameter in addition to using it to mask the hover glow if needed.
  gfx::CanvasSkia background_canvas(width(), height(), false);

  // Draw left edge.  Don't draw over the toolbar, as we're not the foreground
  // tab.
  SkBitmap tab_l = SkBitmapOperations::CreateTiledBitmap(
      *tab_bg, offset, bg_offset_y, tab_image->l_width, height());
  SkBitmap theme_l =
      SkBitmapOperations::CreateMaskedBitmap(tab_l, *alpha->image_l);
  background_canvas.DrawBitmapInt(theme_l,
      0, 0, theme_l.width(), theme_l.height() - kToolbarOverlap,
      0, 0, theme_l.width(), theme_l.height() - kToolbarOverlap,
      false);

  // Draw right edge.  Again, don't draw over the toolbar.
  SkBitmap tab_r = SkBitmapOperations::CreateTiledBitmap(*tab_bg,
      offset + width() - tab_image->r_width, bg_offset_y,
      tab_image->r_width, height());
  SkBitmap theme_r =
      SkBitmapOperations::CreateMaskedBitmap(tab_r, *alpha->image_r);
  background_canvas.DrawBitmapInt(theme_r,
      0, 0, theme_r.width(), theme_r.height() - kToolbarOverlap,
      width() - theme_r.width(), 0, theme_r.width(),
      theme_r.height() - kToolbarOverlap, false);

  // Draw center.  Instead of masking out the top portion we simply skip over
  // it by incrementing by kDropShadowHeight, since it's a simple rectangle.
  // And again, don't draw over the toolbar.
  background_canvas.TileImageInt(*tab_bg,
     offset + tab_image->l_width,
     bg_offset_y + kDropShadowHeight + tab_image->y_offset,
     tab_image->l_width,
     kDropShadowHeight + tab_image->y_offset,
     width() - tab_image->l_width - tab_image->r_width,
     height() - kDropShadowHeight - kToolbarOverlap - tab_image->y_offset);

  canvas->DrawBitmapInt(background_canvas.ExtractBitmap(), 0, 0);

  if (!GetThemeProvider()->HasCustomImage(tab_id) &&
      hover_animation() &&
      (hover_animation()->IsShowing() || hover_animation()->is_animating())) {
    SkBitmap hover_glow = DrawHoverGlowBitmap(width(), height());
    // Draw the hover glow clipped to the background into hover_image.
    SkBitmap hover_image = SkBitmapOperations::CreateMaskedBitmap(
        hover_glow, background_canvas.ExtractBitmap());
    canvas->DrawBitmapInt(hover_image, 0, 0);
  }

  // Now draw the highlights/shadows around the tab edge.
  canvas->DrawBitmapInt(*tab_inactive_image->image_l, 0, 0);
  canvas->TileImageInt(*tab_inactive_image->image_c,
                       tab_inactive_image->l_width, 0,
                       width() - tab_inactive_image->l_width -
                           tab_inactive_image->r_width,
                       height());
  canvas->DrawBitmapInt(*tab_inactive_image->image_r,
                        width() - tab_inactive_image->r_width, 0);
}

void Tab::PaintActiveTabBackground(gfx::Canvas* canvas) {
  int offset = GetMirroredX() + background_offset_.x();
  ui::ThemeProvider* tp = GetThemeProvider();
  DCHECK(tp) << "Unable to get theme provider";

  SkBitmap* tab_bg = GetThemeProvider()->GetBitmapNamed(IDR_THEME_TOOLBAR);

  TabImage* tab_image = &tab_active_;
  TabImage* alpha = &tab_alpha_;

  // Draw left edge.
  SkBitmap tab_l = SkBitmapOperations::CreateTiledBitmap(
      *tab_bg, offset, 0, tab_image->l_width, height());
  SkBitmap theme_l =
      SkBitmapOperations::CreateMaskedBitmap(tab_l, *alpha->image_l);
  canvas->DrawBitmapInt(theme_l, 0, 0);

  // Draw right edge.
  SkBitmap tab_r = SkBitmapOperations::CreateTiledBitmap(*tab_bg,
      offset + width() - tab_image->r_width, 0, tab_image->r_width, height());
  SkBitmap theme_r =
      SkBitmapOperations::CreateMaskedBitmap(tab_r, *alpha->image_r);
  canvas->DrawBitmapInt(theme_r, width() - tab_image->r_width, 0);

  // Draw center.  Instead of masking out the top portion we simply skip over it
  // by incrementing by kDropShadowHeight, since it's a simple rectangle.
  canvas->TileImageInt(*tab_bg,
     offset + tab_image->l_width,
     kDropShadowHeight + tab_image->y_offset,
     tab_image->l_width,
     kDropShadowHeight + tab_image->y_offset,
     width() - tab_image->l_width - tab_image->r_width,
     height() - kDropShadowHeight - tab_image->y_offset);

  // Now draw the highlights/shadows around the tab edge.
  canvas->DrawBitmapInt(*tab_image->image_l, 0, 0);
  canvas->TileImageInt(*tab_image->image_c, tab_image->l_width, 0,
      width() - tab_image->l_width - tab_image->r_width, height());
  canvas->DrawBitmapInt(*tab_image->image_r, width() - tab_image->r_width, 0);
}

SkBitmap Tab::DrawHoverGlowBitmap(int width_input, int height_input) {
  // Draw a radial gradient to hover_canvas so we can export the bitmap.
  gfx::CanvasSkia hover_canvas(width_input, height_input, false);

  // Draw a radial gradient to hover_canvas.
  int radius = width() / 3;

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  SkPoint loc = { SkIntToScalar(hover_point_.x()),
                  SkIntToScalar(hover_point_.y()) };
  SkColor colors[2];
  const ui::SlideAnimation* hover_slide = hover_animation();
  int hover_alpha = 0;
  if (hover_slide) {
    hover_alpha = static_cast<int>(255 * kHoverSlideOpacity *
                                   hover_slide->GetCurrentValue());
  }
  colors[0] = SkColorSetARGB(hover_alpha, 255, 255, 255);
  colors[1] = SkColorSetARGB(0, 255, 255, 255);
  SkShader* shader = SkGradientShader::CreateRadial(
      loc,
      SkIntToScalar(radius),
      colors,
      NULL,
      2,
      SkShader::kClamp_TileMode);
  // Shader can end up null when radius = 0.
  // If so, this results in default full tab glow behavior.
  if (shader) {
    paint.setShader(shader);
    shader->unref();
    hover_canvas.DrawRectInt(hover_point_.x() - radius,
                             hover_point_.y() - radius,
                             radius * 2, radius * 2, paint);
  }
  return hover_canvas.ExtractBitmap();
}

int Tab::IconCapacity() const {
  if (height() < GetMinimumUnselectedSize().height())
    return 0;
  return (width() - kLeftPadding - kRightPadding) / kFavIconSize;
}

bool Tab::ShouldShowIcon() const {
  if (data().mini && height() >= GetMinimumUnselectedSize().height())
    return true;
  if (!data().show_icon) {
    return false;
  } else if (IsSelected()) {
    // The selected tab clips favicon before close button.
    return IconCapacity() >= 2;
  }
  // Non-selected tabs clip close button before favicon.
  return IconCapacity() >= 1;
}

bool Tab::ShouldShowCloseBox() const {
  // The selected tab never clips close button.
  return !data().mini && IsCloseable() &&
      (IsSelected() || IconCapacity() >= 3);
}

double Tab::GetThrobValue() {
  if (pulse_animation() && pulse_animation()->is_animating())
    return pulse_animation()->GetCurrentValue() * kHoverOpacity;

  return hover_animation() ?
      kHoverOpacity * hover_animation()->GetCurrentValue() : 0;
}

////////////////////////////////////////////////////////////////////////////////
// Tab, private:

// static
void Tab::InitTabResources() {
  static bool initialized = false;
  if (initialized)
    return;

  initialized = true;

  // Load the tab images once now, and maybe again later if the theme changes.
  LoadTabImages();
}

// static
void Tab::LoadTabImages() {
  // We're not letting people override tab images just yet.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  tab_alpha_.image_l = rb.GetBitmapNamed(IDR_TAB_ALPHA_LEFT);
  tab_alpha_.image_r = rb.GetBitmapNamed(IDR_TAB_ALPHA_RIGHT);

  tab_active_.image_l = rb.GetBitmapNamed(IDR_TAB_ACTIVE_LEFT);
  tab_active_.image_c = rb.GetBitmapNamed(IDR_TAB_ACTIVE_CENTER);
  tab_active_.image_r = rb.GetBitmapNamed(IDR_TAB_ACTIVE_RIGHT);
  tab_active_.l_width = tab_active_.image_l->width();
  tab_active_.r_width = tab_active_.image_r->width();

  tab_inactive_.image_l = rb.GetBitmapNamed(IDR_TAB_INACTIVE_LEFT);
  tab_inactive_.image_c = rb.GetBitmapNamed(IDR_TAB_INACTIVE_CENTER);
  tab_inactive_.image_r = rb.GetBitmapNamed(IDR_TAB_INACTIVE_RIGHT);
  tab_inactive_.l_width = tab_inactive_.image_l->width();
  tab_inactive_.r_width = tab_inactive_.image_r->width();
}
