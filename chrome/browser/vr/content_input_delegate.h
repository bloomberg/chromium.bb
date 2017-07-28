// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_CONTENT_INPUT_DELEGATE_H_
#define CHROME_BROWSER_VR_CONTENT_INPUT_DELEGATE_H_

#include <memory>

namespace blink {
class WebGestureEvent;
}  // namespace blink

namespace gfx {
class PointF;
}  // namespace gfx

namespace vr {

// Receives interaction events with the web content.
class ContentInputDelegate {
 public:
  virtual ~ContentInputDelegate() {}

  virtual void OnContentEnter(const gfx::PointF& normalized_hit_point) = 0;
  virtual void OnContentLeave() = 0;
  virtual void OnContentMove(const gfx::PointF& normalized_hit_point) = 0;
  virtual void OnContentDown(const gfx::PointF& normalized_hit_point) = 0;
  virtual void OnContentUp(const gfx::PointF& normalized_hit_point) = 0;
  virtual void OnContentFlingStart(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point) = 0;
  virtual void OnContentFlingCancel(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point) = 0;
  virtual void OnContentScrollBegin(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point) = 0;
  virtual void OnContentScrollUpdate(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point) = 0;
  virtual void OnContentScrollEnd(
      std::unique_ptr<blink::WebGestureEvent> gesture,
      const gfx::PointF& normalized_hit_point) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_CONTENT_INPUT_DELEGATE_H_
