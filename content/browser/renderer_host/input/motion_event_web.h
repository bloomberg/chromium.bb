// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOTION_EVENT_WEB_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOTION_EVENT_WEB_H_

#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/gesture_detection/motion_event.h"

namespace content {

// Implementation of ui::MotionEvent wrapping a WebTouchEvent.
class MotionEventWeb : public ui::MotionEvent {
 public:
  explicit MotionEventWeb(const blink::WebTouchEvent& event);
  virtual ~MotionEventWeb();

  // ui::MotionEvent
  virtual int GetId() const override;
  virtual Action GetAction() const override;
  virtual int GetActionIndex() const override;
  virtual size_t GetPointerCount() const override;
  virtual int GetPointerId(size_t pointer_index) const override;
  virtual float GetX(size_t pointer_index) const override;
  virtual float GetY(size_t pointer_index) const override;
  virtual float GetRawX(size_t pointer_index) const override;
  virtual float GetRawY(size_t pointer_index) const override;
  virtual float GetTouchMajor(size_t pointer_index) const override;
  virtual float GetTouchMinor(size_t pointer_index) const override;
  virtual float GetOrientation(size_t pointer_index) const override;
  virtual float GetPressure(size_t pointer_index) const override;
  virtual base::TimeTicks GetEventTime() const override;
  virtual ToolType GetToolType(size_t pointer_index) const override;
  virtual int GetButtonState() const override;
  virtual int GetFlags() const override;
  virtual scoped_ptr<MotionEvent> Clone() const override;
  virtual scoped_ptr<MotionEvent> Cancel() const override;

 private:
  blink::WebTouchEvent event_;
  Action cached_action_;
  int cached_action_index_;

  DISALLOW_COPY_AND_ASSIGN(MotionEventWeb);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOTION_EVENT_WEB_H_
