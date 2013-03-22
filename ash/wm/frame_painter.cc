// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/frame_painter.h"

#include <vector>

#include "ash/ash_constants.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "base/logging.h"  // DCHECK
#include "grit/ash_resources.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/hit_test.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using aura::RootWindow;
using aura::Window;
using views::Widget;

namespace {
// TODO(jamescook): Border is specified to be a single pixel overlapping
// the web content and may need to be built into the shadow layers instead.
const int kBorderThickness = 0;
// Space between left edge of window and popup window icon.
const int kIconOffsetX = 9;
// Space between top of window and popup window icon.
const int kIconOffsetY = 5;
// Height and width of window icon.
const int kIconSize = 16;
// Space between the title text and the caption buttons.
const int kTitleLogoSpacing = 5;
// Space between window icon and title text.
const int kTitleIconOffsetX = 4;
// Space between window edge and title text, when there is no icon.
const int kTitleNoIconOffsetX = 8;
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
const int kSizeButtonOffsetX = -1;
// In the pre-Ash era the web content area had a frame along the left edge, so
// user-generated theme images for the new tab page assume they are shifted
// right relative to the header.  Now that we have removed the left edge frame
// we need to copy the theme image for the window header from a few pixels
// inset to preserve alignment with the NTP image, or else we'll break a bunch
// of existing themes.  We do something similar on OS X for the same reason.
const int kThemeFrameImageInsetX = 5;
// Duration of crossfade animation for activating and deactivating frame.
const int kActivationCrossfadeDurationMs = 200;
// Alpha/opacity value for fully-opaque headers.
const int kFullyOpaque = 255;

// Tiles an image into an area, rounding the top corners. Samples the |bitmap|
// starting |bitmap_offset_x| pixels from the left of the image.
void TileRoundRect(gfx::Canvas* canvas,
                   int x, int y, int w, int h,
                   const SkPaint& paint,
                   const gfx::ImageSkia& image,
                   int corner_radius,
                   int image_inset_x) {
  // To get the shader to sample the image |inset_y| pixels in but tile across
  // the whole image, we adjust the target rectangle for the shader to the right
  // and translate the canvas left to compensate.
  SkRect rect;
  rect.iset(x, y, x + w, y + h);
  const SkScalar kRadius = SkIntToScalar(corner_radius);
  SkScalar radii[8] = {
      kRadius, kRadius,  // top-left
      kRadius, kRadius,  // top-right
      0, 0,   // bottom-right
      0, 0};  // bottom-left
  SkPath path;
  path.addRoundRect(rect, radii, SkPath::kCW_Direction);
  canvas->DrawImageInPath(image, -image_inset_x, 0, path, paint);
}

// Returns true if |child| and all ancestors are visible.
bool IsVisibleToRoot(Window* child) {
  for (Window* window = child; window; window = window->parent()) {
    // We must use TargetVisibility() because windows animate in and out and
    // IsVisible() also tracks the layer visibility state.
    if (!window->TargetVisibility())
      return false;
  }
  return true;
}

// Returns true if |window| is a visible, normal window.
bool IsVisibleNormalWindow(aura::Window* window) {
  // Test visibility up to root in case the whole workspace is hidden.
  return window &&
    IsVisibleToRoot(window) &&
    (window->type() == aura::client::WINDOW_TYPE_NORMAL ||
     window->type() == aura::client::WINDOW_TYPE_PANEL);
}

// Returns a list of windows in |root_window|| that potentially could have
// a transparent solo-window header.
std::vector<Window*> GetWindowsForSoloHeaderUpdate(RootWindow* root_window) {
  std::vector<Window*> windows;
  // During shutdown there may not be a workspace controller. In that case
  // we don't care about updating any windows.
  ash::internal::WorkspaceController* workspace_controller =
      ash::GetRootWindowController(root_window)->workspace_controller();
  if (workspace_controller) {
    // Avoid memory allocations for typical window counts.
    windows.reserve(16);
    // Collect windows from the active workspace.
    Window* workspace = workspace_controller->GetActiveWorkspaceWindow();
    windows.insert(windows.end(),
                   workspace->children().begin(),
                   workspace->children().end());
    // Collect "always on top" windows.
    Window* top_container =
        ash::Shell::GetContainer(
            root_window, ash::internal::kShellWindowId_AlwaysOnTopContainer);
    windows.insert(windows.end(),
                   top_container->children().begin(),
                   top_container->children().end());
  }
  return windows;
}
}  // namespace

