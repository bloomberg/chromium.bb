// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_GESTURE_TEXT_SELECTOR_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_GESTURE_TEXT_SELECTOR_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "ui/events/gesture_detection/gesture_listeners.h"

namespace ui {
class GestureDetector;
class MotionEvent;
}

namespace content {
class GestureTextSelectorTest;

// Interface with which GestureTextSelector can select, unselect, show
// selection handles, or long press.
class CONTENT_EXPORT GestureTextSelectorClient {
 public:
  virtual ~GestureTextSelectorClient() {}

  virtual void ShowSelectionHandlesAutomatically() = 0;
  virtual void SelectRange(float x1, float y1, float x2, float y2) = 0;
  virtual void LongPress(base::TimeTicks time, float x, float y) = 0;
};

// A class to handle gesture-based text selection, such as when clicking first
// button on stylus input. It also generates a synthetic long press gesture on
// tap so that a word can be selected or the contextual menu can be shown.
class CONTENT_EXPORT GestureTextSelector : public ui::SimpleGestureListener {
 public:
  explicit GestureTextSelector(GestureTextSelectorClient* client);
  ~GestureTextSelector() override;

  // This should be called before |event| is seen by the platform gesture
  // detector or forwarded to web content.
  bool OnTouchEvent(const ui::MotionEvent& event);

 private:
  friend class GestureTextSelectorTest;
  FRIEND_TEST_ALL_PREFIXES(GestureTextSelectorTest,
                           ShouldStartTextSelection);

  // SimpleGestureListener implementation.
  bool OnSingleTapUp(const ui::MotionEvent& e) override;
  bool OnScroll(const ui::MotionEvent& e1,
                const ui::MotionEvent& e2,
                float distance_x,
                float distance_y) override;

  static bool ShouldStartTextSelection(const ui::MotionEvent& event);

  GestureTextSelectorClient* client_;
  bool text_selection_triggered_;
  bool secondary_button_pressed_;
  float anchor_x_;
  float anchor_y_;
  scoped_ptr<ui::GestureDetector> gesture_detector_;

  DISALLOW_COPY_AND_ASSIGN(GestureTextSelector);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_GESTURE_TEXT_SELECTOR_H_
