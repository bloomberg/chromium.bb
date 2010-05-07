// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper_scrollbar_widget.h"

#include "base/basictypes.h"
#include "base/keyboard_codes.h"
#include "base/logging.h"
#include "chrome/renderer/pepper_devices.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/platform_device.h"
#include "webkit/glue/plugins/plugin_instance.h"

const int PepperScrollbarWidget::kPixelsPerLine = 40;
const float PepperScrollbarWidget::kMinFractionToStepWhenPaging = 0.875f;
#if !defined(OS_MACOSX)
const int PepperScrollbarWidget::kMaxOverlapBetweenPages = kint32max;
#endif
const float PepperScrollbarWidget::kInitialAutoscrollTimerDelay = 0.25f;
const float PepperScrollbarWidget::kAutoscrollTimerDelay = 0.05f;


PepperScrollbarWidget::PepperScrollbarWidget()
    : vertical_(true),
      enabled_(true),
      length_(0),
      total_length_(0),
      pixels_per_page_(0),
      thickness_(0),
      arrow_length_(0),
      hovered_part_(-1),
      pressed_part_(-1),
      thumb_position_(0),
      have_mouse_capture_(false),
      drag_origin_(-1.0),
      pressed_position_(-1) {
  GenerateMeasurements();
}

PepperScrollbarWidget::~PepperScrollbarWidget() {
  StopTimerIfNeeded();
}

void PepperScrollbarWidget::Destroy() {
  delete this;
}

bool PepperScrollbarWidget::HandleEvent(const NPPepperEvent& event) {
  bool rv = false;
  switch (event.type) {
    case NPEventType_MouseDown:
      rv = OnMouseDown(event);
      break;
    case NPEventType_MouseUp:
      rv = OnMouseUp(event);
      break;
    case NPEventType_MouseMove:
      rv = OnMouseMove(event);
      break;
    case NPEventType_MouseEnter:
      break;
    case NPEventType_MouseLeave:
      rv = OnMouseLeave(event);
      break;
    case NPEventType_MouseWheel:
      rv = OnMouseWheel(event);
      break;
    case NPEventType_KeyDown:
      rv = OnKeyDown(event);
      break;
    case NPEventType_KeyUp:
      rv = OnKeyUp(event);
      break;
    default:
      break;
  }

  if (rv) {
    dirty_rect_ = dirty_rect_.Union(gfx::Rect(
        location_,
        gfx::Size(vertical_ ? thickness_: length_,
        vertical_ ? length_ : thickness_)));
    WidgetPropertyChanged(NPWidgetPropertyDirtyRect);
  }

  return rv;
}

