// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/shell_window_frame_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/views/extensions/native_app_window_views.h"
#include "extensions/common/draggable_region.h"
#include "grit/theme_resources.h"
#include "grit/ui_strings.h"  // Accessibility names
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/path.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN) && !defined(USE_AURA)
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "ui/base/win/shell.h"
#endif

#if defined(USE_ASH)
#include "ash/ash_constants.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "ui/aura/env.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace {
const int kResizeInsideBoundsSize = 5;
const int kResizeAreaCornerSize = 16;

// Height of the chrome-style caption, in pixels.
const int kCaptionHeight = 25;
}  // namespace


const char ShellWindowFrameView::kViewClassName[] =
    "browser/ui/views/extensions/ShellWindowFrameView";

ShellWindowFrameView::ShellWindowFrameView(NativeAppWindowViews* window)
    : window_(window),
      frame_(NULL),
      close_button_(NULL) {
}

ShellWindowFrameView::~ShellWindowFrameView() {
}

void ShellWindowFrameView::Init(views::Widget* frame) {
  frame_ = frame;

  if (!window_->frameless()) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    close_button_ = new views::ImageButton(this);
    close_button_->SetImage(views::CustomButton::STATE_NORMAL,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_CLOSE).ToImageSkia());
    close_button_->SetImage(views::CustomButton::STATE_HOVERED,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_CLOSE_H).ToImageSkia());
    close_button_->SetImage(views::CustomButton::STATE_PRESSED,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_CLOSE_P).ToImageSkia());
    close_button_->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
    AddChildView(close_button_);
    maximize_button_ = new views::ImageButton(this);
    maximize_button_->SetImage(views::CustomButton::STATE_NORMAL,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_MAXIMIZE).ToImageSkia());
    maximize_button_->SetImage(views::CustomButton::STATE_HOVERED,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_MAXIMIZE_H).ToImageSkia());
    maximize_button_->SetImage(views::CustomButton::STATE_PRESSED,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_MAXIMIZE_P).ToImageSkia());
    maximize_button_->SetImage(views::CustomButton::STATE_DISABLED,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_MAXIMIZE_D).ToImageSkia());
    maximize_button_->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MAXIMIZE));
    AddChildView(maximize_button_);
    restore_button_ = new views::ImageButton(this);
    restore_button_->SetImage(views::CustomButton::STATE_NORMAL,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_RESTORE).ToImageSkia());
    restore_button_->SetImage(views::CustomButton::STATE_HOVERED,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_RESTORE_H).ToImageSkia());
    restore_button_->SetImage(views::CustomButton::STATE_PRESSED,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_RESTORE_P).ToImageSkia());
    restore_button_->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_APP_ACCNAME_RESTORE));
    AddChildView(restore_button_);
    minimize_button_ = new views::ImageButton(this);
    minimize_button_->SetImage(views::CustomButton::STATE_NORMAL,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_MINIMIZE).ToImageSkia());
    minimize_button_->SetImage(views::CustomButton::STATE_HOVERED,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_MINIMIZE_H).ToImageSkia());
    minimize_button_->SetImage(views::CustomButton::STATE_PRESSED,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_MINIMIZE_P).ToImageSkia());
    minimize_button_->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MINIMIZE));
    AddChildView(minimize_button_);
  }

#if defined(USE_AURA)
  int resize_inside_bounds_size = kResizeInsideBoundsSize;
  aura::Window* window = frame->GetNativeWindow();
#if defined(USE_ASH)
  if (chrome::IsNativeWindowInAsh(window)) {
    // Ensure we get resize cursors for a few pixels outside our bounds.
    window->SetHitTestBoundsOverrideOuter(
        gfx::Insets(-ash::kResizeOutsideBoundsSize,
                    -ash::kResizeOutsideBoundsSize,
                    -ash::kResizeOutsideBoundsSize,
                    -ash::kResizeOutsideBoundsSize),
        ash::kResizeOutsideBoundsScaleForTouch);

    // If the window is in ash, the inside area used for resizing will be
    // smaller due to the fact that outside area is also used for resizing.
    resize_inside_bounds_size = ash::kResizeInsideBoundsSize;
  }
