// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab.h"

#include <limits>

#include "base/utf_string_conversions.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/tabs/tab_resources.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/animation/multi_animation.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/path.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace {

// Padding around the "content" of a tab, occupied by the tab border graphics.

int left_padding() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_ASH:
      case ui::LAYOUT_DESKTOP:
        value = 22;
        break;
      case ui::LAYOUT_TOUCH:
        value = 30;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

int top_padding() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_ASH:
      case ui::LAYOUT_DESKTOP:
        value = 6;
        break;
      case ui::LAYOUT_TOUCH:
        value = 12;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

int right_padding() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_ASH:
      case ui::LAYOUT_DESKTOP:
        value = 19;
        break;
      case ui::LAYOUT_TOUCH:
        value = 23;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

int bottom_padding() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_ASH:
      case ui::LAYOUT_DESKTOP:
        value = 5;
        break;
      case ui::LAYOUT_TOUCH:
        value = 7;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

// Height of the shadow at the top of the tab image assets.
int drop_shadow_height() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_ASH:
      case ui::LAYOUT_DESKTOP:
        value = 4;
        break;
      case ui::LAYOUT_TOUCH:
        value = 5;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

// Size of icon used for throbber and favicon next to tab title.
int tab_icon_size() {
  static int value = -1;
  if (value == -1) {
    switch (ui::GetDisplayLayout()) {
      case ui::LAYOUT_ASH:
      case ui::LAYOUT_DESKTOP:
        value = gfx::kFaviconSize;
        break;
      case ui::LAYOUT_TOUCH:
        value = 20;
        break;
      default:
        NOTREACHED();
    }
  }
  return value;
}

// Width of touch tabs.
static const int kTouchWidth = 120;

static const int kToolbarOverlap = 1;
static const int kFaviconTitleSpacing = 4;
#if defined(USE_ASH)
// Additional vertical offset for title text relative to top of tab.
// Ash text rendering may be different than Windows.
// TODO(jamescook): Make this Chrome OS or Linux only?
static const int kTitleTextOffsetY = 1;
#else
static const int kTitleTextOffsetY = 0;
#endif
static const int kTitleCloseButtonSpacing = 3;
static const int kStandardTitleWidth = 175;
#if defined(USE_ASH)
// Additional vertical offset for close button relative to top of tab.
// Ash needs this to match the text vertical position.
static const int kCloseButtonVertFuzz = 1;
#else
static const int kCloseButtonVertFuzz = 0;
#endif
// Additional horizontal offset for close button relative to title text.
static const int kCloseButtonHorzFuzz = 3;

// When a non-mini-tab becomes a mini-tab the width of the tab animates. If
// the width of a mini-tab is >= kMiniTabRendererAsNormalTabWidth then the tab
// is rendered as a normal tab. This is done to avoid having the title
// immediately disappear when transitioning a tab from normal to mini-tab.
static const int kMiniTabRendererAsNormalTabWidth =
    browser_defaults::kMiniTabWidth + 30;

// How opaque to make the hover state (out of 1).
static const double kHoverOpacity = 0.33;

// Opacity for non-active selected tabs.
static const double kSelectedTabOpacity = .45;

// Selected (but not active) tabs have their throb value scaled down by this.
static const double kSelectedTabThrobScale = .5;

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

}  // namespace

Tab::TabImage Tab::tab_alpha_ = {0};
Tab::TabImage Tab::tab_active_ = {0};
Tab::TabImage Tab::tab_active_search_ = {0};
Tab::TabImage Tab::tab_inactive_ = {0};

// static
const char Tab::kViewClassName[] = "BrowserTab";

////////////////////////////////////////////////////////////////////////////////
// Tab, public:

