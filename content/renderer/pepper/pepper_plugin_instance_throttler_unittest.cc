// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/pepper/pepper_plugin_instance_throttler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "ui/gfx/canvas.h"

using testing::_;
using testing::Return;

class GURL;

namespace content {

class PepperPluginInstanceThrottlerTest : public testing::Test {
 protected:
  PepperPluginInstanceThrottlerTest() : change_callback_calls_(0) {}

  void SetUp() override {
    blink::WebRect rect;
    rect.width = 100;
    rect.height = 100;
    throttler_.reset(new PepperPluginInstanceThrottler(
        nullptr, rect, kFlashPluginName, GURL("http://example.com"),
        content::RenderFrame::POWER_SAVER_MODE_PERIPHERAL_THROTTLED,
        base::Bind(&PepperPluginInstanceThrottlerTest::ChangeCallback,
                   base::Unretained(this))));
  }

  PepperPluginInstanceThrottler* throttler() {
    DCHECK(throttler_.get());
    return throttler_.get();
  }

  void DisablePowerSaverByRetroactiveWhitelist() {
    throttler()->DisablePowerSaver(
        PepperPluginInstanceThrottler::UNTHROTTLE_METHOD_BY_WHITELIST);
  }

  int change_callback_calls() { return change_callback_calls_; }

  void EngageThrottle() {
    throttler_->SetPluginThrottled(true);
  }

  void SendEventAndTest(blink::WebInputEvent::Type event_type,
                        bool expect_consumed,
                        bool expect_throttled,
                        int expect_change_callback_count) {
    blink::WebMouseEvent event;
    event.type = event_type;
    EXPECT_EQ(expect_consumed, throttler()->ConsumeInputEvent(event));
    EXPECT_EQ(expect_throttled, throttler()->is_throttled());
    EXPECT_EQ(expect_change_callback_count, change_callback_calls());
  }

 private:
  void ChangeCallback() { ++change_callback_calls_; }

  scoped_ptr<PepperPluginInstanceThrottler> throttler_;

  int change_callback_calls_;

  base::MessageLoop loop_;
};

TEST_F(PepperPluginInstanceThrottlerTest, ThrottleAndUnthrottleByClick) {
  EXPECT_FALSE(throttler()->is_throttled());
  EXPECT_EQ(0, change_callback_calls());

  EngageThrottle();
  EXPECT_TRUE(throttler()->is_throttled());
  EXPECT_EQ(1, change_callback_calls());

  // MouseUp while throttled should be consumed and disengage throttling.
  SendEventAndTest(blink::WebInputEvent::Type::MouseUp, true, false, 2);
}

TEST_F(PepperPluginInstanceThrottlerTest, ThrottleByKeyframe) {
  EXPECT_FALSE(throttler()->is_throttled());
  EXPECT_EQ(0, change_callback_calls());

  SkBitmap boring_bitmap;
  gfx::Canvas canvas(gfx::Size(20, 10), 1.0f, true);
  canvas.FillRect(gfx::Rect(20, 10), SK_ColorBLACK);
  canvas.FillRect(gfx::Rect(10, 10), SK_ColorWHITE);
  SkBitmap interesting_bitmap =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  // Don't throttle for a boring frame.
  throttler()->OnImageFlush(&boring_bitmap);
  EXPECT_FALSE(throttler()->is_throttled());
  EXPECT_EQ(0, change_callback_calls());

  // Don't throttle for non-consecutive interesting frames.
  throttler()->OnImageFlush(&interesting_bitmap);
  throttler()->OnImageFlush(&boring_bitmap);
  throttler()->OnImageFlush(&interesting_bitmap);
  throttler()->OnImageFlush(&boring_bitmap);
  throttler()->OnImageFlush(&interesting_bitmap);
  throttler()->OnImageFlush(&boring_bitmap);
  EXPECT_FALSE(throttler()->is_throttled());
  EXPECT_EQ(0, change_callback_calls());

  // Throttle after consecutive interesting frames.
  throttler()->OnImageFlush(&interesting_bitmap);
  throttler()->OnImageFlush(&interesting_bitmap);
  throttler()->OnImageFlush(&interesting_bitmap);
  throttler()->OnImageFlush(&interesting_bitmap);
  EXPECT_TRUE(throttler()->is_throttled());
  EXPECT_EQ(1, change_callback_calls());
}

TEST_F(PepperPluginInstanceThrottlerTest, IgnoreThrottlingAfterMouseUp) {
  EXPECT_FALSE(throttler()->is_throttled());
  EXPECT_EQ(0, change_callback_calls());

  // MouseUp before throttling engaged should not be consumed, but should
  // prevent subsequent throttling from engaging.
  SendEventAndTest(blink::WebInputEvent::Type::MouseUp, false, false, 0);

  EngageThrottle();
  EXPECT_FALSE(throttler()->is_throttled());
  EXPECT_EQ(0, change_callback_calls());
}

TEST_F(PepperPluginInstanceThrottlerTest, FastWhitelisting) {
  EXPECT_FALSE(throttler()->is_throttled());
  EXPECT_EQ(0, change_callback_calls());

  DisablePowerSaverByRetroactiveWhitelist();

  EngageThrottle();
  EXPECT_FALSE(throttler()->is_throttled());
  EXPECT_EQ(1, change_callback_calls());
}

TEST_F(PepperPluginInstanceThrottlerTest, SlowWhitelisting) {
  EXPECT_FALSE(throttler()->is_throttled());
  EXPECT_EQ(0, change_callback_calls());

  EngageThrottle();
  EXPECT_TRUE(throttler()->is_throttled());
  EXPECT_EQ(1, change_callback_calls());

  DisablePowerSaverByRetroactiveWhitelist();
  EXPECT_FALSE(throttler()->is_throttled());
  EXPECT_EQ(2, change_callback_calls());
}

TEST_F(PepperPluginInstanceThrottlerTest, EventConsumption) {
  EXPECT_FALSE(throttler()->is_throttled());
  EXPECT_EQ(0, change_callback_calls());

  EngageThrottle();
  EXPECT_TRUE(throttler()->is_throttled());
  EXPECT_EQ(1, change_callback_calls());

  // Consume but don't unthrottle on a variety of other events.
  SendEventAndTest(blink::WebInputEvent::Type::MouseDown, true, true, 1);
  SendEventAndTest(blink::WebInputEvent::Type::MouseWheel, true, true, 1);
  SendEventAndTest(blink::WebInputEvent::Type::MouseMove, true, true, 1);
  SendEventAndTest(blink::WebInputEvent::Type::KeyDown, true, true, 1);
  SendEventAndTest(blink::WebInputEvent::Type::KeyUp, true, true, 1);

  // Consume and unthrottle on MouseUp
  SendEventAndTest(blink::WebInputEvent::Type::MouseUp, true, false, 2);

  // Don't consume events after unthrottle.
  SendEventAndTest(blink::WebInputEvent::Type::MouseDown, false, false, 2);
  SendEventAndTest(blink::WebInputEvent::Type::MouseWheel, false, false, 2);
  SendEventAndTest(blink::WebInputEvent::Type::MouseMove, false, false, 2);
  SendEventAndTest(blink::WebInputEvent::Type::KeyDown, false, false, 2);
  SendEventAndTest(blink::WebInputEvent::Type::KeyUp, false, false, 2);

  // Subsequent MouseUps should also not be consumed.
  SendEventAndTest(blink::WebInputEvent::Type::MouseUp, false, false, 2);
}

}  // namespace content
