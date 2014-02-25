// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/header_painter.h"

#include <vector>

#include "ash/wm/caption_buttons/frame_caption_button_container_view.h"
#include "base/logging.h"  // DCHECK
#include "grit/ash_resources.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using aura::Window;
using views::Widget;

namespace {
// Space between left edge of window and popup window icon.
const int kIconOffsetX = 9;
// Height and width of window icon.
const int kIconSize = 16;
// Space between the title text and the caption buttons.
const int kTitleLogoSpacing = 5;
// Space between window icon and title text.
const int kTitleIconOffsetX = 5;
// Space between window edge and title text, when there is no icon.
const int kTitleNoIconOffsetX = 8;
// Color for the non-maximized window title text.
const SkColor kNonMaximizedWindowTitleTextColor = SkColorSetRGB(40, 40, 40);
// Color for the maximized window title text.
const SkColor kMaximizedWindowTitleTextColor = SK_ColorWHITE;
// Size of header/content separator line below the header image.
const int kHeaderContentSeparatorSize = 1;
// Color of header bottom edge line.
const SkColor kHeaderContentSeparatorColor = SkColorSetRGB(128, 128, 128);
// In the pre-Ash era the web content area had a frame along the left edge, so
// user-generated theme images for the new tab page assume they are shifted
// right relative to the header.  Now that we have removed the left edge frame
// we need to copy the theme image for the window header from a few pixels
// inset to preserve alignment with the NTP image, or else we'll break a bunch
// of existing themes.  We do something similar on OS X for the same reason.
const int kThemeFrameImageInsetX = 5;
// Duration of crossfade animation for activating and deactivating frame.
const int kActivationCrossfadeDurationMs = 200;

// Tiles an image into an area, rounding the top corners. Samples |image|
// starting |image_inset_x| pixels from the left of the image.
void TileRoundRect(gfx::Canvas* canvas,
                   const gfx::ImageSkia& image,
                   const SkPaint& paint,
                   const gfx::Rect& bounds,
                   int top_left_corner_radius,
                   int top_right_corner_radius,
                   int image_inset_x) {
  SkRect rect = gfx::RectToSkRect(bounds);
  const SkScalar kTopLeftRadius = SkIntToScalar(top_left_corner_radius);
  const SkScalar kTopRightRadius = SkIntToScalar(top_right_corner_radius);
  SkScalar radii[8] = {
      kTopLeftRadius, kTopLeftRadius,  // top-left
      kTopRightRadius, kTopRightRadius,  // top-right
      0, 0,   // bottom-right
      0, 0};  // bottom-left
  SkPath path;
  path.addRoundRect(rect, radii, SkPath::kCW_Direction);
  canvas->DrawImageInPath(image, -image_inset_x, 0, path, paint);
}

// Tiles |frame_image| and |frame_overlay_image| into an area, rounding the top
// corners.
void PaintFrameImagesInRoundRect(gfx::Canvas* canvas,
                                 const gfx::ImageSkia* frame_image,
                                 const gfx::ImageSkia* frame_overlay_image,
                                 const SkPaint& paint,
                                 const gfx::Rect& bounds,
                                 int corner_radius,
                                 int image_inset_x) {
  SkXfermode::Mode normal_mode;
  SkXfermode::AsMode(NULL, &normal_mode);

  // If |paint| is using an unusual SkXfermode::Mode (this is the case while
  // crossfading), we must create a new canvas to overlay |frame_image| and
  // |frame_overlay_image| using |normal_mode| and then paint the result
  // using the unusual mode. We try to avoid this because creating a new
  // browser-width canvas is expensive.
  bool fast_path = (!frame_overlay_image ||
      SkXfermode::IsMode(paint.getXfermode(), normal_mode));
  if (fast_path) {
    TileRoundRect(canvas, *frame_image, paint, bounds, corner_radius,
        corner_radius, image_inset_x);

    if (frame_overlay_image) {
      // Adjust |bounds| such that |frame_overlay_image| is not tiled.
      gfx::Rect overlay_bounds = bounds;
      overlay_bounds.Intersect(
          gfx::Rect(bounds.origin(), frame_overlay_image->size()));
      int top_left_corner_radius = corner_radius;
      int top_right_corner_radius = corner_radius;
      if (overlay_bounds.width() < bounds.width() - corner_radius)
        top_right_corner_radius = 0;
      TileRoundRect(canvas, *frame_overlay_image, paint, overlay_bounds,
          top_left_corner_radius, top_right_corner_radius, 0);
    }
  } else {
    gfx::Canvas temporary_canvas(bounds.size(), canvas->image_scale(), false);
    temporary_canvas.TileImageInt(*frame_image,
                                  image_inset_x, 0,
                                  0, 0,
                                  bounds.width(), bounds.height());
    temporary_canvas.DrawImageInt(*frame_overlay_image, 0, 0);
    TileRoundRect(canvas, gfx::ImageSkia(temporary_canvas.ExtractImageRep()),
        paint, bounds, corner_radius, corner_radius, 0);
  }
}

}  // namespace

