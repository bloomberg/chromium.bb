// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_service_impl.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/mock_permission_manager.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "third_party/WebKit/public/platform/WebFeaturePolicy.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission.mojom.h"
#include "url/origin.h"

using blink::mojom::PermissionStatus;
using blink::mojom::PermissionName;

namespace content {

namespace {

blink::mojom::PermissionDescriptorPtr CreatePermissionDescriptor(
    PermissionName name) {
  auto descriptor = blink::mojom::PermissionDescriptor::New();
  descriptor->name = name;
  return descriptor;
}

class TestPermissionManager : public MockPermissionManager {
 public:
  ~TestPermissionManager() override = default;

  PermissionStatus GetPermissionStatus(PermissionType permission,
                                       const GURL& requesting_origin,
                                       const GURL& embedding_origin) override {
    // Always return granted.
    return PermissionStatus::GRANTED;
  }

  int RequestPermissions(
      const std::vector<PermissionType>& permissions,
      RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      bool user_gesture,
      const base::Callback<void(const std::vector<PermissionStatus>&)>&
          callback) override {
    callback.Run(std::vector<PermissionStatus>(permissions.size(),
                                               PermissionStatus::GRANTED));
    return 0;
  }
};

}  // namespace

class PermissionServiceImplTest : public RenderViewHostTestHarness {
 public:
  PermissionServiceImplTest()
      : origin_(url::Origin::Create(GURL("https://www.google.com"))) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    static_cast<TestBrowserContext*>(browser_context())
        ->SetPermissionManager(base::MakeUnique<TestPermissionManager>());
    NavigateAndCommit(origin_.GetURL());
    service_context_.reset(new PermissionServiceContext(main_rfh()));
    service_impl_.reset(new PermissionServiceImpl(service_context_.get()));
  }

  void TearDown() override {
    service_impl_.reset();
    service_context_.reset();
    RenderViewHostTestHarness::TearDown();
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

  PermissionStatus HasPermission(PermissionName permission) {
    base::Callback<void(PermissionStatus)> callback =
        base::Bind(&PermissionServiceImplTest::PermissionStatusCallback,
                   base::Unretained(this));
    service_impl_->HasPermission(CreatePermissionDescriptor(permission),
                                 origin_, callback);
    EXPECT_EQ(1u, last_permission_statuses_.size());
    return last_permission_statuses_[0];
  }

  std::vector<PermissionStatus> RequestPermissions(
      const std::vector<PermissionName>& permissions) {
    std::vector<blink::mojom::PermissionDescriptorPtr> descriptors;
    for (PermissionName name : permissions)
      descriptors.push_back(CreatePermissionDescriptor(name));
    base::Callback<void(const std::vector<PermissionStatus>&)> callback =
        base::Bind(&PermissionServiceImplTest::RequestPermissionsCallback,
                   base::Unretained(this));
    service_impl_->RequestPermissions(std::move(descriptors), origin_,
                                      /*user_gesture=*/false, callback);
    EXPECT_EQ(permissions.size(), last_permission_statuses_.size());
    return last_permission_statuses_;
  }

 private:
  void PermissionStatusCallback(blink::mojom::PermissionStatus status) {
    last_permission_statuses_ = std::vector<PermissionStatus>{status};
  }

  void RequestPermissionsCallback(
      const std::vector<PermissionStatus>& statuses) {
    last_permission_statuses_ = statuses;
  }

  url::Origin origin_;

  base::Closure quit_closure_;

  std::vector<PermissionStatus> last_permission_statuses_;

  std::unique_ptr<PermissionServiceImpl> service_impl_;
  std::unique_ptr<PermissionServiceContext> service_context_;
};

// Basic tests for feature policy checks through the PermissionService.  These
// tests are not meant to cover every edge case as the FeaturePolicy class
// itself is tested thoroughly in feature_policy_unittest.cc and in
// render_frame_host_feature_policy_unittest.cc.
TEST_F(PermissionServiceImplTest, HasPermissionWithFeaturePolicy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUseFeaturePolicyForPermissions);
  // Geolocation should be enabled by default for a frame (if permission is
  // granted).
  EXPECT_EQ(PermissionStatus::GRANTED,
            HasPermission(PermissionName::GEOLOCATION));

  RefreshPageAndSetHeaderPolicy(blink::WebFeaturePolicyFeature::kGeolocation,
                                /*enabled=*/false);
  EXPECT_EQ(PermissionStatus::DENIED,
            HasPermission(PermissionName::GEOLOCATION));

  // Midi should be allowed even though geolocation was disabled.
  EXPECT_EQ(PermissionStatus::GRANTED, HasPermission(PermissionName::MIDI));

  // Now block midi.
  RefreshPageAndSetHeaderPolicy(blink::WebFeaturePolicyFeature::kMidiFeature,
                                /*enabled=*/false);
  EXPECT_EQ(PermissionStatus::DENIED, HasPermission(PermissionName::MIDI));

  // Ensure that the policy is ignored if kUseFeaturePolicyForPermissions is
  // disabled.
  base::test::ScopedFeatureList empty_feature_list;
  empty_feature_list.InitAndDisableFeature(
      features::kUseFeaturePolicyForPermissions);
  EXPECT_EQ(PermissionStatus::GRANTED, HasPermission(PermissionName::MIDI));
}

TEST_F(PermissionServiceImplTest, RequestPermissionsWithFeaturePolicy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kUseFeaturePolicyForPermissions);

  // Disable midi.
  RefreshPageAndSetHeaderPolicy(blink::WebFeaturePolicyFeature::kMidiFeature,
                                /*enabled=*/false);

  std::vector<PermissionStatus> result =
      RequestPermissions(std::vector<PermissionName>{PermissionName::MIDI});
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(PermissionStatus::DENIED, result[0]);

  // Request midi along with geolocation. Geolocation should be granted.
  result = RequestPermissions(std::vector<PermissionName>{
      PermissionName::MIDI, PermissionName::GEOLOCATION});
  EXPECT_EQ(2u, result.size());
  EXPECT_EQ(PermissionStatus::DENIED, result[0]);
  EXPECT_EQ(PermissionStatus::GRANTED, result[1]);
}

}  // namespace content
