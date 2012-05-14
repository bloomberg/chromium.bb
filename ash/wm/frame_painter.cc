// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/frame_painter.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"  // DCHECK
#include "grit/ui_resources.h"
#include "grit/ui_resources_standard.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {
// TODO(jamescook): Border is specified to be a single pixel overlapping
// the web content and may need to be built into the shadow layers instead.
const int kBorderThickness = 0;
// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of each edge triggers diagonal resizing.
const int kResizeAreaCornerSize = 16;
// Ash windows do not have a traditional visible window frame.  Window content
// extends to the edge of the window.  We consider a small region outside the
// window bounds and an even smaller region overlapping the window to be the
// "non-client" area and use it for resizing.
const int kResizeOutsideBoundsSize = 6;
const int kResizeInsideBoundsSize = 1;
// Space between left edge of window and popup window icon.
const int kIconOffsetX = 4;
// Space between top of window and popup window icon.
const int kIconOffsetY = 6;
// Height and width of window icon.
const int kIconSize = 16;
// Space between the title text and the caption buttons.
const int kTitleLogoSpacing = 5;
// Space between window icon and title text.
const int kTitleIconOffsetX = 4;
// Space between window edge and title text, when there is no icon.
const int kTitleNoIconOffsetX = 8;
// Space between title text and top of window.
const int kTitleOffsetY = 7;
// Color for the title text.
const SkColor kTitleTextColor = SkColorSetRGB(40, 40, 40);
// Size of header/content separator line below the header image.
const int kHeaderContentSeparatorSize = 1;
// Color of header bottom edge line.
const SkColor kHeaderContentSeparatorColor = SkColorSetRGB(128, 128, 128);
// Space between close button and right edge of window.
const int kCloseButtonOffsetX = 0;
// Space between close button and top edge of window.
const int kCloseButtonOffsetY = 0;
// The size and close buttons are designed to slightly overlap in order
// to do fancy hover highlighting.
const int kButtonOverlap = 1;
// In the pre-Ash era the web content area had a frame along the left edge, so
// user-generated theme images for the new tab page assume they are shifted
// right relative to the header.  Now that we have removed the left edge frame
// we need to copy the theme image for the window header from a few pixels
// inset to preserve alignment with the NTP image, or else we'll break a bunch
// of existing themes.  We do something similar on OS X for the same reason.
const int kThemeFrameBitmapOffsetX = 5;
// Duration of crossfade animation for activating and deactivating frame.
const int kActivationCrossfadeDurationMs = 200;
// Alpha/opacity value for fully-opaque headers.
const int kFullyOpaque = 255;

// Tiles an image into an area, rounding the top corners.  Samples the |bitmap|
// starting |bitmap_offset_x| pixels from the left of the image.
void TileRoundRect(gfx::Canvas* canvas,
                   int x, int y, int w, int h,
                   SkPaint* paint,
                   const SkBitmap& bitmap,
                   int corner_radius,
                   int bitmap_offset_x) {
  // To get the shader to sample the image |inset_y| pixels in but tile across
  // the whole image, we adjust the target rectangle for the shader to the right
  // and translate the canvas left to compensate.
  SkRect rect;
  rect.iset(x + bitmap_offset_x, y, x + bitmap_offset_x + w, y + h);
  const SkScalar kRadius = SkIntToScalar(corner_radius);
  SkScalar radii[8] = {
      kRadius, kRadius,  // top-left
      kRadius, kRadius,  // top-right
      0, 0,   // bottom-right
      0, 0};  // bottom-left
  SkPath path;
  path.addRoundRect(rect, radii, SkPath::kCW_Direction);

  SkShader* shader = SkShader::CreateBitmapShader(bitmap,
                                                  SkShader::kRepeat_TileMode,
                                                  SkShader::kRepeat_TileMode);
  paint->setShader(shader);
  // CreateBitmapShader returns a Shader with a reference count of one, we
  // need to unref after paint takes ownership of the shader.
  shader->unref();
  // Adjust canvas to compensate for image sampling offset, draw, then adjust
  // back. This is cheaper than pushing/popping the entire canvas state.
  canvas->sk_canvas()->translate(SkIntToScalar(-bitmap_offset_x), 0);
  canvas->DrawPath(path, *paint);
  canvas->sk_canvas()->translate(SkIntToScalar(bitmap_offset_x), 0);
}

// Returns true if |window| is a visible, normal window.
bool IsVisibleNormalWindow(aura::Window* window) {
  // We must use TargetVisibility() because windows animate in and out and
  // IsVisible() also tracks the layer visibility state.
  return window &&
    window->TargetVisibility() &&
    (window->type() == aura::client::WINDOW_TYPE_NORMAL ||
     window->type() == aura::client::WINDOW_TYPE_PANEL);
}

}  // namespace

