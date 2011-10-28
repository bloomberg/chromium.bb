// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/frame/touch_browser_frame_view.h"

#include "chrome/browser/ui/touch/animation/screen_rotation_setter.h"
#include "views/controls/button/image_button.h"
#include "views/desktop/desktop_window_view.h"
#include "ui/gfx/transform.h"

namespace {

ui::Transform SideToTransform(sensors::ScreenOrientation::Side side,
                              const ui::Transform& old_transform,
                              const gfx::Size& size) {
  gfx::Point origin;
  gfx::Point bottom_right(size.width(), size.height());
  old_transform.TransformPoint(origin);
  old_transform.TransformPoint(bottom_right);
  int original_width = abs(origin.x() - bottom_right.x());
  int original_height = abs(origin.y() - bottom_right.y());
  ui::Transform to_return;
  switch (side) {
    case sensors::ScreenOrientation::TOP: break;
    case sensors::ScreenOrientation::RIGHT:
      to_return.ConcatRotate(90);
      to_return.ConcatTranslate(original_width, 0);
      break;
    case sensors::ScreenOrientation::LEFT:
      to_return.ConcatRotate(-90);
      to_return.ConcatTranslate(0, original_height);
      break;
    case sensors::ScreenOrientation::BOTTOM:
      to_return.ConcatRotate(180);
      to_return.ConcatTranslate(original_width, original_height);
      break;
    default:
      to_return = old_transform;
      break;
  }
  return to_return;
}

}  // namespace

// static
const char TouchBrowserFrameView::kViewClassName[] =
    "browser/ui/touch/frame/TouchBrowserFrameView";

///////////////////////////////////////////////////////////////////////////////
// TouchBrowserFrameView, public:

TouchBrowserFrameView::TouchBrowserFrameView(BrowserFrame* frame,
                                             BrowserView* browser_view)
    : OpaqueBrowserFrameView(frame, browser_view),
      initialized_screen_rotation_(false) {
  sensors::Provider::GetInstance()->AddListener(this);
}

TouchBrowserFrameView::~TouchBrowserFrameView() {
 sensors::Provider::GetInstance()->RemoveListener(this);
}

std::string TouchBrowserFrameView::GetClassName() const {
  return kViewClassName;
}

bool TouchBrowserFrameView::HitTest(const gfx::Point& point) const {
  if (OpaqueBrowserFrameView::HitTest(point))
    return true;

  if (close_button()->IsVisible() &&
      close_button()->GetMirroredBounds().Contains(point))
    return true;
  if (restore_button()->IsVisible() &&
      restore_button()->GetMirroredBounds().Contains(point))
    return true;
  if (maximize_button()->IsVisible() &&
      maximize_button()->GetMirroredBounds().Contains(point))
    return true;
  if (minimize_button()->IsVisible() &&
      minimize_button()->GetMirroredBounds().Contains(point))
    return true;

  return false;
}

void TouchBrowserFrameView::OnScreenOrientationChanged(
    const sensors::ScreenOrientation& change) {
  // In views desktop mode, then the desktop_window_view will not be NULL and
  // is the view to be rotated.
  views::View* to_rotate =
      views::desktop::DesktopWindowView::desktop_window_view;

  if (!to_rotate) {
    // Otherwise, rotate the widget's view.
    views::Widget* widget = GetWidget();
    to_rotate = widget->GetRootView();
  }

  if (!initialized_screen_rotation_) {
    to_rotate->SetPaintToLayer(true);
    to_rotate->SetLayerPropertySetter(
        ScreenRotationSetterFactory::Create(to_rotate));
    initialized_screen_rotation_ = true;
  }

  const ui::Transform& old_xform = to_rotate->GetTransform();
  const ui::Transform& new_xform = SideToTransform(change.upward,
                                                   old_xform,
                                                   to_rotate->size());
  if (old_xform != new_xform)
    to_rotate->SetTransform(new_xform);
}