namespace ash {

// static
int FramePainter::kActiveWindowOpacity = 255;  // 1.0
int FramePainter::kInactiveWindowOpacity = 255;  // 1.0
int FramePainter::kSoloWindowOpacity = 77;  // 0.3

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
}

FramePainter::~FramePainter() {
  // Sometimes we are destroyed before the window closes, so ensure we clean up.
  if (window_) {
    window_->RemoveObserver(this);
    aura::RootWindow* root = window_->GetRootWindow();
    if (root)
      root->RemoveObserver(this);
  }
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
      rb.GetImageNamed(IDR_AURA_WINDOW_BUTTON_SEPARATOR).ToImageSkia();
  top_left_corner_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_TOP_LEFT).ToImageSkia();
  top_edge_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_TOP).ToImageSkia();
  top_right_corner_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_TOP_RIGHT).ToImageSkia();
  header_left_edge_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_LEFT).ToImageSkia();
  header_right_edge_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_RIGHT).ToImageSkia();

  window_ = frame->GetNativeWindow();
  // Ensure we get resize cursors for a few pixels outside our bounds.
  window_->SetHitTestBoundsOverrideOuter(
      gfx::Insets(-kResizeOutsideBoundsSize, -kResizeOutsideBoundsSize,
                  -kResizeOutsideBoundsSize, -kResizeOutsideBoundsSize),
      kResizeOutsideBoundsScaleForTouch);
  // Ensure we get resize cursors just inside our bounds as well.
  window_->set_hit_test_bounds_override_inner(
      gfx::Insets(kResizeInsideBoundsSize, kResizeInsideBoundsSize,
                  kResizeInsideBoundsSize, kResizeInsideBoundsSize));

  // Watch for maximize/restore/fullscreen state changes.  Observer removes
  // itself in OnWindowDestroying() below, or in the destructor if we go away
  // before the window.
  window_->AddObserver(this);
  aura::Window* root = window_->GetRootWindow();
  if (root)
    root->AddObserver(this);

  // Solo-window header updates are handled by the workspace controller when
  // this window is added to the active workspace.
}

// static
void FramePainter::UpdateSoloWindowHeader(RootWindow* root_window) {
  // Use a separate function here so callers outside of FramePainter don't need
  // to know about "ignorable_window".
  UpdateSoloWindowInRoot(root_window, NULL /* ignorable_window */);
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
  int outside_bounds = kResizeOutsideBoundsSize;

  if (aura::Env::GetInstance()->is_touch_down())
    outside_bounds *= kResizeOutsideBoundsScaleForTouch;
  expanded_bounds.Inset(-outside_bounds, -outside_bounds);

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
  frame_component = AdjustFrameHitCodeForMaximizedModes(frame_component);
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
      size_button_->width() + kSizeButtonOffsetX +
      close_button_->width() + kCloseButtonOffsetX;
  if (title_width > min_size.width())
    min_size.set_width(title_width);
  return min_size;
}

gfx::Size FramePainter::GetMaximumSize(views::NonClientFrameView* view) {
  return frame_->client_view()->GetMaximumSize();
}

int FramePainter::GetRightInset() const {
  gfx::Size close_size = close_button_->GetPreferredSize();
  gfx::Size size_button_size = size_button_->GetPreferredSize();
  int inset = close_size.width() + kCloseButtonOffsetX +
      size_button_size.width() + kSizeButtonOffsetX;
  return inset;
}

int FramePainter::GetThemeBackgroundXInset() const {
  return kThemeFrameImageInsetX;
}

