// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vsstyle.h>

#include "chrome/renderer/pepper_scrollbar_widget.h"

#include "base/logging.h"
#include "base/win_util.h"
#include "gfx/native_theme_win.h"
#include "chrome/renderer/pepper_devices.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/platform_device.h"


void PepperScrollbarWidget::Paint(Graphics2DDeviceContext* context,
                                  const NPRect& dirty) {
  skia::PlatformCanvas* canvas = context->canvas();
  HDC dc = canvas->beginPlatformPaint();

  Part part;
  int part_id;

  // Up/Left arrow.
  part = vertical_ ? UP_ARROW : LEFT_ARROW;
  part_id = vertical_ ? DFCS_SCROLLUP : DFCS_SCROLLLEFT;
  gfx::NativeTheme::instance()->PaintScrollbarArrow(
      dc, GetThemeArrowState(part), part_id | GetClassicThemeState(part),
      &BackArrowRect().ToRECT());

  // Down/Right arrow.
  part = vertical_ ? DOWN_ARROW : RIGHT_ARROW;
  part_id = vertical_ ? DFCS_SCROLLDOWN : DFCS_SCROLLRIGHT;
  gfx::NativeTheme::instance()->PaintScrollbarArrow(
      dc, GetThemeArrowState(part), part_id | GetClassicThemeState(part),
      &ForwardArrowRect().ToRECT());

  // Beginning track.
  part = vertical_ ? VERTICAL_TRACK : HORIZONTAL_TRACK;
  part_id = vertical_ ? SBP_UPPERTRACKVERT : SBP_UPPERTRACKHORZ;
  gfx::Rect align_rect(location_, gfx::Size());
  gfx::NativeTheme::instance()->PaintScrollbarTrack(
      dc, part_id, GetThemeState(part), GetClassicThemeState(part),
      &BackTrackRect().ToRECT(), &align_rect.ToRECT(), canvas);

  // End track.
  part_id = vertical_ ? SBP_LOWERTRACKVERT : SBP_LOWERTRACKHORZ;
  gfx::NativeTheme::instance()->PaintScrollbarTrack(
      dc, part_id, GetThemeState(part), GetClassicThemeState(part),
      &ForwardTrackRect().ToRECT(), &align_rect.ToRECT(), canvas);

  // Thumb.
  part = vertical_ ? VERTICAL_THUMB : HORIZONTAL_THUMB;
  part_id = vertical_ ? SBP_THUMBBTNVERT : SBP_THUMBBTNHORZ;
  gfx::NativeTheme::instance()->PaintScrollbarThumb(
      dc, part_id, GetThemeState(part), GetClassicThemeState(part),
      &ThumbRect().ToRECT());

  // Gripper.
  part_id = vertical_ ? SBP_GRIPPERVERT : SBP_GRIPPERHORZ;
  gfx::NativeTheme::instance()->PaintScrollbarThumb(
      dc, part_id, GetThemeState(part), GetClassicThemeState(part),
      &ThumbRect().ToRECT());

  if (gfx::NativeTheme::instance()->IsClassicTheme(
          gfx::NativeTheme::SCROLLBAR)) {
    // Make the pixels opaque for the themed controls to appear correctly.
    canvas->getTopPlatformDevice().makeOpaque(
      location_.x(), location_.y(), vertical_ ? thickness_ : length_,
        vertical_ ? length_ : thickness_);
  }

  canvas->endPlatformPaint();
}

int PepperScrollbarWidget::GetThemeState(Part part) const {
  // When dragging the thumb, draw thumb pressed and other segments normal
  // regardless of where the cursor actually is.  See also four places in
  // getThemeArrowState().
  if (IsThumb(pressed_part_)) {
    if (IsThumb(part))
      return SCRBS_PRESSED;
    return IsVistaOrNewer() ? SCRBS_HOVER : SCRBS_NORMAL;
  }
  if (!enabled_)
    return SCRBS_DISABLED;
  if (hovered_part_ != part || IsTrackbar(part)) {
    return (hovered_part_ == -1 || !IsVistaOrNewer()) ?
        SCRBS_NORMAL : SCRBS_HOVER;
  }
  if (pressed_part_ == -1)
    return SCRBS_HOT;
  return (pressed_part_ == part) ? SCRBS_PRESSED : SCRBS_NORMAL;
}

