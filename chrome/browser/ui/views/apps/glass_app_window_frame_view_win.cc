// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/glass_app_window_frame_view_win.h"

#include "apps/ui/native_app_window.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/win/dpi.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

const int kResizeAreaCornerSize = 16;

}  // namespace

const char GlassAppWindowFrameViewWin::kViewClassName[] =
    "ui/views/apps/GlassAppWindowFrameViewWin";

GlassAppWindowFrameViewWin::GlassAppWindowFrameViewWin(
    apps::NativeAppWindow* window,
    views::Widget* widget)
    : window_(window), widget_(widget) {
}

GlassAppWindowFrameViewWin::~GlassAppWindowFrameViewWin() {
}

gfx::Insets GlassAppWindowFrameViewWin::GetGlassInsets() const {
  // If 1 is not subtracted, they are too big. There is possibly some reason
  // for that.
  // Also make sure the insets don't go below 1, as bad things happen when they
  // do.
  int caption_height = std::max(0,
      gfx::win::GetSystemMetricsInDIP(SM_CYSMICON) +
          gfx::win::GetSystemMetricsInDIP(SM_CYSIZEFRAME) - 1);
  int frame_size =
      std::max(1, gfx::win::GetSystemMetricsInDIP(SM_CXSIZEFRAME) - 1);
  return gfx::Insets(
      frame_size + caption_height, frame_size, frame_size, frame_size);
}

gfx::Rect GlassAppWindowFrameViewWin::GetBoundsForClientView() const {
  if (widget_->IsFullscreen())
    return bounds();

  gfx::Insets insets = GetGlassInsets();
  return gfx::Rect(insets.left(),
                   insets.top(),
                   std::max(0, width() - insets.left() - insets.right()),
                   std::max(0, height() - insets.top() - insets.bottom()));
}

gfx::Rect GlassAppWindowFrameViewWin::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Insets insets = GetGlassInsets();
  return gfx::Rect(client_bounds.x() - insets.left(),
                   client_bounds.y() - insets.top(),
                   client_bounds.width() + insets.left() + insets.right(),
                   client_bounds.height() + insets.top() + insets.bottom());
}

int GlassAppWindowFrameViewWin::NonClientHitTest(const gfx::Point& point) {
  if (widget_->IsFullscreen())
    return HTCLIENT;

  if (!bounds().Contains(point))
    return HTNOWHERE;

  // Check the frame first, as we allow a small area overlapping the contents
  // to be used for resize handles.
  bool can_ever_resize = widget_->widget_delegate()
                             ? widget_->widget_delegate()->CanResize()
                             : false;
  // Don't allow overlapping resize handles when the window is maximized or
  // fullscreen, as it can't be resized in those states.
  int resize_border = gfx::win::GetSystemMetricsInDIP(SM_CXSIZEFRAME);
  int frame_component =
      GetHTComponentForFrame(point,
                             resize_border,
                             resize_border,
                             kResizeAreaCornerSize - resize_border,
                             kResizeAreaCornerSize - resize_border,
                             can_ever_resize);
  if (frame_component != HTNOWHERE)
    return frame_component;

  int client_component = widget_->client_view()->NonClientHitTest(point);
  if (client_component != HTNOWHERE)
    return client_component;

  // Caption is a safe default.
  return HTCAPTION;
}

void GlassAppWindowFrameViewWin::GetWindowMask(const gfx::Size& size,
                                               gfx::Path* window_mask) {
  // We got nothing to say about no window mask.
}

gfx::Size GlassAppWindowFrameViewWin::GetPreferredSize() const {
  gfx::Size pref = widget_->client_view()->GetPreferredSize();
  gfx::Rect bounds(0, 0, pref.width(), pref.height());
  return widget_->non_client_view()
      ->GetWindowBoundsForClientBounds(bounds)
      .size();
}

const char* GlassAppWindowFrameViewWin::GetClassName() const {
  return kViewClassName;
}

gfx::Size GlassAppWindowFrameViewWin::GetMinimumSize() const {
  gfx::Size min_size = widget_->client_view()->GetMinimumSize();
  gfx::Rect client_bounds = GetBoundsForClientView();
  min_size.Enlarge(0, client_bounds.y());
  return min_size;
}

gfx::Size GlassAppWindowFrameViewWin::GetMaximumSize() const {
  gfx::Size max_size = widget_->client_view()->GetMaximumSize();

  // Add to the client maximum size the height of any title bar and borders.
  gfx::Size client_size = GetBoundsForClientView().size();
  if (max_size.width())
    max_size.Enlarge(width() - client_size.width(), 0);
  if (max_size.height())
    max_size.Enlarge(0, height() - client_size.height());

  return max_size;
}
