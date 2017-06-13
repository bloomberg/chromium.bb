// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_devices_permission_checker.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "third_party/WebKit/public/platform/WebFeaturePolicy.h"
#include "url/origin.h"

namespace content {

namespace {

class TestWebContentsDelegate : public content::WebContentsDelegate {
 public:
  ~TestWebContentsDelegate() override {}

  bool CheckMediaAccessPermission(WebContents* web_contents,
                                  const GURL& security_origin,
                                  MediaStreamType type) override {
    return true;
  }
};

}  // namespace

class MediaDevicesPermissionCheckerTest : public RenderViewHostImplTestHarness {
 public:
  MediaDevicesPermissionCheckerTest()
      : origin_(GURL("https://www.google.com")),
        callback_run_(false),
        callback_result_(false) {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(origin_.GetURL());
    contents()->SetDelegate(&delegate_);
  }

 protected:
  // The header policy should only be set once on page load, so we refresh the
  // page to simulate that.
  void RefreshPageAndSetHeaderPolicy(blink::WebFeaturePolicyFeature feature,
                                     bool enabled) {
    NavigateAndCommit(origin_.GetURL());
    std::vector<url::Origin> whitelist;
    if (enabled)
      whitelist.push_back(origin_);
    RenderFrameHostTester::For(main_rfh())
        ->SimulateFeaturePolicyHeader(feature, whitelist);
  }

  bool CheckPermission(MediaDeviceType device_type) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    checker_.CheckPermission(
        device_type, main_rfh()->GetProcess()->GetID(),
        main_rfh()->GetRoutingID(),
        base::Bind(&MediaDevicesPermissionCheckerTest::CheckPermissionCallback,
                   base::Unretained(this)));
    run_loop.Run();

    EXPECT_TRUE(callback_run_);
    callback_run_ = false;
    return callback_result_;
  }

 private:
  void CheckPermissionCallback(bool result) {
    callback_run_ = true;
    callback_result_ = result;
    quit_closure_.Run();
  }

  url::Origin origin_;

  base::Closure quit_closure_;

  bool callback_run_;
  bool callback_result_;

  MediaDevicesPermissionChecker checker_;
  TestWebContentsDelegate delegate_;
};

// Basic tests for feature policy checks through the
// MediaDevicesPermissionChecker.  These tests are not meant to cover every edge
// case as the FeaturePolicy class itself is tested thoroughly in
// feature_policy_unittest.cc and in
// render_frame_host_feature_policy_unittest.cc.
TEST_F(MediaDevicesPermissionCheckerTest, CheckPermissionWithFeaturePolicy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUseFeaturePolicyForPermissions);
  // Mic and Camera should be enabled by default for a frame (if permission is
  // granted).
  EXPECT_TRUE(CheckPermission(MEDIA_DEVICE_TYPE_AUDIO_INPUT));
  EXPECT_TRUE(CheckPermission(MEDIA_DEVICE_TYPE_VIDEO_INPUT));

  RefreshPageAndSetHeaderPolicy(blink::WebFeaturePolicyFeature::kMicrophone,
                                /*enabled=*/false);
  EXPECT_FALSE(CheckPermission(MEDIA_DEVICE_TYPE_AUDIO_INPUT));
  EXPECT_TRUE(CheckPermission(MEDIA_DEVICE_TYPE_VIDEO_INPUT));

  RefreshPageAndSetHeaderPolicy(blink::WebFeaturePolicyFeature::kCamera,
                                /*enabled=*/false);
  EXPECT_TRUE(CheckPermission(MEDIA_DEVICE_TYPE_AUDIO_INPUT));
  EXPECT_FALSE(CheckPermission(MEDIA_DEVICE_TYPE_VIDEO_INPUT));

  // Ensure that the policy is ignored if kUseFeaturePolicyForPermissions is
  // disabled.
  base::test::ScopedFeatureList empty_feature_list;
  empty_feature_list.Init();
  EXPECT_TRUE(CheckPermission(MEDIA_DEVICE_TYPE_AUDIO_INPUT));
  EXPECT_TRUE(CheckPermission(MEDIA_DEVICE_TYPE_VIDEO_INPUT));
}

}  // namespace