namespace ash {

///////////////////////////////////////////////////////////////////////////////
// HeaderPainter, public:

HeaderPainter::HeaderPainter()
    : frame_(NULL),
      header_view_(NULL),
      window_icon_(NULL),
      caption_button_container_(NULL),
      header_height_(0),
      top_left_corner_(NULL),
      top_edge_(NULL),
      top_right_corner_(NULL),
      header_left_edge_(NULL),
      header_right_edge_(NULL),
      previous_theme_frame_id_(0),
      previous_theme_frame_overlay_id_(0),
      crossfade_theme_frame_id_(0),
      crossfade_theme_frame_overlay_id_(0) {}

HeaderPainter::~HeaderPainter() {
}

void HeaderPainter::Init(
    Style style,
    views::Widget* frame,
    views::View* header_view,
    views::View* window_icon,
    FrameCaptionButtonContainerView* caption_button_container) {
  DCHECK(frame);
  DCHECK(header_view);
  // window_icon may be NULL.
  DCHECK(caption_button_container);
  style_ = style;
  frame_ = frame;
  header_view_ = header_view;
  window_icon_ = window_icon;
  caption_button_container_ = caption_button_container;

  // Window frame image parts.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  top_left_corner_ = rb.GetImageNamed(
      IDR_AURA_BROWSER_WINDOW_HEADER_SHADE_TOP_LEFT).ToImageSkia();
  top_edge_ = rb.GetImageNamed(
      IDR_AURA_BROWSER_WINDOW_HEADER_SHADE_TOP).ToImageSkia();
  top_right_corner_ = rb.GetImageNamed(
      IDR_AURA_BROWSER_WINDOW_HEADER_SHADE_TOP_RIGHT).ToImageSkia();
  header_left_edge_ = rb.GetImageNamed(
      IDR_AURA_BROWSER_WINDOW_HEADER_SHADE_LEFT).ToImageSkia();
  header_right_edge_ = rb.GetImageNamed(
      IDR_AURA_BROWSER_WINDOW_HEADER_SHADE_RIGHT).ToImageSkia();
}

// static
gfx::Rect HeaderPainter::GetBoundsForClientView(
    int header_height,
    const gfx::Rect& window_bounds) {
  gfx::Rect client_bounds(window_bounds);
  client_bounds.Inset(0, header_height, 0, 0);
  return client_bounds;
}

// static
gfx::Rect HeaderPainter::GetWindowBoundsForClientBounds(
      int header_height,
      const gfx::Rect& client_bounds) {
  gfx::Rect window_bounds(client_bounds);
  window_bounds.Inset(0, -header_height, 0, 0);
  if (window_bounds.y() < 0)
    window_bounds.set_y(0);
  return window_bounds;
}