void PepperScrollbarWidget::GetProperty(
    NPWidgetProperty property, void* value) {
  switch (property) {
    case NPWidgetPropertyDirtyRect: {
      NPRect* rv = reinterpret_cast<NPRect*>(value);
      rv->left = dirty_rect_.x();
      rv->top = dirty_rect_.y();
      rv->right = dirty_rect_.right();
      rv->bottom = dirty_rect_.bottom();
      dirty_rect_ = gfx::Rect();
      break;
    }
    case NPWidgetPropertyScrollbarThickness: {
      int32* rv = static_cast<int32*>(value);
      *rv = thickness_;
      break;
    }
    case NPWidgetPropertyScrollbarPosition: {
      int32* rv = static_cast<int32*>(value);
      *rv = GetPosition();
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void PepperScrollbarWidget::SetProperty(
    NPWidgetProperty property, void* value) {
  switch (property) {
    case NPWidgetPropertyLocation: {
      NPRect* r = static_cast<NPRect*>(value);
      gfx::Point location(r->left, r->top);
      gfx::Size size(r->right - r->left, r->bottom - r->top);
      vertical_ = size.height() > size.width();
      SetLocation(location,
                  vertical_ ? size.height() : size.width(),
                  total_length_);
      break;
    }
    case NPWidgetPropertyScrollbarPosition: {
      int32* position = static_cast<int*>(value);
      ScrollTo(*position);
      break;
    }
    case NPWidgetPropertyScrollbarDocumentSize: {
      int32* total_length = static_cast<int32*>(value);
      SetLocation(location_, length_, *total_length);
      break;
    }
    case NPWidgetPropertyScrollbarTickMarks:
      break;
    case NPWidgetPropertyScrollbarScrollByLine: {
      bool* forward = static_cast<bool*>(value);
      Scroll(!forward, SCROLL_BY_LINE, 0);
      break;
    }
    case NPWidgetPropertyScrollbarScrollByPage: {
      bool* forward = static_cast<bool*>(value);
      Scroll(!forward, SCROLL_BY_PAGE, 0);
      break;
    }
    case NPWidgetPropertyScrollbarScrollByDocument: {
      bool* forward = static_cast<bool*>(value);
      Scroll(!forward, SCROLL_BY_DOCUMENT, 0);
      break;
    }
    case NPWidgetPropertyScrollbarScrollByPixels: {
      int32 pixels = *static_cast<int32*>(value);
      bool forward = pixels >= 0;
      if (pixels < 0)
        pixels *= -1;
      Scroll(!forward, SCROLL_BY_PIXELS, pixels);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void PepperScrollbarWidget::SetLocation(
    const gfx::Point& location, int length, int total_length) {
  location_ = location;
  length_ = length;
  total_length_ = total_length;

  // As a result of zooming out, the thumb's current position might need to be
  // updated.
  if (thumb_position_ > MaximumThumbPosition())
    SetPosition(static_cast<float>(MaximumThumbPosition()), true);

  pixels_per_page_ = std::max<int>(std::max<int>(
      static_cast<int>(length_ * kMinFractionToStepWhenPaging),
      length_ - kMaxOverlapBetweenPages), 1);


  // did this above tho inside setpositoin...
  dirty_rect_ = dirty_rect_.Union(gfx::Rect(
        location_,
        gfx::Size(vertical_ ? thickness_: length_,
        vertical_ ? length_ : thickness_)));
  WidgetPropertyChanged(NPWidgetPropertyDirtyRect);
}

gfx::Rect PepperScrollbarWidget::GetLocation() const {
  return gfx::Rect(location_.x(),
                   location_.y(),
                   vertical_ ? thickness_ : length_,
                   vertical_ ? length_ : thickness_);
}

int PepperScrollbarWidget::GetPosition() const {
  return static_cast<int>(thumb_position_ * (total_length_ - length_) /
      MaximumThumbPosition());
}

bool PepperScrollbarWidget::OnMouseDown(const NPPepperEvent& event) {
  if (event.u.mouse.button == NPMouseButton_Right)
    return false;

  gfx::Point location(event.u.mouse.x, event.u.mouse.y);
  int pressed_part = HitTest(location);
  if (pressed_part == -1)
    return false;

  have_mouse_capture_ = true;

  pressed_part_ = pressed_part;
  int pressed_position = ThumbPosition(location);
  if (IsThumb(pressed_part_)) {
    pressed_position_ = pressed_position;
    drag_origin_ = thumb_position_;
    return true;
  } else if (IsTrackbar(pressed_part_) && ShouldCenterOnThumb(event)) {
    hovered_part_ = pressed_part_ = vertical_ ?
        VERTICAL_THUMB : HORIZONTAL_THUMB;
    drag_origin_ = thumb_position_;
    // Need to mark the middle of the thumb as the pressed position so that when
    // it's moved, the delta will be from the current pixel position of the
    // thumb to its new position.
    pressed_position_ = static_cast<int>(thumb_position_) + ThumbLength() / 2;
    MoveThumb(pressed_position);
    return true;
  }

  pressed_position_ = pressed_position;
  DCHECK(IsArrow(pressed_part_) || IsTrackbar(pressed_part_));

  AutoScroll(kInitialAutoscrollTimerDelay);

  return true;
}

bool PepperScrollbarWidget::OnMouseUp(const NPPepperEvent& event) {
  if (event.u.mouse.button != NPMouseButton_Left)
    return false;

  pressed_part_ = -1;
  pressed_position_ = -1;
  drag_origin_ = -1.0;
  StopTimerIfNeeded();

  gfx::Point location(event.u.mouse.x, event.u.mouse.y);
  return HitTest(location) != -1;
}

bool PepperScrollbarWidget::OnMouseMove(const NPPepperEvent& event) {
  gfx::Point location(event.u.mouse.x, event.u.mouse.y);
  if (IsThumb(pressed_part_)) {
    if (ShouldSnapBack(location)) {
      SetPosition(drag_origin_, true);
    } else {
      MoveThumb(ThumbPosition(location));
    }
    return true;
  }

  if (pressed_part_ != -1)
    pressed_position_ = ThumbPosition(location);

  int part = HitTest(location);
  if (part != hovered_part_) {
    if (pressed_part_ != -1) {
      if (part == pressed_part_) {
        // The mouse just moved back over the pressed part.  Need to start the
        // timers again.
        StartTimerIfNeeded(kAutoscrollTimerDelay);
      } else if (hovered_part_ == pressed_part_) {
        // The mouse is leaving the pressed part.  Stop the timer.
        StopTimerIfNeeded();
      }
    }

    hovered_part_ = part;
    return true;
  }

  return false;
}

bool PepperScrollbarWidget::OnMouseLeave(const NPPepperEvent& event) {
  hovered_part_ = -1;
  return have_mouse_capture_;
}

bool PepperScrollbarWidget::OnMouseWheel(const NPPepperEvent& event) {
  // TODO(jabdelmalek): handle middle clicking and moving the mouse diagonaly,
  // which would give both deltaX and deltaY.
  int delta = static_cast<int>(
      vertical_ ? event.u.wheel.deltaY : event.u.wheel.deltaX);
  // Delta is negative for down/right.
  bool backwards = delta > 0;
  if (delta < 0)
    delta = -delta;
  if (delta != 0) {
    if (event.u.wheel.scrollByPage) {
      Scroll(backwards, SCROLL_BY_PAGE, 0);
    } else {
      Scroll(backwards, SCROLL_BY_PIXELS, delta);
    }
  }

  return delta != 0;
}

bool PepperScrollbarWidget::OnKeyDown(const NPPepperEvent& event) {
  bool rv = true;
  int key = event.u.key.normalizedKeyCode;
  if ((vertical_ && key == base::VKEY_UP) ||
      (!vertical_ && key == base::VKEY_LEFT)) {
    Scroll(true, SCROLL_BY_LINE, 0);
  } else if ((vertical_ && key == base::VKEY_DOWN) ||
             (!vertical_ && key == base::VKEY_RIGHT)) {
    Scroll(false, SCROLL_BY_LINE, 0);
  } else if (key == base::VKEY_HOME) {
    Scroll(true, SCROLL_BY_DOCUMENT, 0);
  } else if (key == base::VKEY_END) {
    Scroll(false, SCROLL_BY_DOCUMENT, 0);
  } else if (key == base::VKEY_PRIOR) {
    Scroll(true, SCROLL_BY_PAGE, 0);
  } else if (key == base::VKEY_NEXT) {
    Scroll(false, SCROLL_BY_PAGE, 0);
  } else {
    rv = false;
  }

  return rv;
}

bool PepperScrollbarWidget::OnKeyUp(const NPPepperEvent& event) {
  return false;
}

int PepperScrollbarWidget::HitTest(const gfx::Point& location) const {
  if (BackArrowRect().Contains(location)) {
    return vertical_ ? UP_ARROW : LEFT_ARROW;
  } else if (ForwardArrowRect().Contains(location)) {
    return vertical_ ? DOWN_ARROW : RIGHT_ARROW;
  } else if (ThumbRect().Contains(location)) {
    return vertical_ ? VERTICAL_THUMB : HORIZONTAL_THUMB;
  } else if (BackTrackRect().Contains(location)) {
    return vertical_ ? VERTICAL_TRACK : HORIZONTAL_TRACK;
  } else if (ForwardTrackRect().Contains(location)) {
    return vertical_ ? VERTICAL_TRACK : HORIZONTAL_TRACK;
  }

  return -1;
}

int PepperScrollbarWidget::ThumbPosition(const gfx::Point& location) const {
  int position;
  if (vertical_) {
    position = location.y() - location_.y() - arrow_length_;
  } else {
    position = location.x() - location_.x() - arrow_length_;
  }

  return position;
}

void PepperScrollbarWidget::MoveThumb(int new_position) {
  float delta = static_cast<float>(new_position - pressed_position_);
  if (delta > MaximumThumbPosition() - thumb_position_) {
    // Don't want to go beyond max pos.
    delta = MaximumThumbPosition() - thumb_position_;
  } else if (delta < -1 * thumb_position_) {
    // Don't want to go below 0.
    delta = - 1 * thumb_position_;
  }

  SetPosition(thumb_position_ + delta, true);
}

void PepperScrollbarWidget::SetPosition(float position, bool notify_client) {
  float old_thumb_position = thumb_position_;
  thumb_position_ = position;
  if (IsThumb(pressed_part_)) {
    pressed_position_ =
        pressed_position_ + static_cast<int>(position - old_thumb_position);
  }

  if (notify_client)
    WidgetPropertyChanged(NPWidgetPropertyScrollbarPosition);

  dirty_rect_ = dirty_rect_.Union(gfx::Rect(
        location_,
        gfx::Size(vertical_ ? thickness_: length_,
        vertical_ ? length_ : thickness_)));
  WidgetPropertyChanged(NPWidgetPropertyDirtyRect);
}

void PepperScrollbarWidget::OnTimerFired() {
  AutoScroll(kAutoscrollTimerDelay);
}

void PepperScrollbarWidget::AutoScroll(float delay) {
  if (pressed_part_ == -1 || IsThumb(pressed_part_)) {
    NOTREACHED();
    return;
  }

  if (DidThumbCatchUpWithMouse())
    return;

  ScrollGranularity granularity =
      IsArrow(pressed_part_) ? SCROLL_BY_LINE : SCROLL_BY_PAGE;
  Scroll(IsBackscrolling(), granularity, 0);

  StartTimerIfNeeded(delay);
}

void PepperScrollbarWidget::Scroll(bool backwards,
                                   ScrollGranularity granularity,
                                   int pixels) {
  float new_position;
  if (granularity == SCROLL_BY_LINE || granularity == SCROLL_BY_PAGE) {
    int delta =
        granularity == SCROLL_BY_LINE ? kPixelsPerLine : pixels_per_page_;
    if (backwards)
      delta *= -1;

    // delta is in document pixels.  We want to convert to scrollbar pixels.
    new_position = thumb_position_ + ThumbPixelsFromDocumentPixels(delta);
  } else if (granularity == SCROLL_BY_PIXELS) {
    if (backwards)
      pixels *= -1;
    new_position = thumb_position_ + ThumbPixelsFromDocumentPixels(pixels);
  } else {
    DCHECK(granularity == SCROLL_BY_DOCUMENT);
    new_position =
        backwards ? 0.0f : static_cast<float>(MaximumThumbPosition());
  }

  SetPosition(std::max<float>(std::min<float>(
      new_position, static_cast<float>(MaximumThumbPosition())), 0.0), true);
}

void PepperScrollbarWidget::ScrollTo(int position) {
  if (position > (total_length_ - length_))
    position = total_length_ - length_;
  float thumb_position =
      static_cast<float>(position) * MaximumThumbPosition() /
          (total_length_ - length_);
  SetPosition(thumb_position, false);
}

void PepperScrollbarWidget::StartTimerIfNeeded(float delay) {
  if (IsThumb(pressed_part_))
    return;

  if (DidThumbCatchUpWithMouse())
    return;

  if (IsBackscrolling()) {
    if (thumb_position_ == 0)
      return;  // Already at beginning.
  } else {
    if (static_cast<int>(thumb_position_) == MaximumThumbPosition())
      return;  // Already at end.
  }

  DCHECK(!timer_.IsRunning());
  timer_.Start(
      base::TimeDelta::FromMilliseconds(static_cast<int64>(1000 * delay)),
      this,
      &PepperScrollbarWidget::OnTimerFired);
}

void PepperScrollbarWidget::StopTimerIfNeeded() {
  if (!timer_.IsRunning())
    return;

  timer_.Stop();
}

gfx::Rect PepperScrollbarWidget::BackArrowRect() const {
  int width = vertical_ ? thickness_ : arrow_length_;
  int height = vertical_ ? arrow_length_ : thickness_;
  return gfx::Rect(location_.x(), location_.y(), width, height);
}

gfx::Rect PepperScrollbarWidget::ForwardArrowRect() const {
  int x = location_.x();
  int y = location_.y();
  if (vertical_) {
    y += length_ - arrow_length_;
  } else {
    x += length_ - arrow_length_;
  }
  int width = vertical_ ? thickness_ : arrow_length_;
  int height = vertical_ ? arrow_length_ : thickness_;
  return gfx::Rect(x, y, width, height);
}

gfx::Rect PepperScrollbarWidget::BackTrackRect() const {
  int x = location_.x();
  int y = location_.y();
  if (vertical_) {
    y += arrow_length_;
  } else {
    x += arrow_length_;
  }
  int length = static_cast<int>(thumb_position_) + ThumbLength() / 2;
  int width = vertical_ ? thickness_ : length;
  int height = vertical_ ? length : thickness_;
  return gfx::Rect(x, y, width, height);
}

gfx::Rect PepperScrollbarWidget::ForwardTrackRect() const {
  int track_begin_offset =
      arrow_length_ + static_cast<int>(thumb_position_) + ThumbLength() / 2;
  int track_length =
      TrackLength() - ThumbLength() / 2 - static_cast<int>(thumb_position_);
  int x = location_.x();
  int y = location_.y();
  if (vertical_) {
    y += track_begin_offset;
  } else {
    x += track_begin_offset;
  }
  int width = vertical_ ? thickness_ : track_length;
  int height = vertical_ ? track_length : thickness_;
  return gfx::Rect(x, y, width, height);
}

gfx::Rect PepperScrollbarWidget::TrackRect() const {
  int x = location_.x();
  int y = location_.y();
  if (vertical_) {
    y += arrow_length_;
  } else {
    x += arrow_length_;
  }
  int width = vertical_ ? thickness_ : TrackLength();
  int height = vertical_ ? TrackLength() : thickness_;
  return gfx::Rect(x, y, width, height);
}

gfx::Rect PepperScrollbarWidget::ThumbRect() const {
  int thumb_begin_offset = arrow_length_ + static_cast<int>(thumb_position_);
  int x = location_.x();
  int y = location_.y();
  if (vertical_) {
    y += thumb_begin_offset;
  } else {
    x += thumb_begin_offset;
  }
  int width = vertical_ ? thickness_ : ThumbLength();
  int height = vertical_ ? ThumbLength() : thickness_;
  return gfx::Rect(x, y, width, height);
}

bool PepperScrollbarWidget::IsBackTrackPressed() const {
  return pressed_position_ >= 0 &&
         pressed_position_ < static_cast<int>(thumb_position_);
}

bool PepperScrollbarWidget::DidThumbCatchUpWithMouse() {
  if (IsTrackbar(pressed_part_) && IsThumbUnderMouse()) {
    hovered_part_ = vertical_ ?
        VERTICAL_THUMB : HORIZONTAL_THUMB;
    return true;
  }

  return false;
}

bool PepperScrollbarWidget::IsThumbUnderMouse() const {
  int thumb_start = static_cast<int>(thumb_position_);
  int thumb_end = thumb_start + ThumbLength();
  return pressed_position_ >=  thumb_start &&
         pressed_position_ < thumb_end;
}

bool PepperScrollbarWidget::IsBackscrolling() const {
  return pressed_part_ == UP_ARROW ||
         pressed_part_ == LEFT_ARROW ||
         ((pressed_part_ == VERTICAL_TRACK ||
           pressed_part_ == HORIZONTAL_TRACK) &&
          IsBackTrackPressed());
}

int PepperScrollbarWidget::TrackLength() const {
  int rv = length_ - 2 * arrow_length_;
  if (rv < 0)
    rv = 0;  // In case there isn't space after we draw the arrows.
  return rv;
}

int PepperScrollbarWidget::ThumbLength() const {
  float proportion = static_cast<float>(length_) / total_length_;
  int track_length = TrackLength();
  int length = static_cast<int>(proportion * track_length);
  length = std::max(length, MinimumThumbLength());
  if (length > track_length)
    length = 0;  // In case there's no space for the thumb.
  return length;
}

int PepperScrollbarWidget::MaximumThumbPosition() const {
  return TrackLength() - ThumbLength();
}

float PepperScrollbarWidget::ThumbPixelsFromDocumentPixels(int pixels) const {
  return static_cast<float>(pixels) * MaximumThumbPosition() /
      (total_length_ - length_);
}

bool PepperScrollbarWidget::IsArrow(int part) {
  return part == DOWN_ARROW ||
         part == LEFT_ARROW ||
         part == RIGHT_ARROW ||
         part == UP_ARROW;
}

bool PepperScrollbarWidget::IsTrackbar(int part) {
  return part == HORIZONTAL_TRACK || part == VERTICAL_TRACK;
}

bool PepperScrollbarWidget::IsThumb(int part) {
  return part == HORIZONTAL_THUMB || part == VERTICAL_THUMB;
}
