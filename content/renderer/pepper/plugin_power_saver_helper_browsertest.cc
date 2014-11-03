// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "content/common/frame_messages.h"
#include "content/common/view_message_enums.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/pepper/plugin_power_saver_helper.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "url/gurl.h"

namespace content {

class PluginPowerSaverHelperTest : public RenderViewTest {
 public:
  PluginPowerSaverHelperTest() : sink_(NULL) {}

  RenderFrameImpl* frame() {
    return static_cast<RenderFrameImpl*>(view_->GetMainRenderFrame());
  }

  PluginPowerSaverHelper* power_saver_helper() {
    return frame()->plugin_power_saver_helper();
  }

  void SetUp() override {
    RenderViewTest::SetUp();
    sink_ = &render_thread_->sink();
  }

 protected:
  IPC::TestSink* sink_;

  DISALLOW_COPY_AND_ASSIGN(PluginPowerSaverHelperTest);
};

TEST_F(PluginPowerSaverHelperTest, AllowSameOrigin) {
  bool cross_origin = false;
  EXPECT_FALSE(power_saver_helper()->ShouldThrottleContent(
      GURL(), 100, 100, &cross_origin));
  EXPECT_FALSE(cross_origin);

  EXPECT_FALSE(power_saver_helper()->ShouldThrottleContent(
      GURL(), 1000, 1000, &cross_origin));
  EXPECT_FALSE(cross_origin);
}

TEST_F(PluginPowerSaverHelperTest, DisallowCrossOriginUnlessLarge) {
  bool cross_origin = false;
  EXPECT_TRUE(power_saver_helper()->ShouldThrottleContent(
      GURL("http://other.com"), 100, 100, &cross_origin));
  EXPECT_TRUE(cross_origin);

  EXPECT_FALSE(power_saver_helper()->ShouldThrottleContent(
      GURL("http://other.com"), 1000, 1000, &cross_origin));
  EXPECT_TRUE(cross_origin);
}

TEST_F(PluginPowerSaverHelperTest, TemporaryOriginWhitelist) {
  bool cross_origin = false;
  EXPECT_TRUE(power_saver_helper()->ShouldThrottleContent(
      GURL("http://other.com"), 100, 100, &cross_origin));
  EXPECT_TRUE(cross_origin);

  // Clear out other messages so we find just the plugin power saver IPCs.
  sink_->ClearMessages();

  power_saver_helper()->WhitelistContentOrigin(GURL("http://other.com"));
  EXPECT_FALSE(power_saver_helper()->ShouldThrottleContent(
      GURL("http://other.com"), 100, 100, &cross_origin));
  EXPECT_TRUE(cross_origin);

  // Test that we've sent an IPC to the browser.
  ASSERT_EQ(1u, sink_->message_count());
  const IPC::Message* msg = sink_->GetMessageAt(0);
  EXPECT_EQ(FrameHostMsg_PluginContentOriginAllowed::ID, msg->type());
  FrameHostMsg_PluginContentOriginAllowed::Param params;
  FrameHostMsg_PluginContentOriginAllowed::Read(msg, &params);
  EXPECT_EQ(GURL("http://other.com"), params.a);
}

TEST_F(PluginPowerSaverHelperTest, UnthrottleOnExPostFactoWhitelist) {
  base::RunLoop loop;
  power_saver_helper()->RegisterPeripheralPlugin(GURL("http://other.com"),
                                                 loop.QuitClosure());

  std::set<GURL> origin_whitelist;
  origin_whitelist.insert(GURL("http://other.com"));
  frame()->OnMessageReceived(FrameMsg_UpdatePluginContentOriginWhitelist(
      frame()->GetRoutingID(), origin_whitelist));

  // Runs until the unthrottle closure is run.
  loop.Run();
}

TEST_F(PluginPowerSaverHelperTest, ClearWhitelistOnNavigate) {
  power_saver_helper()->WhitelistContentOrigin(GURL("http://other.com"));

  bool cross_origin = false;
  EXPECT_FALSE(power_saver_helper()->ShouldThrottleContent(
      GURL("http://other.com"), 100, 100, &cross_origin));
  EXPECT_TRUE(cross_origin);

  LoadHTML("<html></html>");

  EXPECT_TRUE(power_saver_helper()->ShouldThrottleContent(
      GURL("http://other.com"), 100, 100, &cross_origin));
  EXPECT_TRUE(cross_origin);
}

}  // namespace content