int HeaderPainter::NonClientHitTest(const gfx::Point& point) const {
  gfx::Point point_in_header_view(point);
  views::View::ConvertPointFromWidget(header_view_, &point_in_header_view);
  if (!GetHeaderLocalBounds().Contains(point_in_header_view))
    return HTNOWHERE;
  if (caption_button_container_->visible()) {
    gfx::Point point_in_caption_button_container(point);
    views::View::ConvertPointFromWidget(caption_button_container_,
        &point_in_caption_button_container);
    int component = caption_button_container_->NonClientHitTest(
        point_in_caption_button_container);
    if (component != HTNOWHERE)
      return component;
  }
  // Caption is a safe default.
  return HTCAPTION;
}

int HeaderPainter::GetMinimumHeaderWidth() const {
  // Ensure we have enough space for the window icon and buttons. We allow
  // the title string to collapse to zero width.
  return GetTitleOffsetX() +
      caption_button_container_->GetMinimumSize().width();
}

int HeaderPainter::GetRightInset() const {
  return caption_button_container_->GetPreferredSize().width();
}

int HeaderPainter::GetThemeBackgroundXInset() const {
  return kThemeFrameImageInsetX;
}

void HeaderPainter::PaintHeader(gfx::Canvas* canvas,
                                int theme_frame_id,
                                int theme_frame_overlay_id) {
  bool initial_paint = (previous_theme_frame_id_ == 0);
  if (!initial_paint &&
      (previous_theme_frame_id_ != theme_frame_id ||
       previous_theme_frame_overlay_id_ != theme_frame_overlay_id)) {
    aura::Window* parent = frame_->GetNativeWindow()->parent();
    // Don't animate the header if the parent (a workspace) is already
    // animating. Doing so results in continually painting during the animation
    // and gives a slower frame rate.
    // TODO(sky): expose a better way to determine this rather than assuming
    // the parent is a workspace.
    bool parent_animating = parent &&
        (parent->layer()->GetAnimator()->IsAnimatingProperty(
            ui::LayerAnimationElement::OPACITY) ||
         parent->layer()->GetAnimator()->IsAnimatingProperty(
             ui::LayerAnimationElement::VISIBILITY));
    if (!parent_animating) {
      crossfade_animation_.reset(new gfx::SlideAnimation(this));
      crossfade_theme_frame_id_ = previous_theme_frame_id_;
      crossfade_theme_frame_overlay_id_ = previous_theme_frame_overlay_id_;
      crossfade_animation_->SetSlideDuration(kActivationCrossfadeDurationMs);
      crossfade_animation_->Show();
    } else {
      crossfade_animation_.reset();
    }
  }

  ui::ThemeProvider* theme_provider = frame_->GetThemeProvider();
  gfx::ImageSkia* theme_frame = theme_provider->GetImageSkiaNamed(
      theme_frame_id);
  gfx::ImageSkia* theme_frame_overlay = NULL;
  if (theme_frame_overlay_id != 0) {
    theme_frame_overlay = theme_provider->GetImageSkiaNamed(
        theme_frame_overlay_id);
  }

  int corner_radius = GetHeaderCornerRadius();
  SkPaint paint;

  if (crossfade_animation_.get() && crossfade_animation_->is_animating()) {
    gfx::ImageSkia* crossfade_theme_frame =
        theme_provider->GetImageSkiaNamed(crossfade_theme_frame_id_);
    gfx::ImageSkia* crossfade_theme_frame_overlay = NULL;
    if (crossfade_theme_frame_overlay_id_ != 0) {
      crossfade_theme_frame_overlay = theme_provider->GetImageSkiaNamed(
          crossfade_theme_frame_overlay_id_);
    }
    if (!crossfade_theme_frame ||
        (crossfade_theme_frame_overlay_id_ != 0 &&
         !crossfade_theme_frame_overlay)) {
      // Reset the animation. This case occurs when the user switches the theme
      // that they are using.
      crossfade_animation_.reset();
    } else {
      int old_alpha = crossfade_animation_->CurrentValueBetween(255, 0);
      int new_alpha = 255 - old_alpha;

      // Draw the old header background, clipping the corners to be rounded.
      paint.setAlpha(old_alpha);
      paint.setXfermodeMode(SkXfermode::kPlus_Mode);
      PaintFrameImagesInRoundRect(canvas,
                                  crossfade_theme_frame,
                                  crossfade_theme_frame_overlay,
                                  paint,
                                  GetHeaderLocalBounds(),
                                  corner_radius,
                                  GetThemeBackgroundXInset());

      paint.setAlpha(new_alpha);
    }
  }

  // Draw the header background, clipping the corners to be rounded.
  PaintFrameImagesInRoundRect(canvas,
                              theme_frame,
                              theme_frame_overlay,
                              paint,
                              GetHeaderLocalBounds(),
                              corner_radius,
                              GetThemeBackgroundXInset());

  previous_theme_frame_id_ = theme_frame_id;
  previous_theme_frame_overlay_id_ = theme_frame_overlay_id;

  // We don't need the extra lightness in the edges when the window is maximized
  // or fullscreen.
  if (frame_->IsMaximized() || frame_->IsFullscreen())
    return;

  // Draw the top corners and edge.
  int top_left_width = top_left_corner_->width();
  int top_left_height = top_left_corner_->height();
  canvas->DrawImageInt(*top_left_corner_,
                       0, 0, top_left_width, top_left_height,
                       0, 0, top_left_width, top_left_height,
                       false);
  canvas->TileImageInt(*top_edge_,
      top_left_width,
      0,
      header_view_->width() - top_left_width - top_right_corner_->width(),
      top_edge_->height());
  int top_right_height = top_right_corner_->height();
  canvas->DrawImageInt(*top_right_corner_,
                       0, 0,
                       top_right_corner_->width(), top_right_height,
                       header_view_->width() - top_right_corner_->width(), 0,
                       top_right_corner_->width(), top_right_height,
                       false);

  // Header left edge.
  int header_left_height = theme_frame->height() - top_left_height;
  canvas->TileImageInt(*header_left_edge_,
                       0, top_left_height,
                       header_left_edge_->width(), header_left_height);

  // Header right edge.
  int header_right_height = theme_frame->height() - top_right_height;
  canvas->TileImageInt(*header_right_edge_,
                       header_view_->width() - header_right_edge_->width(),
                       top_right_height,
                       header_right_edge_->width(),
                       header_right_height);

  // We don't draw edges around the content area.  Web content goes flush
  // to the edge of the window.
}