#endif
  // Ensure we get resize cursors just inside our bounds as well.
  // TODO(jeremya): do we need to update these when in fullscreen/maximized?
  window->set_hit_test_bounds_override_inner(
      gfx::Insets(resize_inside_bounds_size, resize_inside_bounds_size,
                  resize_inside_bounds_size, resize_inside_bounds_size));
#endif
}

// views::NonClientFrameView implementation.

gfx::Rect ShellWindowFrameView::GetBoundsForClientView() const {
  if (window_->frameless() || frame_->IsFullscreen())
    return bounds();
  return gfx::Rect(0, kCaptionHeight, width(),
      std::max(0, height() - kCaptionHeight));
}

gfx::Rect ShellWindowFrameView::GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const {
  if (window_->frameless()) {
    gfx::Rect window_bounds = client_bounds;
    // Enforce minimum size (1, 1) in case that client_bounds is passed with
    // empty size. This could occur when the frameless window is being
    // initialized.
    if (window_bounds.IsEmpty()) {
      window_bounds.set_width(1);
      window_bounds.set_height(1);
    }
    return window_bounds;
  }

  int closeButtonOffsetX =
      (kCaptionHeight - close_button_->height()) / 2;
  int header_width = close_button_->width() + closeButtonOffsetX * 2;
  return gfx::Rect(client_bounds.x(),
                   std::max(0, client_bounds.y() - kCaptionHeight),
                   std::max(header_width, client_bounds.width()),
                   client_bounds.height() + kCaptionHeight);
}

int ShellWindowFrameView::NonClientHitTest(const gfx::Point& point) {
  if (frame_->IsFullscreen())
    return HTCLIENT;

  int resize_inside_bounds_size = kResizeInsideBoundsSize;
  int resize_area_corner_size = kResizeAreaCornerSize;

#if defined(USE_ASH)
  aura::Window* window = frame_->GetNativeWindow();
  if (chrome::IsNativeWindowInAsh(window)) {
    gfx::Rect expanded_bounds = bounds();
    int outside_bounds = ash::kResizeOutsideBoundsSize;
    if (aura::Env::GetInstance()->is_touch_down())
      outside_bounds *= ash::kResizeOutsideBoundsScaleForTouch;
    expanded_bounds.Inset(-outside_bounds, -outside_bounds);
    if (!expanded_bounds.Contains(point))
      return HTNOWHERE;

    resize_inside_bounds_size = ash::kResizeInsideBoundsSize;
    resize_area_corner_size = ash::kResizeAreaCornerSize;
  }
#endif

  // Check the frame first, as we allow a small area overlapping the contents
  // to be used for resize handles.
  bool can_ever_resize = frame_->widget_delegate() ?
      frame_->widget_delegate()->CanResize() :
      false;
  if (can_ever_resize) {
    // Don't allow overlapping resize handles when the window is maximized or
    // fullscreen, as it can't be resized in those states.
    int resize_border =
        frame_->IsMaximized() || frame_->IsFullscreen() ? 0 :
        resize_inside_bounds_size;
    int frame_component = GetHTComponentForFrame(point,
                                                 resize_border,
                                                 resize_border,
                                                 resize_area_corner_size,
                                                 resize_area_corner_size,
                                                 can_ever_resize);
    if (frame_component != HTNOWHERE)
      return frame_component;
  }

  // Check for possible draggable region in the client area for the frameless
  // window.
  if (window_->frameless() &&
      window_->draggable_region() &&
      window_->draggable_region()->contains(point.x(), point.y()))
    return HTCAPTION;

  int client_component = frame_->client_view()->NonClientHitTest(point);
  if (client_component != HTNOWHERE)
    return client_component;

  // Then see if the point is within any of the window controls.
  if (close_button_ && close_button_->visible() &&
      close_button_->GetMirroredBounds().Contains(point))
    return HTCLOSE;

  // Caption is a safe default.
  return HTCAPTION;
}

void ShellWindowFrameView::GetWindowMask(const gfx::Size& size,
                                         gfx::Path* window_mask) {
  // We got nothing to say about no window mask.
}

// views::View implementation.

gfx::Size ShellWindowFrameView::GetPreferredSize() {
  gfx::Size pref = frame_->client_view()->GetPreferredSize();
  gfx::Rect bounds(0, 0, pref.width(), pref.height());
  return frame_->non_client_view()->GetWindowBoundsForClientBounds(
      bounds).size();
}