namespace ash {

// static
int FramePainter::kActiveWindowOpacity = 230;  // "Linus-approved" values
int FramePainter::kInactiveWindowOpacity = 204;
int FramePainter::kSoloWindowOpacity = 51;
std::set<FramePainter*>* FramePainter::instances_ = NULL;

///////////////////////////////////////////////////////////////////////////////
// FramePainter, public:

FramePainter::FramePainter()
    : frame_(NULL),
      window_icon_(NULL),
      size_button_(NULL),
      close_button_(NULL),
      window_(NULL),
      button_separator_(NULL),
      top_left_corner_(NULL),
      top_edge_(NULL),
      top_right_corner_(NULL),
      header_left_edge_(NULL),
      header_right_edge_(NULL),
      previous_theme_frame_id_(0),
      previous_opacity_(0),
      crossfade_theme_frame_id_(0),
      crossfade_opacity_(0),
      crossfade_animation_(NULL),
      size_button_behavior_(SIZE_BUTTON_MAXIMIZES) {
  if (!instances_)
    instances_ = new std::set<FramePainter*>();
  instances_->insert(this);
}

FramePainter::~FramePainter() {
  // Sometimes we are destroyed before the window closes, so ensure we clean up.
  if (window_)
    window_->RemoveObserver(this);
  instances_->erase(this);
}

void FramePainter::Init(views::Widget* frame,
                        views::View* window_icon,
                        views::ImageButton* size_button,
                        views::ImageButton* close_button,
                        SizeButtonBehavior behavior) {
  DCHECK(frame);
  // window_icon may be NULL.
  DCHECK(size_button);
  DCHECK(close_button);
  frame_ = frame;
  window_icon_ = window_icon;
  size_button_ = size_button;
  close_button_ = close_button;
  size_button_behavior_ = behavior;

  // Window frame image parts.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  button_separator_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_BUTTON_SEPARATOR).ToSkBitmap();
  top_left_corner_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_TOP_LEFT).ToSkBitmap();
  top_edge_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_TOP).ToSkBitmap();
  top_right_corner_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_TOP_RIGHT).ToSkBitmap();
  header_left_edge_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_LEFT).ToSkBitmap();
  header_right_edge_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_RIGHT).ToSkBitmap();

  window_ = frame->GetNativeWindow();
  // Ensure we get resize cursors for a few pixels outside our bounds.
  window_->set_hit_test_bounds_override_outer(
      gfx::Insets(-kResizeOutsideBoundsSize, -kResizeOutsideBoundsSize,
                  -kResizeOutsideBoundsSize, -kResizeOutsideBoundsSize));
  // Ensure we get resize cursors just inside our bounds as well.
  window_->set_hit_test_bounds_override_inner(
      gfx::Insets(kResizeInsideBoundsSize, kResizeInsideBoundsSize,
                  kResizeInsideBoundsSize, kResizeInsideBoundsSize));

  // Watch for maximize/restore/fullscreen state changes.  Observer removes
  // itself in OnWindowDestroying() below, or in the destructor if we go away
  // before the window.
  window_->AddObserver(this);
}

gfx::Rect FramePainter::GetBoundsForClientView(
    int top_height,
    const gfx::Rect& window_bounds) const {
  return gfx::Rect(
      kBorderThickness,
      top_height,
      std::max(0, window_bounds.width() - (2 * kBorderThickness)),
      std::max(0, window_bounds.height() - top_height - kBorderThickness));
}

gfx::Rect FramePainter::GetWindowBoundsForClientBounds(
    int top_height,
    const gfx::Rect& client_bounds) const {
  return gfx::Rect(std::max(0, client_bounds.x() - kBorderThickness),
                   std::max(0, client_bounds.y() - top_height),
                   client_bounds.width() + (2 * kBorderThickness),
                   client_bounds.height() + top_height + kBorderThickness);
}

