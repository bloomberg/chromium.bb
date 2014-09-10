// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/athena_frame_view.h"

#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/hit_test.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/client_view.h"

namespace athena {
namespace {

// The height of the top border necessary to display the title without the icon.
const int kDefaultTitleHeight = 13;

// The default background color for athena's frame. This is placeholder.
const SkColor kDefaultTitleBackground = 0xFFcccccc;

}  // namespace

// static
const char AthenaFrameView::kViewClassName[] = "AthenaFrameView";

AthenaFrameView::AthenaFrameView(views::Widget* frame) : frame_(frame) {
  set_background(
      views::Background::CreateSolidBackground(kDefaultTitleBackground));
  UpdateWindowTitle();
}

AthenaFrameView::~AthenaFrameView() {
}

gfx::Rect AthenaFrameView::GetBoundsForClientView() const {
  gfx::Rect client_bounds = bounds();
  client_bounds.Inset(NonClientBorderInsets());
  return client_bounds;
}

gfx::Rect AthenaFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect window_bounds = client_bounds;
  window_bounds.Inset(-NonClientBorderInsets());
  return window_bounds;
}

int AthenaFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;
  int client_hit_test = frame_->client_view()->NonClientHitTest(point);
  if (client_hit_test != HTNOWHERE)
    return client_hit_test;
  int window_hit_test =
      GetHTComponentForFrame(point, 0, NonClientBorderThickness(), 0, 0, false);
  return (window_hit_test == HTNOWHERE) ? HTCAPTION : client_hit_test;
}

gfx::Size AthenaFrameView::GetPreferredSize() const {
  gfx::Size pref = frame_->client_view()->GetPreferredSize();
  gfx::Rect bounds(0, 0, pref.width(), pref.height());
  return frame_->non_client_view()
      ->GetWindowBoundsForClientBounds(bounds)
      .size();
}

const char* AthenaFrameView::GetClassName() const {
  return kViewClassName;
}

gfx::Insets AthenaFrameView::NonClientBorderInsets() const {
  int border_thickness = NonClientBorderThickness();
  return gfx::Insets(NonClientTopBorderHeight(),
                     border_thickness,
                     border_thickness,
                     border_thickness);
}

int AthenaFrameView::NonClientBorderThickness() const {
  return 0;
}

int AthenaFrameView::NonClientTopBorderHeight() const {
  return kDefaultTitleHeight;
}

}  // namespace athena
