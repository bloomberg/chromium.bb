// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "content/common/frame_messages.h"
#include "content/common/view_message_enums.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/pepper/plugin_power_saver_helper_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "url/gurl.h"

namespace content {

class PluginPowerSaverHelperTest : public RenderViewTest {
 public:
  PluginPowerSaverHelperTest() : sink_(NULL) {}

  RenderFrameImpl* frame() {
    return static_cast<RenderFrameImpl*>(view_->GetMainRenderFrame());
  }

  PluginPowerSaverHelperImpl* power_saver_helper() {
    return frame()->GetPluginPowerSaverHelper();
  }

  void SetUp() override {
    RenderViewTest::SetUp();
    sink_ = &render_thread_->sink();
  }

 protected:
  IPC::TestSink* sink_;

  DISALLOW_COPY_AND_ASSIGN(PluginPowerSaverHelperTest);
};

TEST_F(PluginPowerSaverHelperTest, PosterImage) {
  size_t size = 3;
  blink::WebVector<blink::WebString> names(size);
  blink::WebVector<blink::WebString> values(size);

  blink::WebPluginParams params;
  params.url = GURL("http://b.com/foo.swf");

  params.attributeNames.swap(names);
  params.attributeValues.swap(values);

  params.attributeNames[0] = "poster";
  params.attributeNames[1] = "height";
  params.attributeNames[2] = "width";
  params.attributeValues[0] = "poster.jpg";
  params.attributeValues[1] = "100";
  params.attributeValues[2] = "100";

  EXPECT_EQ(GURL("http://a.com/poster.jpg"),
            power_saver_helper()->GetPluginInstancePosterImage(
                params, GURL("http://a.com/page.html")));

  // Ignore empty poster paramaters.
  params.attributeValues[0] = "";
  EXPECT_EQ(GURL(), power_saver_helper()->GetPluginInstancePosterImage(
                        params, GURL("http://a.com/page.html")));

  // Ignore poster parameter when plugin is big (shouldn't be throttled).
  params.attributeValues[0] = "poster.jpg";
  params.attributeValues[1] = "500";
  params.attributeValues[2] = "500";
  EXPECT_EQ(GURL(), power_saver_helper()->GetPluginInstancePosterImage(
                        params, GURL("http://a.com/page.html")));
}

TEST_F(PluginPowerSaverHelperTest, AllowSameOrigin) {
  EXPECT_FALSE(
      power_saver_helper()->ShouldThrottleContent(GURL(), 100, 100, nullptr));

  EXPECT_FALSE(
      power_saver_helper()->ShouldThrottleContent(GURL(), 1000, 1000, nullptr));
}

TEST_F(PluginPowerSaverHelperTest, DisallowCrossOriginUnlessLarge) {
  bool is_main_attraction = false;
  EXPECT_TRUE(power_saver_helper()->ShouldThrottleContent(
      GURL("http://other.com"), 100, 100, &is_main_attraction));
  EXPECT_FALSE(is_main_attraction);

  EXPECT_FALSE(power_saver_helper()->ShouldThrottleContent(
      GURL("http://other.com"), 1000, 1000, &is_main_attraction));
  EXPECT_TRUE(is_main_attraction);
}

TEST_F(PluginPowerSaverHelperTest, AlwaysAllowTinyContent) {
  bool is_main_attraction = false;
  EXPECT_FALSE(power_saver_helper()->ShouldThrottleContent(
      GURL(), 1, 1, &is_main_attraction));
  EXPECT_FALSE(is_main_attraction);

  EXPECT_FALSE(power_saver_helper()->ShouldThrottleContent(
      GURL("http://other.com"), 1, 1, &is_main_attraction));
  EXPECT_FALSE(is_main_attraction);

  EXPECT_FALSE(power_saver_helper()->ShouldThrottleContent(
      GURL("http://other.com"), 5, 5, &is_main_attraction));
  EXPECT_FALSE(is_main_attraction);

  EXPECT_TRUE(power_saver_helper()->ShouldThrottleContent(
      GURL("http://other.com"), 10, 10, &is_main_attraction));
  EXPECT_FALSE(is_main_attraction);
}

TEST_F(PluginPowerSaverHelperTest, TemporaryOriginWhitelist) {
  bool is_main_attraction = false;
  EXPECT_TRUE(power_saver_helper()->ShouldThrottleContent(
      GURL("http://other.com"), 100, 100, &is_main_attraction));
  EXPECT_FALSE(is_main_attraction);

  // Clear out other messages so we find just the plugin power saver IPCs.
  sink_->ClearMessages();

  power_saver_helper()->WhitelistContentOrigin(GURL("http://other.com"));
  EXPECT_FALSE(power_saver_helper()->ShouldThrottleContent(
      GURL("http://other.com"), 100, 100, &is_main_attraction));
  EXPECT_FALSE(is_main_attraction);

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

  EXPECT_FALSE(power_saver_helper()->ShouldThrottleContent(
      GURL("http://other.com"), 100, 100, nullptr));

  LoadHTML("<html></html>");

  EXPECT_TRUE(power_saver_helper()->ShouldThrottleContent(
      GURL("http://other.com"), 100, 100, nullptr));
}

}  // namespace content