void HeaderPainter::PaintHeaderContentSeparator(gfx::Canvas* canvas) {
  canvas->FillRect(gfx::Rect(0,
                             header_height_ - kHeaderContentSeparatorSize,
                             header_view_->width(),
                             kHeaderContentSeparatorSize),
                   kHeaderContentSeparatorColor);
}

int HeaderPainter::HeaderContentSeparatorSize() const {
  return kHeaderContentSeparatorSize;
}

void HeaderPainter::PaintTitleBar(gfx::Canvas* canvas,
                                  const gfx::FontList& title_font_list) {
  // The window icon is painted by its own views::View.
  views::WidgetDelegate* delegate = frame_->widget_delegate();
  if (delegate && delegate->ShouldShowWindowTitle()) {
    gfx::Rect title_bounds = GetTitleBounds(title_font_list);
    title_bounds.set_x(header_view_->GetMirroredXForRect(title_bounds));
    SkColor title_color = (frame_->IsMaximized() || frame_->IsFullscreen()) ?
        kMaximizedWindowTitleTextColor : kNonMaximizedWindowTitleTextColor;
    canvas->DrawStringRectWithFlags(delegate->GetWindowTitle(),
                                    title_font_list,
                                    title_color,
                                    title_bounds,
                                    gfx::Canvas::NO_SUBPIXEL_RENDERING);
  }
}