int FramePainter::NonClientHitTest(views::NonClientFrameView* view,
                                   const gfx::Point& point) {
  gfx::Rect expanded_bounds = view->bounds();
  expanded_bounds.Inset(-kResizeOutsideBoundsSize, -kResizeOutsideBoundsSize);
  if (!expanded_bounds.Contains(point))
    return HTNOWHERE;

  // No avatar button.

  // Check the frame first, as we allow a small area overlapping the contents
  // to be used for resize handles.
  bool can_ever_resize = frame_->widget_delegate() ?
      frame_->widget_delegate()->CanResize() :
      false;
  // Don't allow overlapping resize handles when the window is maximized or
  // fullscreen, as it can't be resized in those states.
  int resize_border =
      frame_->IsMaximized() || frame_->IsFullscreen() ? 0 :
      kResizeInsideBoundsSize;
  int frame_component = view->GetHTComponentForFrame(point,
                                                     resize_border,
                                                     resize_border,
                                                     kResizeAreaCornerSize,
                                                     kResizeAreaCornerSize,
                                                     can_ever_resize);
  if (frame_component != HTNOWHERE)
    return frame_component;

  int client_component = frame_->client_view()->NonClientHitTest(point);
  if (client_component != HTNOWHERE)
    return client_component;

  // Then see if the point is within any of the window controls.
  if (close_button_->visible() &&
      close_button_->GetMirroredBounds().Contains(point))
    return HTCLOSE;
  if (size_button_->visible() &&
      size_button_->GetMirroredBounds().Contains(point))
    return HTMAXBUTTON;

  // Caption is a safe default.
  return HTCAPTION;
}

gfx::Size FramePainter::GetMinimumSize(views::NonClientFrameView* view) {
  gfx::Size min_size = frame_->client_view()->GetMinimumSize();
  // Ensure we can display the top of the caption area.
  gfx::Rect client_bounds = view->GetBoundsForClientView();
  min_size.Enlarge(0, client_bounds.y());
  // Ensure we have enough space for the window icon and buttons.  We allow
  // the title string to collapse to zero width.
  int title_width = GetTitleOffsetX() +
      size_button_->width() -
      kButtonOverlap +
      close_button_->width();
  if (title_width > min_size.width())
    min_size.set_width(title_width);
  return min_size;
}

void FramePainter::PaintHeader(views::NonClientFrameView* view,
                               gfx::Canvas* canvas,
                               HeaderMode header_mode,
                               int theme_frame_id,
                               const SkBitmap* theme_frame_overlay) {
  if (previous_theme_frame_id_ != 0 &&
      previous_theme_frame_id_ != theme_frame_id) {
    crossfade_animation_.reset(new ui::SlideAnimation(this));
    crossfade_theme_frame_id_ = previous_theme_frame_id_;
    crossfade_opacity_ = previous_opacity_;
    crossfade_animation_->SetSlideDuration(kActivationCrossfadeDurationMs);
    crossfade_animation_->Show();
  }

  int opacity =
      GetHeaderOpacity(header_mode, theme_frame_id, theme_frame_overlay);
  ui::ThemeProvider* theme_provider = frame_->GetThemeProvider();
  SkBitmap* theme_frame = theme_provider->GetBitmapNamed(theme_frame_id);
  header_frame_bounds_ = gfx::Rect(0, 0, view->width(), theme_frame->height());

  const int kCornerRadius = 2;
  SkPaint paint;

  if (crossfade_animation_.get() && crossfade_animation_->is_animating()) {
    SkBitmap* crossfade_theme_frame =
        theme_provider->GetBitmapNamed(crossfade_theme_frame_id_);
    if (crossfade_theme_frame) {
      double current_value = crossfade_animation_->GetCurrentValue();
      int old_alpha = (1 - current_value) * crossfade_opacity_;
      int new_alpha = current_value * opacity;

      // Draw the old header background, clipping the corners to be rounded.
      paint.setAlpha(old_alpha);
      paint.setXfermodeMode(SkXfermode::kPlus_Mode);
      TileRoundRect(canvas,
                    0, 0, view->width(), theme_frame->height(),
                    &paint,
                    *crossfade_theme_frame,
                    kCornerRadius,
                    kThemeFrameBitmapOffsetX);

      paint.setAlpha(new_alpha);
    } else {
      crossfade_animation_.reset();
      paint.setAlpha(opacity);
    }
  } else {
    paint.setAlpha(opacity);
  }

  // Draw the header background, clipping the corners to be rounded.
  TileRoundRect(canvas,
                0, 0, view->width(), theme_frame->height(),
                &paint,
                *theme_frame,
                kCornerRadius,
                kThemeFrameBitmapOffsetX);

  previous_theme_frame_id_ = theme_frame_id;
  previous_opacity_ = opacity;

  // Draw the theme frame overlay, if available.
  if (theme_frame_overlay)
    canvas->DrawBitmapInt(*theme_frame_overlay, 0, 0);

  // Separator between the maximize and close buttons.  It overlaps the left
  // edge of the close button.
  canvas->DrawBitmapInt(*button_separator_,
                        close_button_->x(),
                        close_button_->y());

  // We don't need the extra lightness in the edges when we're maximized.
  if (frame_->IsMaximized())
    return;

  // Draw the top corners and edge.
  int top_left_height = top_left_corner_->height();
  canvas->DrawBitmapInt(*top_left_corner_,
                        0, 0, top_left_corner_->width(), top_left_height,
                        0, 0, top_left_corner_->width(), top_left_height,
                        false);
  canvas->TileImageInt(*top_edge_,
      top_left_corner_->width(),
      0,
      view->width() - top_left_corner_->width() - top_right_corner_->width(),
      top_edge_->height());
  int top_right_height = top_right_corner_->height();
  canvas->DrawBitmapInt(*top_right_corner_,
                        0, 0,
                        top_right_corner_->width(), top_right_height,
                        view->width() - top_right_corner_->width(), 0,
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
                       view->width() - header_right_edge_->width(),
                       top_right_height,
                       header_right_edge_->width(),
                       header_right_height);

  // We don't draw edges around the content area.  Web content goes flush
  // to the edge of the window.
}

