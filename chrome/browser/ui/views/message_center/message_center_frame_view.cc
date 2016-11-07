// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/message_center/message_center_frame_view.h"

#include "build/build_config.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/shadow_value.h"
#include "ui/message_center/message_center_style.h"
#include "ui/views/shadow_border.h"
#include "ui/views/widget/widget.h"

namespace message_center {

MessageCenterFrameView::MessageCenterFrameView() {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  const int kBorderWidth = 1;
  SetBorder(views::CreateSolidBorder(
      kBorderWidth, message_center::kMessageCenterBorderColor));
#else
  const int kShadowBlur = 8;
  SetBorder(std::unique_ptr<views::Border>(new views::ShadowBorder(
      gfx::ShadowValue(gfx::Vector2d(0, 0), kShadowBlur,
                       message_center::kMessageCenterShadowColor))));
#endif
}

MessageCenterFrameView::~MessageCenterFrameView() {}

gfx::Rect MessageCenterFrameView::GetBoundsForClientView() const {
  gfx::Rect client_bounds = GetLocalBounds();
  client_bounds.Inset(GetInsets());
  return client_bounds;
}

gfx::Rect MessageCenterFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect window_bounds = client_bounds;
  window_bounds.Inset(GetInsets());
  return window_bounds;
}

int MessageCenterFrameView::NonClientHitTest(const gfx::Point& point) {
  gfx::Rect frame_bounds = bounds();
  frame_bounds.Inset(GetInsets());
  if (!frame_bounds.Contains(point))
    return HTNOWHERE;

  return GetWidget()->client_view()->NonClientHitTest(point);
}

void MessageCenterFrameView::GetWindowMask(const gfx::Size& size,
                                           gfx::Path* window_mask) {
}

void MessageCenterFrameView::ResetWindowControls() {
}

void MessageCenterFrameView::UpdateWindowIcon() {
}

void MessageCenterFrameView::UpdateWindowTitle() {
}

void MessageCenterFrameView::SizeConstraintsChanged() {
}

gfx::Insets MessageCenterFrameView::GetInsets() const {
  return border()->GetInsets();
}

const char* MessageCenterFrameView::GetClassName() const {
  return "MessageCenterFrameView";
}

}  // namespace message_center