void HeaderPainter::LayoutHeader() {
  // Purposefully set |header_height_| to an invalid value. We cannot use
  // |header_height_| because the computation of |header_height_| may depend
  // on having laid out the window controls.
  header_height_ = -1;

  UpdateCaptionButtonImages();
  caption_button_container_->Layout();

  gfx::Size caption_button_container_size =
      caption_button_container_->GetPreferredSize();
  caption_button_container_->SetBounds(
      header_view_->width() - caption_button_container_size.width(),
      0,
      caption_button_container_size.width(),
      caption_button_container_size.height());

  if (window_icon_) {
    // Vertically center the window icon with respect to the caption button
    // container.
    int icon_offset_y =
        GetCaptionButtonContainerCenterY() - window_icon_->height() / 2;
    window_icon_->SetBounds(kIconOffsetX, icon_offset_y, kIconSize, kIconSize);
  }
}

void HeaderPainter::SchedulePaintForTitle(
    const gfx::FontList& title_font_list) {
  header_view_->SchedulePaintInRect(GetTitleBounds(title_font_list));
}

void HeaderPainter::OnThemeChanged() {
  // We do not cache the images for |previous_theme_frame_id_| and
  // |previous_theme_frame_overlay_id_|. Changing the theme changes the images
  // returned from ui::ThemeProvider for |previous_theme_frame_id_|
  // and |previous_theme_frame_overlay_id_|. Reset the image ids to prevent
  // starting a crossfade animation with these images.
  previous_theme_frame_id_ = 0;
  previous_theme_frame_overlay_id_ = 0;

  if (crossfade_animation_.get() && crossfade_animation_->is_animating()) {
    crossfade_animation_.reset();
    header_view_->SchedulePaintInRect(GetHeaderLocalBounds());
  }
}

///////////////////////////////////////////////////////////////////////////////
// gfx::AnimationDelegate overrides:

void HeaderPainter::AnimationProgressed(const gfx::Animation* animation) {
  header_view_->SchedulePaintInRect(GetHeaderLocalBounds());
}

///////////////////////////////////////////////////////////////////////////////
// HeaderPainter, private:

