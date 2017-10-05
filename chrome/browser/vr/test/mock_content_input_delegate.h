// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_MOCK_CONTENT_INPUT_DELEGATE_H_
#define CHROME_BROWSER_VR_TEST_MOCK_CONTENT_INPUT_DELEGATE_H_

#include "base/macros.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"

namespace vr {

class MockContentInputDelegate : public ContentInputDelegate {
 public:
  MockContentInputDelegate();
  ~MockContentInputDelegate() override;

  MOCK_METHOD1(OnContentEnter, void(const gfx::PointF& normalized_hit_point));
  MOCK_METHOD0(OnContentLeave, void());
  MOCK_METHOD1(OnContentMove, void(const gfx::PointF& normalized_hit_point));
  MOCK_METHOD1(OnContentDown, void(const gfx::PointF& normalized_hit_point));
  MOCK_METHOD1(OnContentUp, void(const gfx::PointF& normalized_hit_point));

  // As move-only parameters aren't supported by mock methods, we will override
  // the functions explicitly and fwd the calls to the mocked functions.
  MOCK_METHOD2(FwdContentFlingStart,
               void(std::unique_ptr<blink::WebGestureEvent>& gesture,
                    const gfx::PointF& normalized_hit_point));
  MOCK_METHOD2(FwdContentFlingCancel,
               void(std::unique_ptr<blink::WebGestureEvent>& gesture,
                    const gfx::PointF& normalized_hit_point));
  MOCK_METHOD2(FwdContentScrollBegin,
               void(std::unique_ptr<blink::WebGestureEvent>& gesture,
                    const gfx::PointF& normalized_hit_point));
  MOCK_METHOD2(FwdContentScrollUpdate,
               void(std::unique_ptr<blink::WebGestureEvent>& gesture,
                    const gfx::PointF& normalized_hit_point));
  MOCK_METHOD2(FwdContentScrollEnd,
               void(std::unique_ptr<blink::WebGestureEvent>& gesture,
                    const gfx::PointF& normalized_hit_point));

  void OnContentFlingStart(std::unique_ptr<blink::WebGestureEvent> gesture,
                           const gfx::PointF& normalized_hit_point) override {
    FwdContentFlingStart(gesture, normalized_hit_point);
  }
  void OnContentFlingCancel(std::unique_ptr<blink::WebGestureEvent> gesture,
                            const gfx::PointF& normalized_hit_point) override {
    FwdContentFlingCancel(gesture, normalized_hit_point);
  }
  void OnContentScrollBegin(std::unique_ptr<blink::WebGestureEvent> gesture,
                            const gfx::PointF& normalized_hit_point) override {
    FwdContentScrollBegin(gesture, normalized_hit_point);
  }
  void OnContentScrollUpdate(std::unique_ptr<blink::WebGestureEvent> gesture,
                             const gfx::PointF& normalized_hit_point) override {
    FwdContentScrollUpdate(gesture, normalized_hit_point);
  }
  void OnContentScrollEnd(std::unique_ptr<blink::WebGestureEvent> gesture,
                          const gfx::PointF& normalized_hit_point) override {
    FwdContentScrollEnd(gesture, normalized_hit_point);
  }
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_MOCK_CONTENT_INPUT_DELEGATE_H_