int PepperScrollbarWidget::GetThemeArrowState(Part part) const {
  if (part == LEFT_ARROW || part == UP_ARROW) {
    if (!vertical_) {
      if (IsThumb(pressed_part_))
          return !IsVistaOrNewer() ? ABS_LEFTNORMAL : ABS_LEFTHOVER;
      if (!enabled_)
          return ABS_LEFTDISABLED;
      if (hovered_part_ != part) {
          return ((hovered_part_ == -1) || !IsVistaOrNewer()) ?
              ABS_LEFTNORMAL : ABS_LEFTHOVER;
      }
      if (pressed_part_ == -1)
          return ABS_LEFTHOT;
      return (pressed_part_ == part) ? ABS_LEFTPRESSED : ABS_LEFTNORMAL;
    }
    if (IsThumb(pressed_part_))
        return !IsVistaOrNewer() ? ABS_UPNORMAL : ABS_UPHOVER;
    if (!enabled_)
        return ABS_UPDISABLED;
    if (hovered_part_ != part) {
        return ((hovered_part_ == -1) || !IsVistaOrNewer()) ?
            ABS_UPNORMAL : ABS_UPHOVER;
    }
    if (pressed_part_ == -1)
        return ABS_UPHOT;
    return (pressed_part_ == part) ? ABS_UPPRESSED : ABS_UPNORMAL;
  }
  if (!vertical_) {
    if (IsThumb(pressed_part_))
        return !IsVistaOrNewer() ? ABS_RIGHTNORMAL : ABS_RIGHTHOVER;
    if (!enabled_)
        return ABS_RIGHTDISABLED;
    if (hovered_part_ != part) {
        return ((hovered_part_ == -1) || !IsVistaOrNewer()) ?
            ABS_RIGHTNORMAL : ABS_RIGHTHOVER;
    }
    if (pressed_part_ == -1)
        return ABS_RIGHTHOT;
    return (pressed_part_ == part) ? ABS_RIGHTPRESSED : ABS_RIGHTNORMAL;
  }
  if (IsThumb(pressed_part_))
      return !IsVistaOrNewer() ? ABS_DOWNNORMAL : ABS_DOWNHOVER;
  if (!enabled_)
      return ABS_DOWNDISABLED;
  if (hovered_part_ != part) {
      return ((hovered_part_ == -1) || !IsVistaOrNewer()) ?
          ABS_DOWNNORMAL : ABS_DOWNHOVER;
  }
  if (pressed_part_ == -1)
      return ABS_DOWNHOT;
  return (pressed_part_ == part) ? ABS_DOWNPRESSED : ABS_DOWNNORMAL;
}

int PepperScrollbarWidget::GetClassicThemeState(Part part) const {
  // When dragging the thumb, draw the buttons normal even when hovered.
  if (IsThumb(pressed_part_))
    return 0;
  if (!enabled_)
    return DFCS_INACTIVE;
  if (hovered_part_ != part || IsTrackbar(part))
    return 0;
  if (pressed_part_ == -1)
    return DFCS_HOT;
  return (pressed_part_ == part) ? (DFCS_PUSHED | DFCS_FLAT) : 0;
}

bool PepperScrollbarWidget::IsVistaOrNewer() const {
  return win_util::GetWinVersion() >= win_util::WINVERSION_VISTA;
}

void PepperScrollbarWidget::GenerateMeasurements() {
  thickness_ = GetSystemMetrics(SM_CXVSCROLL);
  arrow_length_ = GetSystemMetrics(SM_CYVSCROLL);
}

bool PepperScrollbarWidget::ShouldSnapBack(const gfx::Point& location) const {
  static const int kOffEndMultiplier = 3;
  static const int kOffSideMultiplier = 8;

  // Find the rect within which we shouldn't snap, by expanding the track rect
  // in both dimensions.
  gfx::Rect rect = TrackRect();
  int x_diff =
      (vertical_ ? kOffSideMultiplier : kOffEndMultiplier) * thickness_;
  int y_diff =
      (vertical_ ? kOffEndMultiplier : kOffSideMultiplier) * thickness_;
  rect.set_x(rect.x() - x_diff);
  rect.set_width(rect.width() + 2 * x_diff);
  rect.set_y(rect.y() - y_diff);
  rect.set_height(rect.height() + 2 * y_diff);

  return !rect.Contains(location);
}

bool PepperScrollbarWidget::ShouldCenterOnThumb(
    const NPPepperEvent& event) const {
  return event.u.mouse.button == NPMouseButton_Left &&
         event.u.mouse.modifier & NPEventModifier_ShiftKey;
}

int PepperScrollbarWidget::MinimumThumbLength() const {
  return thickness_;
}