void HeaderPainter::UpdateCaptionButtonImages() {
  if (frame_->IsMaximized() || frame_->IsFullscreen()) {
    caption_button_container_->SetButtonImages(
        CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
        IDR_AURA_WINDOW_MAXIMIZED_RESTORE2,
        IDR_AURA_WINDOW_MAXIMIZED_RESTORE2_H,
        IDR_AURA_WINDOW_MAXIMIZED_RESTORE2_P);
    caption_button_container_->SetButtonImages(
        CAPTION_BUTTON_ICON_CLOSE,
        IDR_AURA_WINDOW_MAXIMIZED_CLOSE2,
        IDR_AURA_WINDOW_MAXIMIZED_CLOSE2_H,
        IDR_AURA_WINDOW_MAXIMIZED_CLOSE2_P);
    caption_button_container_->SetButtonImages(
        CAPTION_BUTTON_ICON_LEFT_SNAPPED,
        IDR_AURA_WINDOW_MAXIMIZED_RESTORE2,
        IDR_AURA_WINDOW_MAXIMIZED_RESTORE2_H,
        IDR_AURA_WINDOW_MAXIMIZED_RESTORE2_P);
    caption_button_container_->SetButtonImages(
        CAPTION_BUTTON_ICON_RIGHT_SNAPPED,
        IDR_AURA_WINDOW_MAXIMIZED_RESTORE2,
        IDR_AURA_WINDOW_MAXIMIZED_RESTORE2_H,
        IDR_AURA_WINDOW_MAXIMIZED_RESTORE2_P);
  } else if (style_ == STYLE_BROWSER) {
    caption_button_container_->SetButtonImages(
        CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
        IDR_AURA_WINDOW_MAXIMIZE,
        IDR_AURA_WINDOW_MAXIMIZE_H,
        IDR_AURA_WINDOW_MAXIMIZE_P);
    caption_button_container_->SetButtonImages(
        CAPTION_BUTTON_ICON_CLOSE,
        IDR_AURA_WINDOW_CLOSE,
        IDR_AURA_WINDOW_CLOSE_H,
        IDR_AURA_WINDOW_CLOSE_P);
    caption_button_container_->SetButtonImages(
        CAPTION_BUTTON_ICON_LEFT_SNAPPED,
        IDR_AURA_WINDOW_MAXIMIZE,
        IDR_AURA_WINDOW_MAXIMIZE_H,
        IDR_AURA_WINDOW_MAXIMIZE_P);
    caption_button_container_->SetButtonImages(
        CAPTION_BUTTON_ICON_RIGHT_SNAPPED,
        IDR_AURA_WINDOW_MAXIMIZE,
        IDR_AURA_WINDOW_MAXIMIZE_H,
        IDR_AURA_WINDOW_MAXIMIZE_P);
  } else {
    caption_button_container_->SetButtonImages(
        CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
        IDR_AURA_WINDOW_MAXIMIZED_RESTORE,
        IDR_AURA_WINDOW_MAXIMIZED_RESTORE_H,
        IDR_AURA_WINDOW_MAXIMIZED_RESTORE_P);
    caption_button_container_->SetButtonImages(
        CAPTION_BUTTON_ICON_CLOSE,
        IDR_AURA_WINDOW_MAXIMIZED_CLOSE,
        IDR_AURA_WINDOW_MAXIMIZED_CLOSE_H,
        IDR_AURA_WINDOW_MAXIMIZED_CLOSE_P);
    caption_button_container_->SetButtonImages(
        CAPTION_BUTTON_ICON_LEFT_SNAPPED,
        IDR_AURA_WINDOW_MAXIMIZED_RESTORE,
        IDR_AURA_WINDOW_MAXIMIZED_RESTORE_H,
        IDR_AURA_WINDOW_MAXIMIZED_RESTORE_P);
    caption_button_container_->SetButtonImages(
        CAPTION_BUTTON_ICON_RIGHT_SNAPPED,
        IDR_AURA_WINDOW_MAXIMIZED_RESTORE,
        IDR_AURA_WINDOW_MAXIMIZED_RESTORE_H,
        IDR_AURA_WINDOW_MAXIMIZED_RESTORE_P);
  }

  caption_button_container_->SetButtonImages(
      CAPTION_BUTTON_ICON_MINIMIZE,
      IDR_AURA_WINDOW_MINIMIZE_SHORT,
      IDR_AURA_WINDOW_MINIMIZE_SHORT_H,
      IDR_AURA_WINDOW_MINIMIZE_SHORT_P);
}

gfx::Rect HeaderPainter::GetHeaderLocalBounds() const {
  return gfx::Rect(header_view_->width(), header_height_);
}

int HeaderPainter::GetTitleOffsetX() const {
  return window_icon_ ?
      window_icon_->bounds().right() + kTitleIconOffsetX :
      kTitleNoIconOffsetX;
}

int HeaderPainter::GetCaptionButtonContainerCenterY() const {
  return caption_button_container_->y() +
      caption_button_container_->height() / 2;
}

int HeaderPainter::GetHeaderCornerRadius() const {
  bool square_corners = (frame_->IsMaximized() || frame_->IsFullscreen());
  const int kCornerRadius = 2;
  return square_corners ? 0 : kCornerRadius;
}

gfx::Rect HeaderPainter::GetTitleBounds(const gfx::FontList& title_font_list) {
  int title_x = GetTitleOffsetX();
  // Center the text with respect to the caption button container. This way it
  // adapts to the caption button height and aligns exactly with the window
  // icon. Don't use |window_icon_| for this computation as it may be NULL.
  int title_y =
      GetCaptionButtonContainerCenterY() - title_font_list.GetHeight() / 2;
  return gfx::Rect(
      title_x,
      std::max(0, title_y),
      std::max(0, caption_button_container_->x() - kTitleLogoSpacing - title_x),
      title_font_list.GetHeight());
}

}  // namespace ash