void ShellWindowFrameView::Layout() {
  if (window_->frameless())
    return;
  gfx::Size close_size = close_button_->GetPreferredSize();
  const int kButtonOffsetY = 0;
  const int kButtonSpacing = 1;
  const int kRightMargin = 3;

  close_button_->SetBounds(
      width() - kRightMargin - close_size.width(),
      kButtonOffsetY,
      close_size.width(),
      close_size.height());

  bool can_ever_resize = frame_->widget_delegate() ?
      frame_->widget_delegate()->CanResize() :
      false;
  maximize_button_->SetEnabled(can_ever_resize);
  gfx::Size maximize_size = maximize_button_->GetPreferredSize();
  maximize_button_->SetBounds(
      close_button_->x() - kButtonSpacing - maximize_size.width(),
      kButtonOffsetY,
      maximize_size.width(),
      maximize_size.height());
  gfx::Size restore_size = restore_button_->GetPreferredSize();
  restore_button_->SetBounds(
      close_button_->x() - kButtonSpacing - restore_size.width(),
      kButtonOffsetY,
      restore_size.width(),
      restore_size.height());

  bool maximized = frame_->IsMaximized();
  maximize_button_->SetVisible(!maximized);
  restore_button_->SetVisible(maximized);
  if (maximized)
    maximize_button_->SetState(views::CustomButton::STATE_NORMAL);
  else
    restore_button_->SetState(views::CustomButton::STATE_NORMAL);

  gfx::Size minimize_size = minimize_button_->GetPreferredSize();
  minimize_button_->SetBounds(
      maximize_button_->x() - kButtonSpacing - minimize_size.width(),
      kButtonOffsetY,
      minimize_size.width(),
      minimize_size.height());
}

void ShellWindowFrameView::OnPaint(gfx::Canvas* canvas) {
  if (window_->frameless())
    return;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (ShouldPaintAsActive()) {
    close_button_->SetImage(views::CustomButton::STATE_NORMAL,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_CLOSE).ToImageSkia());
  } else {
    close_button_->SetImage(views::CustomButton::STATE_NORMAL,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_CLOSE_U).ToImageSkia());
  }

  // TODO(jeremya): different look for inactive?
  SkPaint paint;
  paint.setAntiAlias(false);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SK_ColorWHITE);
  gfx::Path path;
  const int radius = frame_->IsMaximized() ? 0 : 1;
  path.moveTo(0, radius);
  path.lineTo(radius, 0);
  path.lineTo(width() - radius - 1, 0);
  path.lineTo(width(), radius + 1);
  path.lineTo(width(), kCaptionHeight);
  path.lineTo(0, kCaptionHeight);
  path.close();
  canvas->DrawPath(path, paint);
}

std::string ShellWindowFrameView::GetClassName() const {
  return kViewClassName;
}

gfx::Size ShellWindowFrameView::GetMinimumSize() {
  gfx::Size min_size = frame_->client_view()->GetMinimumSize();
  if (window_->frameless())
    return min_size;

  // Ensure we can display the top of the caption area.
  gfx::Rect client_bounds = GetBoundsForClientView();
  min_size.Enlarge(0, client_bounds.y());
  // Ensure we have enough space for the window icon and buttons.  We allow
  // the title string to collapse to zero width.
  int closeButtonOffsetX =
      (kCaptionHeight - close_button_->height()) / 2;
  int header_width = close_button_->width() + closeButtonOffsetX * 2;
  if (header_width > min_size.width())
    min_size.set_width(header_width);
  return min_size;
}

gfx::Size ShellWindowFrameView::GetMaximumSize() {
  gfx::Size max_size = frame_->client_view()->GetMaximumSize();
  if (window_->frameless())
    return max_size;

  if (!max_size.IsEmpty()) {
    gfx::Rect client_bounds = GetBoundsForClientView();
    max_size.Enlarge(0, client_bounds.y());
  }
  return max_size;
}

// views::ButtonListener implementation.

void ShellWindowFrameView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  DCHECK(!window_->frameless());
  if (sender == close_button_)
    frame_->Close();
  else if (sender == maximize_button_)
    frame_->Maximize();
  else if (sender == restore_button_)
    frame_->Restore();
  else if (sender == minimize_button_)
    frame_->Minimize();
}