void FramePainter::PaintHeaderContentSeparator(views::NonClientFrameView* view,
                                               gfx::Canvas* canvas) {
  // Paint the line just above the content area.
  gfx::Rect client_bounds = view->GetBoundsForClientView();
  canvas->FillRect(gfx::Rect(client_bounds.x(),
                             client_bounds.y() - kHeaderContentSeparatorSize,
                             client_bounds.width(),
                             kHeaderContentSeparatorSize),
                   kHeaderContentSeparatorColor);
}

int FramePainter::HeaderContentSeparatorSize() const {
  return kHeaderContentSeparatorSize;
}

void FramePainter::PaintTitleBar(views::NonClientFrameView* view,
                                 gfx::Canvas* canvas,
                                 const gfx::Font& title_font) {
  // The window icon is painted by its own views::View.
  views::WidgetDelegate* delegate = frame_->widget_delegate();
  if (delegate && delegate->ShouldShowWindowTitle()) {
    int title_x = GetTitleOffsetX();
    gfx::Rect title_bounds(
        title_x,
        kTitleOffsetY,
        std::max(0, size_button_->x() - kTitleLogoSpacing - title_x),
        title_font.GetHeight());
    canvas->DrawStringInt(delegate->GetWindowTitle(),
                          title_font,
                          kTitleTextColor,
                          view->GetMirroredXForRect(title_bounds),
                          title_bounds.y(),
                          title_bounds.width(),
                          title_bounds.height(),
                          gfx::Canvas::NO_SUBPIXEL_RENDERING);
  }
}

void FramePainter::LayoutHeader(views::NonClientFrameView* view,
                                bool maximized_layout) {
  // The maximized layout uses shorter buttons.
  if (maximized_layout) {
    SetButtonImages(close_button_,
                    IDR_AURA_WINDOW_MAXIMIZED_CLOSE,
                    IDR_AURA_WINDOW_MAXIMIZED_CLOSE_H,
                    IDR_AURA_WINDOW_MAXIMIZED_CLOSE_P);
    if (size_button_behavior_ == SIZE_BUTTON_MINIMIZES) {
      SetButtonImages(size_button_,
                      IDR_AURA_WINDOW_MAXIMIZED_MINIMIZE,
                      IDR_AURA_WINDOW_MAXIMIZED_MINIMIZE_H,
                      IDR_AURA_WINDOW_MAXIMIZED_MINIMIZE_P);
    } else {
      SetButtonImages(size_button_,
                      IDR_AURA_WINDOW_MAXIMIZED_RESTORE,
                      IDR_AURA_WINDOW_MAXIMIZED_RESTORE_H,
                      IDR_AURA_WINDOW_MAXIMIZED_RESTORE_P);
    }
  } else {
    SetButtonImages(close_button_,
                    IDR_AURA_WINDOW_CLOSE,
                    IDR_AURA_WINDOW_CLOSE_H,
                    IDR_AURA_WINDOW_CLOSE_P);
    // TODO(jamescook): If we ever have normal-layout windows (with the
    // standard 35 pixel tall headers) that can only minimize, we'll need art
    // assets for SIZE_BUTTON_MINIMIZES.  As of R19 we don't use them.
    SetButtonImages(size_button_,
                    IDR_AURA_WINDOW_MAXIMIZE,
                    IDR_AURA_WINDOW_MAXIMIZE_H,
                    IDR_AURA_WINDOW_MAXIMIZE_P);
  }

  gfx::Size close_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(
      view->width() - close_size.width() - kCloseButtonOffsetX,
      kCloseButtonOffsetY,
      close_size.width(),
      close_size.height());

  gfx::Size size_button_size = size_button_->GetPreferredSize();
  size_button_->SetBounds(
      close_button_->x() - size_button_size.width() + kButtonOverlap,
      close_button_->y(),
      size_button_size.width(),
      size_button_size.height());

  if (window_icon_)
    window_icon_->SetBoundsRect(
        gfx::Rect(kIconOffsetX, kIconOffsetY, kIconSize, kIconSize));
}