void FramePainter::PaintHeader(views::NonClientFrameView* view,
                               gfx::Canvas* canvas,
                               HeaderMode header_mode,
                               int theme_frame_id,
                               const gfx::ImageSkia* theme_frame_overlay) {
  if (previous_theme_frame_id_ != 0 &&
      previous_theme_frame_id_ != theme_frame_id) {
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
      crossfade_animation_.reset(new ui::SlideAnimation(this));
      crossfade_theme_frame_id_ = previous_theme_frame_id_;
      crossfade_opacity_ = previous_opacity_;
      crossfade_animation_->SetSlideDuration(kActivationCrossfadeDurationMs);
      crossfade_animation_->Show();
    } else {
      crossfade_animation_.reset();
    }
  }

  int opacity =
      GetHeaderOpacity(header_mode, theme_frame_id, theme_frame_overlay);
  ui::ThemeProvider* theme_provider = frame_->GetThemeProvider();
  gfx::ImageSkia* theme_frame = theme_provider->GetImageSkiaNamed(
      theme_frame_id);
  header_frame_bounds_ = gfx::Rect(0, 0, view->width(), theme_frame->height());

  const int kCornerRadius = 2;
  SkPaint paint;

  if (crossfade_animation_.get() && crossfade_animation_->is_animating()) {
    gfx::ImageSkia* crossfade_theme_frame =
        theme_provider->GetImageSkiaNamed(crossfade_theme_frame_id_);
    if (crossfade_theme_frame) {
      double current_value = crossfade_animation_->GetCurrentValue();
      int old_alpha = (1 - current_value) * crossfade_opacity_;
      int new_alpha = current_value * opacity;

      // Draw the old header background, clipping the corners to be rounded.
      paint.setAlpha(old_alpha);
      paint.setXfermodeMode(SkXfermode::kPlus_Mode);
      TileRoundRect(canvas,
                    0, 0, view->width(), theme_frame->height(),
                    paint,
                    *crossfade_theme_frame,
                    kCornerRadius,
                    GetThemeBackgroundXInset());

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
                paint,
                *theme_frame,
                kCornerRadius,
                GetThemeBackgroundXInset());

  previous_theme_frame_id_ = theme_frame_id;
  previous_opacity_ = opacity;

  // Draw the theme frame overlay, if available.
  if (theme_frame_overlay)
    canvas->DrawImageInt(*theme_frame_overlay, 0, 0);

  // Separator between the maximize and close buttons.  It overlaps the left
  // edge of the close button.
  gfx::Rect divider(close_button_->x(), close_button_->y(),
                    button_separator_->width(), close_button_->height());
  canvas->DrawImageInt(*button_separator_,
                       view->GetMirroredXForRect(divider),
                       close_button_->y());

  // We don't need the extra lightness in the edges when we're at the top edge
  // of the screen or maximized. We have the maximized check as during
  // animations the bounds may not be at 0, but the border shouldn't be drawn.
  //
  // TODO(sky): this isn't quite right. What we really want is a method that
  // returns bounds ignoring transforms on certain windows (such as workspaces)
  // and is relative to the root.
  if (frame_->GetNativeWindow()->bounds().y() == 0 ||
      frame_->IsMaximized() ||
      frame_->IsFullscreen())
    return;

  // Draw the top corners and edge.
  int top_left_height = top_left_corner_->height();
  canvas->DrawImageInt(*top_left_corner_,
                       0, 0, top_left_corner_->width(), top_left_height,
                       0, 0, top_left_corner_->width(), top_left_height,
                       false);
  canvas->TileImageInt(*top_edge_,
      top_left_corner_->width(),
      0,
      view->width() - top_left_corner_->width() - top_right_corner_->width(),
      top_edge_->height());
  int top_right_height = top_right_corner_->height();
  canvas->DrawImageInt(*top_right_corner_,
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
    gfx::Rect title_bounds = GetTitleBounds(view, title_font);
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
                                bool shorter_layout) {
  // The new assets only make sense if the window is actually maximized or
  // fullscreen.
  if (shorter_layout &&
      (frame_->IsMaximized() || frame_->IsFullscreen()) &&
      GetTrackedByWorkspace(frame_->GetNativeWindow())) {
    SetButtonImages(close_button_,
                    IDR_AURA_WINDOW_MAXIMIZED_CLOSE2,
                    IDR_AURA_WINDOW_MAXIMIZED_CLOSE2_H,
                    IDR_AURA_WINDOW_MAXIMIZED_CLOSE2_P);
    // The chat window cannot be restored but only minimized.
    if (size_button_behavior_ == SIZE_BUTTON_MINIMIZES) {
      SetButtonImages(size_button_,
                      IDR_AURA_WINDOW_MINIMIZE_SHORT,
                      IDR_AURA_WINDOW_MINIMIZE_SHORT_H,
                      IDR_AURA_WINDOW_MINIMIZE_SHORT_P);
    } else {
      SetButtonImages(size_button_,
                      IDR_AURA_WINDOW_MAXIMIZED_RESTORE2,
                      IDR_AURA_WINDOW_MAXIMIZED_RESTORE2_H,
                      IDR_AURA_WINDOW_MAXIMIZED_RESTORE2_P);
    }
  } else if (shorter_layout) {
    SetButtonImages(close_button_,
                    IDR_AURA_WINDOW_MAXIMIZED_CLOSE,
                    IDR_AURA_WINDOW_MAXIMIZED_CLOSE_H,
                    IDR_AURA_WINDOW_MAXIMIZED_CLOSE_P);
    // The chat window cannot be restored but only minimized.
    if (size_button_behavior_ == SIZE_BUTTON_MINIMIZES) {
      SetButtonImages(size_button_,
                      IDR_AURA_WINDOW_MINIMIZE_SHORT,
                      IDR_AURA_WINDOW_MINIMIZE_SHORT_H,
                      IDR_AURA_WINDOW_MINIMIZE_SHORT_P);
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
      close_button_->x() - size_button_size.width() - kSizeButtonOffsetX,
      close_button_->y(),
      size_button_size.width(),
      size_button_size.height());

  if (window_icon_) {
    window_icon_->SetBoundsRect(
        gfx::Rect(kIconOffsetX, kIconOffsetY, kIconSize, kIconSize));
  }
}

void FramePainter::SchedulePaintForTitle(views::NonClientFrameView* view,
                                         const gfx::Font& title_font) {
  frame_->non_client_view()->SchedulePaintInRect(
      GetTitleBounds(view, title_font));
}

///////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver overrides:

void FramePainter::OnWindowPropertyChanged(aura::Window* window,
                                           const void* key,
                                           intptr_t old) {
  // When either 'kWindowTrackedByWorkspaceKey' changes or
  // 'kCyclingThroughWorkspacesKey' changes, we are going to paint the header
  // differently. Schedule a paint to ensure everything is updated correctly.
  if (key == internal::kWindowTrackedByWorkspaceKey &&
      GetTrackedByWorkspace(window)) {
    frame_->non_client_view()->SchedulePaint();
  } else if (key == internal::kCyclingThroughWorkspacesKey) {
    frame_->non_client_view()->SchedulePaint();
  }

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
  // Ignore updates from the root window.
  if (window != window_)
    return;

  // Window visibility change may trigger the change of window solo-ness in a
  // different window.
  UpdateSoloWindowInRoot(window_->GetRootWindow(), visible ? NULL : window_);
}

void FramePainter::OnWindowDestroying(aura::Window* destroying) {
  aura::RootWindow* root = window_->GetRootWindow();
  DCHECK(destroying == window_ || destroying == root);

  // Must be removed here and not in the destructor, as the aura::Window is
  // already destroyed when our destructor runs.
  window_->RemoveObserver(this);
  if (root)
    root->RemoveObserver(this);

  // If we have two or more windows open and we close this one, we might trigger
  // the solo window appearance for another window.
  UpdateSoloWindowInRoot(root, window_);

  window_ = NULL;
}

void FramePainter::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds) {
  // Ignore updates from the root window.
  if (window != window_)
    return;

  // TODO(sky): this isn't quite right. What we really want is a method that
  // returns bounds ignoring transforms on certain windows (such as workspaces).
  if (!frame_->IsMaximized() &&
      ((old_bounds.y() == 0 && new_bounds.y() != 0) ||
       (old_bounds.y() != 0 && new_bounds.y() == 0))) {
    SchedulePaintForHeader();
  }
}

void FramePainter::OnWindowAddedToRootWindow(aura::Window* window) {
  DCHECK_EQ(window_, window);
  RootWindow* root = window->GetRootWindow();
  root->AddObserver(this);

  // Needs to trigger the window appearance change if the window moves across
  // root windows and a solo window is already in the new root.
  UpdateSoloWindowInRoot(root, NULL /* ignore_window */);
}

void FramePainter::OnWindowRemovingFromRootWindow(aura::Window* window) {
  DCHECK_EQ(window_, window);
  RootWindow* root = window->GetRootWindow();
  root->RemoveObserver(this);

  // Needs to trigger the window appearance change if the window moves across
  // root windows and only one window is left in the previous root.  Because
  // |window| is not yet moved, |window| has to be ignored.
  UpdateSoloWindowInRoot(root, window);
}

///////////////////////////////////////////////////////////////////////////////
// ui::AnimationDelegate overrides:

void FramePainter::AnimationProgressed(const ui::Animation* animation) {
  frame_->SchedulePaintInRect(gfx::Rect(header_frame_bounds_));
}

///////////////////////////////////////////////////////////////////////////////
// FramePainter, private:

void FramePainter::SetButtonImages(views::ImageButton* button,
                                   int normal_image_id,
                                   int hot_image_id,
                                   int pushed_image_id) {
  ui::ThemeProvider* theme_provider = frame_->GetThemeProvider();
  button->SetImage(views::CustomButton::STATE_NORMAL,
                   theme_provider->GetImageSkiaNamed(normal_image_id));
  button->SetImage(views::CustomButton::STATE_HOVERED,
                   theme_provider->GetImageSkiaNamed(hot_image_id));
  button->SetImage(views::CustomButton::STATE_PRESSED,
                   theme_provider->GetImageSkiaNamed(pushed_image_id));
}

void FramePainter::SetToggledButtonImages(views::ToggleImageButton* button,
                                          int normal_image_id,
                                          int hot_image_id,
                                          int pushed_image_id) {
  ui::ThemeProvider* theme_provider = frame_->GetThemeProvider();
  button->SetToggledImage(views::CustomButton::STATE_NORMAL,
                          theme_provider->GetImageSkiaNamed(normal_image_id));
  button->SetToggledImage(views::CustomButton::STATE_HOVERED,
                          theme_provider->GetImageSkiaNamed(hot_image_id));
  button->SetToggledImage(views::CustomButton::STATE_PRESSED,
                          theme_provider->GetImageSkiaNamed(pushed_image_id));
}

int FramePainter::GetTitleOffsetX() const {
  return window_icon_ ?
      window_icon_->bounds().right() + kTitleIconOffsetX :
      kTitleNoIconOffsetX;
}

int FramePainter::GetHeaderOpacity(HeaderMode header_mode,
                                   int theme_frame_id,
                                   const gfx::ImageSkia* theme_frame_overlay) {
  // User-provided themes are painted fully opaque.
  if (frame_->GetThemeProvider()->HasCustomImage(theme_frame_id))
    return kFullyOpaque;
  if (theme_frame_overlay)
    return kFullyOpaque;

  // Maximized windows with workspaces are totally transparent, except:
  // - For windows whose workspace is not tracked by the workspace code (which
  //   are used for tab dragging).
  // - When the user is cycling through workspaces.
  if (frame_->IsMaximized() &&
      GetTrackedByWorkspace(frame_->GetNativeWindow()) &&
      !IsCyclingThroughWorkspaces()) {
    return 0;
  }

  // Single browser window is very transparent.
  if (UseSoloWindowHeader())
    return kSoloWindowOpacity;

  // Otherwise, change transparency based on window activation status.
  if (header_mode == ACTIVE)
    return kActiveWindowOpacity;
  return kInactiveWindowOpacity;
}

int FramePainter::AdjustFrameHitCodeForMaximizedModes(int hit_code) {
  if (hit_code != HTNOWHERE && wm::IsWindowNormal(window_) &&
      GetRestoreBoundsInScreen(window_)) {
    // When there is a restore rectangle, a left/right maximized window might
    // be active.
    const gfx::Rect& bounds = frame_->GetWindowBoundsInScreen();
    const gfx::Rect& screen =
        Shell::GetScreen()->GetDisplayMatching(bounds).work_area();
    if (bounds.y() == screen.y() && bounds.bottom() == screen.bottom()) {
      // The window is probably either left or right maximized.
      if (bounds.x() == screen.x()) {
        // It is left maximized and we can only allow a right resize.
        return (hit_code == HTBOTTOMRIGHT ||
                hit_code == HTTOPRIGHT ||
                hit_code == HTRIGHT) ? HTRIGHT : HTNOWHERE;
      } else if (bounds.right() == screen.right()) {
        // It is right maximized and we can only allow a left resize.
        return (hit_code == HTBOTTOMLEFT ||
                hit_code == HTTOPLEFT ||
                hit_code == HTLEFT) ? HTLEFT : HTNOWHERE;
      }
    } else if (bounds.x() == screen.x() &&
               bounds.right() == screen.right()) {
      // If horizontal fill mode is activated we don't allow a left/right
      // resizing.
      if (hit_code == HTTOPRIGHT ||
          hit_code == HTTOP ||
          hit_code == HTTOPLEFT)
        return HTTOP;
      return (hit_code == HTBOTTOMRIGHT ||
              hit_code == HTBOTTOM ||
              hit_code == HTBOTTOMLEFT) ? HTBOTTOM : HTNOWHERE;
    }
  }
  return hit_code;
}

bool FramePainter::IsCyclingThroughWorkspaces() const {
  aura::RootWindow* root = window_->GetRootWindow();
  return root && root->GetProperty(internal::kCyclingThroughWorkspacesKey);
}

bool FramePainter::UseSoloWindowHeader() {
  aura::RootWindow* root = window_->GetRootWindow();
  if (!root || root->GetProperty(internal::kIgnoreSoloWindowFramePainterPolicy))
    return false;
  // Don't recompute every time, as it would require many window property
  // lookups.
  return root->GetProperty(internal::kSoloWindowHeaderKey);
}

// static
bool FramePainter::UseSoloWindowHeaderInRoot(RootWindow* root_window,
                                             Window* ignore_window) {
  int visible_window_count = 0;
  std::vector<Window*> windows = GetWindowsForSoloHeaderUpdate(root_window);
  for (std::vector<Window*>::const_iterator it = windows.begin();
       it != windows.end();
       ++it) {
    Window* window = *it;
    // Various sorts of windows "don't count" for this computation.
    if (ignore_window == window ||
        !IsVisibleNormalWindow(window) ||
        window->GetProperty(kConstrainedWindowKey))
      continue;
    if (wm::IsWindowMaximized(window))
      return false;
    ++visible_window_count;
    if (visible_window_count > 1)
      return false;
  }
  // Count must be tested because all windows might be "don't count" windows
  // in the loop above.
  return visible_window_count == 1;
}

// static
void FramePainter::UpdateSoloWindowInRoot(RootWindow* root,
                                          Window* ignore_window) {
#if defined(OS_WIN)
  // Non-Ash Windows doesn't do solo-window counting for transparency effects,
  // as the desktop background and window frames are managed by the OS.
  if (!ash::Shell::HasInstance())
    return;
#endif
  if (!root)
    return;
  bool old_solo_header = root->GetProperty(internal::kSoloWindowHeaderKey);
  bool new_solo_header = UseSoloWindowHeaderInRoot(root, ignore_window);
  if (old_solo_header == new_solo_header)
    return;
  root->SetProperty(internal::kSoloWindowHeaderKey, new_solo_header);
  // Invalidate all the window frames in the active workspace. There should
  // only be a few.
  std::vector<Window*> windows = GetWindowsForSoloHeaderUpdate(root);
  for (std::vector<Window*>::const_iterator it = windows.begin();
       it != windows.end();
       ++it) {
    Widget* widget = Widget::GetWidgetForNativeWindow(*it);
    if (widget && widget->non_client_view())
      widget->non_client_view()->SchedulePaint();
  }
}

void FramePainter::SchedulePaintForHeader() {
  int top_left_height = top_left_corner_->height();
  int top_right_height = top_right_corner_->height();
  frame_->non_client_view()->SchedulePaintInRect(
      gfx::Rect(0, 0, frame_->non_client_view()->width(),
                std::max(top_left_height, top_right_height)));
}

gfx::Rect FramePainter::GetTitleBounds(views::NonClientFrameView* view,
                                       const gfx::Font& title_font) {
  int title_x = GetTitleOffsetX();
  // Center the text in the middle of the caption - this way it adapts
  // automatically to the caption height (which is given by the owner).
  int title_y =
      (view->GetBoundsForClientView().y() - title_font.GetHeight()) / 2;
  return gfx::Rect(
      title_x,
      std::max(0, title_y),
      std::max(0, size_button_->x() - kTitleLogoSpacing - title_x),
      title_font.GetHeight());
}

}  // namespace ash
