// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#import "chrome/browser/ui/cocoa/location_bar/button_decoration.h"

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#import "ui/base/cocoa/appkit_utils.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

NSImage* GetImageFromId(int image_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(image_id)
      .ToNSImage();
}

bool IsValidNinePartImageIds(const ui::NinePartImageIds& object) {
  return (object.top_left >= 0) &&
         (object.top >= 0) &&
         (object.top_right >= 0) &&
         (object.left >= 0) &&
         (object.center >= 0) &&
         (object.right >= 0) &&
         (object.bottom_left >= 0) &&
         (object.bottom >= 0) &&
         (object.bottom_right >= 0);
}

}  // namespace

ButtonDecoration::ButtonDecoration(ui::NinePartImageIds normal_image_ids,
                                   int normal_icon_id,
                                   ui::NinePartImageIds hover_image_ids,
                                   int hover_icon_id,
                                   ui::NinePartImageIds pressed_image_ids,
                                   int pressed_icon_id,
                                   CGFloat max_inner_padding)
    : normal_image_ids_(normal_image_ids),
      hover_image_ids_(hover_image_ids),
      pressed_image_ids_(pressed_image_ids),
      normal_icon_id_(normal_icon_id),
      hover_icon_id_(hover_icon_id),
      pressed_icon_id_(pressed_icon_id),
      state_(kButtonStateNormal),
      max_inner_padding_(max_inner_padding) {
  DCHECK(IsValidNinePartImageIds(normal_image_ids_) && normal_icon_id_ >= 0);
  DCHECK(IsValidNinePartImageIds(hover_image_ids_) && hover_icon_id_ >= 0);
  DCHECK(IsValidNinePartImageIds(pressed_image_ids_) && pressed_icon_id_ >= 0);
  DCHECK_GE(max_inner_padding_, 0);
}

ButtonDecoration::~ButtonDecoration() {
}

void ButtonDecoration::SetButtonState(ButtonDecoration::ButtonState state) {
  state_ = state;
}

ButtonDecoration::ButtonState ButtonDecoration::GetButtonState() const {
  return state_;
}

bool ButtonDecoration::PreventFocus(NSPoint location) const {
  return false;
}

void ButtonDecoration::SetIcon(ButtonState state, int icon_id) {
  switch (state) {
    case kButtonStateNormal:
      normal_icon_id_ = icon_id;
      break;
    case kButtonStateHover:
      hover_icon_id_ = icon_id;
      break;
    case kButtonStatePressed:
      pressed_icon_id_ = icon_id;
      break;
    default:
      NOTREACHED();
  }
}

void ButtonDecoration::SetIcon(int icon_id) {
  normal_icon_id_ = icon_id;
  hover_icon_id_ = icon_id;
  pressed_icon_id_ = icon_id;
}

void ButtonDecoration::SetBackgroundImageIds(
    ui::NinePartImageIds normal_image_ids,
    ui::NinePartImageIds hover_image_ids,
    ui::NinePartImageIds pressed_image_ids) {
  DCHECK(IsValidNinePartImageIds(normal_image_ids));
  DCHECK(IsValidNinePartImageIds(hover_image_ids));
  DCHECK(IsValidNinePartImageIds(pressed_image_ids));
  normal_image_ids_ = normal_image_ids;
  hover_image_ids_ = hover_image_ids;
  pressed_image_ids_ = pressed_image_ids;
}

ui::NinePartImageIds ButtonDecoration::GetBackgroundImageIds() const {
  switch (state_) {
    case kButtonStateHover:
      return hover_image_ids_;
    case kButtonStatePressed:
      return pressed_image_ids_;
    case kButtonStateNormal:
    default:
      return normal_image_ids_;
  }
}

NSImage* ButtonDecoration::GetIconImage() const {
  switch (state_) {
    case kButtonStateHover:
      return GetImageFromId(hover_icon_id_);
    case kButtonStatePressed:
      return GetImageFromId(pressed_icon_id_);
    case kButtonStateNormal:
    default:
      return GetImageFromId(normal_icon_id_);
  }
}

CGFloat ButtonDecoration::GetWidthForSpace(CGFloat width) {
  const ui::NinePartImageIds image_ids = GetBackgroundImageIds();
  NSImage* icon = GetIconImage();

  if (!icon)
    return kOmittedWidth;

  const CGFloat min_width = [GetImageFromId(image_ids.left) size].width +
                            [icon size].width +
                            [GetImageFromId(image_ids.right) size].width;
  if (width < min_width)
    return kOmittedWidth;

  const CGFloat max_width = min_width + 2 * max_inner_padding_;
  return std::min(width, max_width);
}

void ButtonDecoration::DrawInFrame(NSRect frame, NSView* control_view) {
  const ui::NinePartImageIds image_ids = GetBackgroundImageIds();
  NSImage* icon = GetIconImage();

  if (!icon)
    return;

  ui::DrawNinePartImage(frame, image_ids, NSCompositeSourceOver, 1.0, YES);

  const CGFloat x_inset =
      std::floor((NSWidth(frame) - [icon size].width) / 2.0);
  const CGFloat y_inset =
      std::floor((NSHeight(frame) - [icon size].height) / 2.0);

  [icon drawInRect:NSInsetRect(frame, x_inset, y_inset)
          fromRect:NSZeroRect
         operation:NSCompositeSourceOver
          fraction:1.0
    respectFlipped:YES
             hints:nil];
}

bool ButtonDecoration::AcceptsMousePress() {
  return true;
}

bool ButtonDecoration::IsDraggable() {
  return false;
}

bool ButtonDecoration::OnMousePressed(NSRect frame, NSPoint location) {
  return false;
}

ButtonDecoration* ButtonDecoration::AsButtonDecoration() {
  return this;
}