///////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver overrides:

void FramePainter::OnWindowPropertyChanged(aura::Window* window,
                                           const void* key,
                                           intptr_t old) {
  if (key != aura::client::kShowStateKey)
    return;

  // Maximized and fullscreen windows don't want resize handles overlapping the
  // content area, because when the user moves the cursor to the right screen
  // edge we want them to be able to hit the scroll bar.
  if (ash::wm::IsWindowMaximized(window) ||
      ash::wm::IsWindowFullscreen(window)) {
    window->set_hit_test_bounds_override_inner(gfx::Insets());
  } else {
    window->set_hit_test_bounds_override_inner(
        gfx::Insets(kResizeInsideBoundsSize, kResizeInsideBoundsSize,
                    kResizeInsideBoundsSize, kResizeInsideBoundsSize));
  }
}

void FramePainter::OnWindowVisibilityChanged(aura::Window* window,
                                             bool visible) {
  // Hiding a window may trigger the solo window appearance in a different
  // window.
  if (!visible && UseSoloWindowHeader())
    SchedulePaintForSoloWindow();
}

void FramePainter::OnWindowDestroying(aura::Window* destroying) {
  DCHECK_EQ(window_, destroying);
  // Must be removed here and not in the destructor, as the aura::Window is
  // already destroyed when our destructor runs.
  window_->RemoveObserver(this);
  window_ = NULL;

  // For purposes of painting and solo window computation, we're done.
  instances_->erase(this);

  // If we have two or more windows open and we close this one, we might trigger
  // the solo window appearance for another window.
  if (UseSoloWindowHeader())
    SchedulePaintForSoloWindow();
}

///////////////////////////////////////////////////////////////////////////////
// ui::AnimationDelegate overrides:

void FramePainter::AnimationProgressed(const ui::Animation* animation) {
  frame_->SchedulePaintInRect(gfx::Rect(header_frame_bounds_));
}

///////////////////////////////////////////////////////////////////////////////
// FramePainter, private:

void FramePainter::SetButtonImages(views::ImageButton* button,
                                   int normal_bitmap_id,
                                   int hot_bitmap_id,
                                   int pushed_bitmap_id) {
  ui::ThemeProvider* theme_provider = frame_->GetThemeProvider();
  button->SetImage(views::CustomButton::BS_NORMAL,
                   theme_provider->GetImageSkiaNamed(normal_bitmap_id));
  button->SetImage(views::CustomButton::BS_HOT,
                   theme_provider->GetImageSkiaNamed(hot_bitmap_id));
  button->SetImage(views::CustomButton::BS_PUSHED,
                   theme_provider->GetImageSkiaNamed(pushed_bitmap_id));
}

int FramePainter::GetTitleOffsetX() const {
  return window_icon_ ?
      window_icon_->bounds().right() + kTitleIconOffsetX :
      kTitleNoIconOffsetX;
}

int FramePainter::GetHeaderOpacity(HeaderMode header_mode,
                                   int theme_frame_id,
                                   const SkBitmap* theme_frame_overlay) {
  // User-provided themes are painted fully opaque.
  if (frame_->GetThemeProvider()->HasCustomImage(theme_frame_id))
    return kFullyOpaque;
  if (theme_frame_overlay)
    return kFullyOpaque;

  // Single browser window is very transparent.
  if (UseSoloWindowHeader())
    return kSoloWindowOpacity;

  // Otherwise, change transparency based on window activation status.
  if (header_mode == ACTIVE)
    return kActiveWindowOpacity;
  return kInactiveWindowOpacity;
}

// static
bool FramePainter::UseSoloWindowHeader() {
  int window_count = 0;
  for (std::set<FramePainter*>::const_iterator it = instances_->begin();
       it != instances_->end();
       ++it) {
    if (IsVisibleNormalWindow((*it)->window_)) {
      window_count++;
      if (window_count > 1)
        return false;
    }
  }
  return window_count == 1;
}

// static
void FramePainter::SchedulePaintForSoloWindow() {
  for (std::set<FramePainter*>::const_iterator it = instances_->begin();
       it != instances_->end();
       ++it) {
    FramePainter* painter = *it;
    if (IsVisibleNormalWindow(painter->window_))
      painter->frame_->non_client_view()->SchedulePaint();
  }
}

}  // namespace ash