Tab::Tab(TabController* controller)
    : BaseTab(controller),
      showing_icon_(false),
      showing_close_button_(false),
      close_button_color_(0) {
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
gfx::Size Tab::GetBasicMinimumUnselectedSize() {
  InitTabResources();

  gfx::Size minimum_size;
  minimum_size.set_width(left_padding() + right_padding());
  // Since we use image images, the real minimum height of the image is
  // defined most accurately by the height of the end cap images.
  minimum_size.set_height(tab_active_.image_l->height());
  return minimum_size;
}

gfx::Size Tab::GetMinimumUnselectedSize() {
  return GetBasicMinimumUnselectedSize();
}

// static
gfx::Size Tab::GetMinimumSelectedSize() {
  gfx::Size minimum_size = GetBasicMinimumUnselectedSize();
  minimum_size.set_width(
      left_padding() + gfx::kFaviconSize + right_padding());
  return minimum_size;
}

// static
gfx::Size Tab::GetStandardSize() {
  gfx::Size standard_size = GetBasicMinimumUnselectedSize();
  standard_size.set_width(
      standard_size.width() + kFaviconTitleSpacing + kStandardTitleWidth);
  return standard_size;
}

// static
int Tab::GetTouchWidth() {
  return kTouchWidth;
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

  gfx::Rect clip;
  if (controller()) {
    if (!controller()->ShouldPaintTab(this, &clip))
      return;
    if (!clip.IsEmpty()) {
      canvas->Save();
      canvas->ClipRect(clip);
    }
  }

  // See if the model changes whether the icons should be painted.
  const bool show_icon = ShouldShowIcon();
  const bool show_close_button = ShouldShowCloseBox();
  if (show_icon != showing_icon_ || show_close_button != showing_close_button_)
    Layout();

  PaintTabBackground(canvas);

  SkColor title_color = GetThemeProvider()->
      GetColor(IsSelected() ?
          ThemeService::COLOR_TAB_TEXT :
          ThemeService::COLOR_BACKGROUND_TAB_TEXT);

  if (!data().mini || width() > kMiniTabRendererAsNormalTabWidth)
    PaintTitle(canvas, title_color);

  if (show_icon)
    PaintIcon(canvas);

  // If the close button color has changed, generate a new one.
  if (!close_button_color_ || title_color != close_button_color_) {
    close_button_color_ = title_color;
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    close_button()->SetBackground(close_button_color_,
        rb.GetImageSkiaNamed(IDR_TAB_CLOSE),
        rb.GetImageSkiaNamed(IDR_TAB_CLOSE_MASK));
  }

  if (!clip.IsEmpty())
    canvas->Restore();
}

void Tab::Layout() {
  gfx::Rect lb = GetContentsBounds();
  if (lb.IsEmpty())
    return;
  lb.Inset(
      left_padding(), top_padding(), right_padding(), bottom_padding());

  // The height of the content of the Tab is the largest of the favicon,
  // the title text and the close button graphic.
  int content_height = std::max(tab_icon_size(), font_height());
  gfx::Size close_button_size(close_button()->GetPreferredSize());
  content_height = std::max(content_height, close_button_size.height());

  // Size the Favicon.
  showing_icon_ = ShouldShowIcon();
  if (showing_icon_) {
    // Use the size of the favicon as apps use a bigger favicon size.
    int favicon_top = top_padding() + content_height / 2 - tab_icon_size() / 2;
    int favicon_left = lb.x();
    favicon_bounds_.SetRect(favicon_left, favicon_top,
                            tab_icon_size(), tab_icon_size());
    if (data().mini && width() < kMiniTabRendererAsNormalTabWidth) {
      // Adjust the location of the favicon when transitioning from a normal
      // tab to a mini-tab.
      int mini_delta = kMiniTabRendererAsNormalTabWidth - GetMiniWidth();
      int ideal_delta = width() - GetMiniWidth();
      if (ideal_delta < mini_delta) {
        int ideal_x = (GetMiniWidth() - tab_icon_size()) / 2;
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
    int close_button_top = top_padding() + kCloseButtonVertFuzz +
        (content_height - close_button_size.height()) / 2;
    // If the ratio of the close button size to tab width exceeds the maximum.
    // The close button should be as large as possible so that there is a larger
    // hit-target for touch events. So the close button bounds extends to the
    // edges of the tab. However, the larger hit-target should be active only
    // for mouse events, and the close-image should show up in the right place.
    // So a border is added to the button with necessary padding. The close
    // button (BaseTab::TabCloseButton) makes sure the padding is a hit-target
    // only for touch events.
    int top_border = close_button_top;
    int bottom_border = height() - (close_button_size.height() + top_border);
    int left_border = kCloseButtonHorzFuzz;
    int right_border = width() - (lb.width() + close_button_size.width() +
        left_border);
    close_button()->set_border(views::Border::CreateEmptyBorder(top_border,
        left_border, bottom_border, right_border));
    close_button()->SetBounds(lb.width(), 0,
        close_button_size.width() + left_border + right_border,
        close_button_size.height() + top_border + bottom_border);
    close_button()->SetVisible(true);
  } else {
    close_button()->SetBounds(0, 0, 0, 0);
    close_button()->SetVisible(false);
  }

  int title_left = favicon_bounds_.right() + kFaviconTitleSpacing;
  int title_top = top_padding() + kTitleTextOffsetY +
      (content_height - font_height()) / 2;
  // Size the Title text to fill the remaining space.
  if (!data().mini || width() >= kMiniTabRendererAsNormalTabWidth) {
    // If the user has big fonts, the title will appear rendered too far down
    // on the y-axis if we use the regular top padding, so we need to adjust it
    // so that the text appears centered.
    gfx::Size minimum_size = GetMinimumUnselectedSize();
    int text_height = title_top + font_height() + bottom_padding();
    if (text_height > minimum_size.height())
      title_top -= (text_height - minimum_size.height()) / 2;

    int title_width;
    if (close_button()->visible()) {
      // The close button has an empty border with some padding (see details
      // above where the close-button's bounds is set). Allow the title to
      // overlap the empty padding.
      title_width = std::max(close_button()->x() +
                             close_button()->GetInsets().left() -
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
  // When the window is maximized we don't want to shave off the edges or top
  // shadow of the tab, such that the user can click anywhere along the top
  // edge of the screen to select a tab.
  bool include_top_shadow = GetWidget() && GetWidget()->IsMaximized();
  TabResources::GetHitTestMask(width(), height(), include_top_shadow, path);
}

bool Tab::GetTooltipTextOrigin(const gfx::Point& p, gfx::Point* origin) const {
  origin->set_x(title_bounds_.x() + 10);
  origin->set_y(-views::TooltipManager::GetTooltipHeight() - 4);
  return true;
}

void Tab::OnMouseMoved(const ui::MouseEvent& event) {
  hover_controller().SetLocation(event.location());
  BaseTab::OnMouseMoved(event);
}

////////////////////////////////////////////////////////////////////////////////
// Tab, private

gfx::ImageSkia* Tab::GetTabBackgroundImage(
    chrome::search::Mode::Type mode) const {
  ui::ThemeProvider* tp = GetThemeProvider();
  if (!tp) {
    DCHECK(tp) << "Unable to get theme provider";
    return NULL;
  }

  if (!controller() || !controller()->IsInstantExtendedAPIEnabled())
    return tp->GetImageSkiaNamed(IDR_THEME_TOOLBAR);

  switch (mode) {
    case chrome::search::Mode::MODE_NTP_LOADING:
    case chrome::search::Mode::MODE_NTP:
      return tp->GetImageSkiaNamed(IDR_THEME_NTP_BACKGROUND);

    case chrome::search::Mode::MODE_SEARCH_SUGGESTIONS:
    case chrome::search::Mode::MODE_SEARCH_RESULTS:
    case chrome::search::Mode::MODE_DEFAULT:
    default:
      return tp->GetImageSkiaNamed(IDR_THEME_TOOLBAR_SEARCH);
  }
}

void Tab::PaintTabBackground(gfx::Canvas* canvas) {
  if (IsActive()) {
    bool fading_in = false;
    // If |gradient_background_opacity| < 1f, paint flat background at full
    // opacity, and only paint gradient background if
    // |gradient_background_opacity| is not 0f;
    // if |gradient_opacity| is 1f, paint the background for the current mode
    // at full opacity.
    if (data().gradient_background_opacity < 1.0f) {
      // Paint flat background of NTP mode.
      PaintActiveTabBackground(canvas,
          GetTabBackgroundImage(chrome::search::Mode::MODE_NTP));
      // We're done if we're not showing gradient background.
      if (data().gradient_background_opacity == 0.0f)
        return;
      // Otherwise, we're fading in the gradient background at
      // |data().gradient_background_opacity|.
      fading_in = true;
      canvas->SaveLayerAlpha(
          static_cast<uint8>(data().gradient_background_opacity * 0xFF),
          gfx::Rect(width(), height()));
    }
    // Paint the background for the current mode.
    PaintActiveTabBackground(canvas, GetTabBackgroundImage(data().mode));
    // If we're fading and have saved canvas, restore it now.
    if (fading_in)
      canvas->Restore();
  } else {
    if (mini_title_animation_.get() && mini_title_animation_->is_animating())
      PaintInactiveTabBackgroundWithTitleChange(canvas);
    else
      PaintInactiveTabBackground(canvas);

    double throb_value = GetThrobValue();
    if (throb_value > 0) {
      canvas->SaveLayerAlpha(static_cast<int>(throb_value * 0xff),
                             gfx::Rect(width(), height()));
      canvas->sk_canvas()->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);
      PaintActiveTabBackground(canvas, GetTabBackgroundImage(data().mode));
      canvas->Restore();
    }
  }
}

void Tab::PaintInactiveTabBackgroundWithTitleChange(gfx::Canvas* canvas) {
  // Render the inactive tab background. We'll use this for clipping.
  gfx::Canvas background_canvas(size(), canvas->scale_factor(), false);
  PaintInactiveTabBackground(&background_canvas);

  gfx::ImageSkia background_image(background_canvas.ExtractImageRep());

  // Draw a radial gradient to hover_canvas.
  gfx::Canvas hover_canvas(size(), canvas->scale_factor(), false);
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
  SkPoint center_point;
  center_point.iset(x, 0);
  SkColor colors[2] = { kMiniTitleChangeGradientColor1,
                        kMiniTitleChangeGradientColor2 };
  SkShader* shader = SkGradientShader::CreateRadial(center_point,
      SkIntToScalar(radius), colors, NULL, 2, SkShader::kClamp_TileMode);
  SkPaint paint;
  paint.setShader(shader);
  shader->unref();
  hover_canvas.DrawRect(gfx::Rect(x - radius, -radius, radius * 2, radius * 2),
                        paint);

  // Draw the radial gradient clipped to the background into hover_image.
  gfx::ImageSkia hover_image = gfx::ImageSkiaOperations::CreateMaskedImage(
      gfx::ImageSkia(hover_canvas.ExtractImageRep()), background_image);

  // Draw the tab background to the canvas.
  canvas->DrawImageInt(background_image, 0, 0);

  // And then the gradient on top of that.
  if (mini_title_animation_->current_part_index() == 2) {
    canvas->SaveLayerAlpha(mini_title_animation_->CurrentValueBetween(255, 0));
    canvas->DrawImageInt(hover_image, 0, 0);
    canvas->Restore();
  } else {
    canvas->DrawImageInt(hover_image, 0, 0);
  }
}

void Tab::PaintInactiveTabBackground(gfx::Canvas* canvas) {
  // The tab image needs to be lined up with the background image
  // so that it feels partially transparent.  These offsets represent the tab
  // position within the frame background image.
  int offset = GetMirroredX() + background_offset_.x();

  int tab_id;
  if (GetWidget() && GetWidget()->GetTopLevelWidget()->ShouldUseNativeFrame()) {
    tab_id = IDR_THEME_TAB_BACKGROUND_V;
  } else {
    tab_id = data().incognito ? IDR_THEME_TAB_BACKGROUND_INCOGNITO :
                                IDR_THEME_TAB_BACKGROUND;
  }

  gfx::ImageSkia* tab_bg = GetThemeProvider()->GetImageSkiaNamed(tab_id);

  TabImage* tab_image =
      controller() && controller()->IsInstantExtendedAPIEnabled() ?
          &tab_active_search_ : &tab_active_;
  TabImage* tab_inactive_image = &tab_inactive_;
  TabImage* alpha = &tab_alpha_;

  // If the theme is providing a custom background image, then its top edge
  // should be at the top of the tab. Otherwise, we assume that the background
  // image is a composited foreground + frame image.
  int bg_offset_y = GetThemeProvider()->HasCustomImage(tab_id) ?
      0 : background_offset_.y();

  // We need a gfx::Canvas object to be able to extract the image from.
  // We draw everything to this canvas and then output it to the canvas
  // parameter in addition to using it to mask the hover glow if needed.
  gfx::Canvas background_canvas(size(), canvas->scale_factor(), false);

  // Draw left edge.  Don't draw over the toolbar, as we're not the foreground
  // tab.
  gfx::ImageSkia tab_l = gfx::ImageSkiaOperations::CreateTiledImage(
      *tab_bg, offset, bg_offset_y, tab_image->l_width, height());
  gfx::ImageSkia theme_l =
      gfx::ImageSkiaOperations::CreateMaskedImage(tab_l, *alpha->image_l);
  background_canvas.DrawImageInt(theme_l,
      0, 0, theme_l.width(), theme_l.height() - kToolbarOverlap,
      0, 0, theme_l.width(), theme_l.height() - kToolbarOverlap,
      false);

  // Draw right edge.  Again, don't draw over the toolbar.
  gfx::ImageSkia tab_r = gfx::ImageSkiaOperations::CreateTiledImage(*tab_bg,
      offset + width() - tab_image->r_width, bg_offset_y,
      tab_image->r_width, height());
  gfx::ImageSkia theme_r =
      gfx::ImageSkiaOperations::CreateMaskedImage(tab_r, *alpha->image_r);
  background_canvas.DrawImageInt(theme_r,
      0, 0, theme_r.width(), theme_r.height() - kToolbarOverlap,
      width() - theme_r.width(), 0, theme_r.width(),
      theme_r.height() - kToolbarOverlap, false);

  // Draw center.  Instead of masking out the top portion we simply skip over
  // it by incrementing by GetDropShadowHeight(), since it's a simple
  // rectangle. And again, don't draw over the toolbar.
  background_canvas.TileImageInt(*tab_bg,
     offset + tab_image->l_width,
     bg_offset_y + drop_shadow_height() + tab_image->y_offset,
     tab_image->l_width,
     drop_shadow_height() + tab_image->y_offset,
     width() - tab_image->l_width - tab_image->r_width,
     height() - drop_shadow_height() - kToolbarOverlap - tab_image->y_offset);

  canvas->DrawImageInt(
      gfx::ImageSkia(background_canvas.ExtractImageRep()), 0, 0);

  if (!GetThemeProvider()->HasCustomImage(tab_id) &&
      hover_controller().ShouldDraw()) {
    hover_controller().Draw(canvas, background_canvas.ExtractImageRep());
  }

  // Now draw the highlights/shadows around the tab edge.
  canvas->DrawImageInt(*tab_inactive_image->image_l, 0, 0);
  canvas->TileImageInt(*tab_inactive_image->image_c,
                       tab_inactive_image->l_width, 0,
                       width() - tab_inactive_image->l_width -
                           tab_inactive_image->r_width,
                       height());
  canvas->DrawImageInt(*tab_inactive_image->image_r,
                       width() - tab_inactive_image->r_width, 0);
}

void Tab::PaintActiveTabBackground(gfx::Canvas* canvas,
                                   gfx::ImageSkia* tab_background) {
  DCHECK(tab_background);

  int offset = GetMirroredX() + background_offset_.x();

  TabImage* tab_image =
      controller() && controller()->IsInstantExtendedAPIEnabled() ?
          &tab_active_search_ : &tab_active_;
  TabImage* alpha = &tab_alpha_;

  // Draw left edge.
  gfx::ImageSkia tab_l = gfx::ImageSkiaOperations::CreateTiledImage(
      *tab_background, offset, 0, tab_image->l_width, height());
  gfx::ImageSkia theme_l =
      gfx::ImageSkiaOperations::CreateMaskedImage(tab_l, *alpha->image_l);
  canvas->DrawImageInt(theme_l, 0, 0);

  // Draw right edge.
  gfx::ImageSkia tab_r = gfx::ImageSkiaOperations::CreateTiledImage(
      *tab_background,
      offset + width() - tab_image->r_width, 0, tab_image->r_width, height());
  gfx::ImageSkia theme_r =
      gfx::ImageSkiaOperations::CreateMaskedImage(tab_r, *alpha->image_r);
  canvas->DrawImageInt(theme_r, width() - tab_image->r_width, 0);

  // Draw center.  Instead of masking out the top portion we simply skip over it
  // by incrementing by GetDropShadowHeight(), since it's a simple rectangle.
  canvas->TileImageInt(*tab_background,
     offset + tab_image->l_width,
     drop_shadow_height() + tab_image->y_offset,
     tab_image->l_width,
     drop_shadow_height() + tab_image->y_offset,
     width() - tab_image->l_width - tab_image->r_width,
     height() - drop_shadow_height() - tab_image->y_offset);

  // Now draw the highlights/shadows around the tab edge.
  canvas->DrawImageInt(*tab_image->image_l, 0, 0);
  canvas->TileImageInt(*tab_image->image_c, tab_image->l_width, 0,
      width() - tab_image->l_width - tab_image->r_width, height());
  canvas->DrawImageInt(*tab_image->image_r, width() - tab_image->r_width, 0);
}

int Tab::IconCapacity() const {
  if (height() < GetMinimumUnselectedSize().height())
    return 0;
  return (width() - left_padding() - right_padding()) / tab_icon_size();
}

bool Tab::ShouldShowIcon() const {
  if (data().mini && height() >= GetMinimumUnselectedSize().height())
    return true;
  if (!data().show_icon) {
    return false;
  } else if (IsActive()) {
    // The active tab clips favicon before close button.
    return IconCapacity() >= 2;
  }
  // Non-selected tabs clip close button before favicon.
  return IconCapacity() >= 1;
}

bool Tab::ShouldShowCloseBox() const {
  // The active tab never clips close button.
  return !data().mini && (IsActive() || IconCapacity() >= 3);
}

double Tab::GetThrobValue() {
  bool is_selected = IsSelected();
  double min = is_selected ? kSelectedTabOpacity : 0;
  double scale = is_selected ? kSelectedTabThrobScale : 1;

  if (pulse_animation() && pulse_animation()->is_animating())
    return pulse_animation()->GetCurrentValue() * kHoverOpacity * scale + min;

  if (hover_controller().ShouldDraw()) {
    return kHoverOpacity * hover_controller().GetAnimationValue() * scale +
        min;
  }

  return is_selected ? kSelectedTabOpacity : 0;
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
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  tab_alpha_.image_l = rb.GetImageSkiaNamed(IDR_TAB_ALPHA_LEFT);
  tab_alpha_.image_r = rb.GetImageSkiaNamed(IDR_TAB_ALPHA_RIGHT);

  tab_active_.image_l = rb.GetImageSkiaNamed(IDR_TAB_ACTIVE_LEFT);
  tab_active_.image_c = rb.GetImageSkiaNamed(IDR_TAB_ACTIVE_CENTER);
  tab_active_.image_r = rb.GetImageSkiaNamed(IDR_TAB_ACTIVE_RIGHT);
  tab_active_.l_width = tab_active_.image_l->width();
  tab_active_.r_width = tab_active_.image_r->width();

  tab_active_search_.image_l =
      rb.GetImageSkiaNamed(IDR_TAB_ACTIVE_LEFT_SEARCH);
  tab_active_search_.image_c =
      rb.GetImageSkiaNamed(IDR_TAB_ACTIVE_CENTER_SEARCH);
  tab_active_search_.image_r =
      rb.GetImageSkiaNamed(IDR_TAB_ACTIVE_RIGHT_SEARCH);
  tab_active_search_.l_width = tab_active_search_.image_l->width();
  tab_active_search_.r_width = tab_active_search_.image_r->width();

  tab_inactive_.image_l = rb.GetImageSkiaNamed(IDR_TAB_INACTIVE_LEFT);
  tab_inactive_.image_c = rb.GetImageSkiaNamed(IDR_TAB_INACTIVE_CENTER);
  tab_inactive_.image_r = rb.GetImageSkiaNamed(IDR_TAB_INACTIVE_RIGHT);
  tab_inactive_.l_width = tab_inactive_.image_l->width();
  tab_inactive_.r_width = tab_inactive_.image_r->width();
}
