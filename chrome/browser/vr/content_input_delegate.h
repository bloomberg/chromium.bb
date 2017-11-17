// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_CONTENT_INPUT_DELEGATE_H_
#define CHROME_BROWSER_VR_CONTENT_INPUT_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/vr/macros.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"

namespace blink {
class WebGestureEvent;
class WebMouseEvent;
}  // namespace blink

namespace gfx {
class PointF;
}  // namespace gfx

namespace vr {

class ContentInputForwarder {
 public:
  virtual ~ContentInputForwarder() {}
  virtual void ForwardEvent(std::unique_ptr<blink::WebInputEvent> event,
                            int content_id) = 0;
};

class PlatformController;

// Receives interaction events with the web content.
class ContentInputDelegate {
 public:
  explicit ContentInputDelegate(ContentInputForwarder* content);
  virtual ~ContentInputDelegate();

  void OnContentEnter(const gfx::PointF& normalized_hit_point);
  void OnContentLeave();
  void OnContentMove(const gfx::PointF& normalized_hit_point);
  void OnContentDown(const gfx::PointF& normalized_hit_point);
  void OnContentUp(const gfx::PointF& normalized_hit_point);

  // The following functions are virtual so that they may be overridden in the
  // MockContentInputDelegate.
  VIRTUAL_FOR_MOCKS void OnContentFlingStart(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point);
  VIRTUAL_FOR_MOCKS void OnContentFlingCancel(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point);
  VIRTUAL_FOR_MOCKS void OnContentScrollBegin(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point);
  VIRTUAL_FOR_MOCKS void OnContentScrollUpdate(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point);
  VIRTUAL_FOR_MOCKS void OnContentScrollEnd(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point);

  void OnSwapContents(int new_content_id);

  void OnContentBoundsChanged(int width, int height);

  void OnPlatformControllerInitialized(PlatformController* controller) {
    controller_ = controller;
  }

 private:
  void UpdateGesture(const gfx::PointF& normalized_content_hit_point,
                     blink::WebGestureEvent& gesture);
  void SendGestureToContent(std::unique_ptr<blink::WebInputEvent> event);
  std::unique_ptr<blink::WebMouseEvent> MakeMouseEvent(
      blink::WebInputEvent::Type type,
      const gfx::PointF& normalized_web_content_location);
  bool ContentGestureIsLocked(blink::WebInputEvent::Type type);

  int content_tex_css_width_ = 0;
  int content_tex_css_height_ = 0;
  int content_id_ = 0;
  int locked_content_id_ = 0;

  ContentInputForwarder* content_ = nullptr;
  PlatformController* controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ContentInputDelegate);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_CONTENT_INPUT_DELEGATE_H_
