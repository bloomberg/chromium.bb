// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/geolocation_service_impl.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/permission_manager.h"
#include "content/public/browser/permission_type.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/mock_permission_manager.h"
#include "content/test/test_render_frame_host.h"
#include "device/geolocation/geolocation_context.h"
#include "device/geolocation/geoposition.h"
#include "device/geolocation/public/interfaces/geolocation.mojom.h"
#include "device/geolocation/public/interfaces/geoposition.mojom.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebFeaturePolicyFeature.h"

using base::test::ScopedFeatureList;
using blink::mojom::PermissionStatus;
using device::GeolocationContext;
using device::Geoposition;
using device::mojom::GeolocationPtr;
using device::mojom::GeopositionPtr;
using device::mojom::GeolocationService;
using device::mojom::GeolocationServicePtr;

typedef base::Callback<void(PermissionStatus)> PermissionCallback;

namespace content {
namespace {

double kMockLatitude = 1.0;
double kMockLongitude = 10.0;
GURL kMainUrl = GURL("https://www.google.com/maps");
GURL kEmbeddedUrl = GURL("https://embeddables.com/someframe");

class TestPermissionManager : public MockPermissionManager {
 public:
  TestPermissionManager()
      : request_id_(PermissionManager::kNoPendingOperation) {}
  ~TestPermissionManager() override = default;

  int RequestPermission(PermissionType permissions,
                        RenderFrameHost* render_frame_host,
                        const GURL& requesting_origin,
                        bool user_gesture,
                        const PermissionCallback& callback) override {
    EXPECT_EQ(permissions, PermissionType::GEOLOCATION);
    EXPECT_TRUE(user_gesture);
    request_callback_.Run(callback);
    return request_id_;
  }

  void CancelPermissionRequest(int request_id) override {
    EXPECT_EQ(request_id, request_id_);
    cancel_callback_.Run();
  }

  void SetRequestId(int request_id) { request_id_ = request_id; }

  void SetRequestCallback(
      const base::Callback<void(const PermissionCallback&)>& request_callback) {
    request_callback_ = request_callback;
  }

  void SetCancelCallback(const base::Closure& cancel_callback) {
    cancel_callback_ = cancel_callback;
  }

 private:
  int request_id_;

  base::Callback<void(const PermissionCallback&)> request_callback_;
  base::Closure cancel_callback_;
};

class GeolocationServiceTest : public RenderViewHostImplTestHarness {
 protected:
  GeolocationServiceTest() {}

  ~GeolocationServiceTest() override {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    NavigateAndCommit(kMainUrl);
    permission_manager_.reset(new TestPermissionManager);
  }

  void CreateEmbeddedFrameAndGeolocationService(bool allow_via_feature_policy) {
    if (allow_via_feature_policy) {
      RenderFrameHostTester::For(main_rfh())
          ->SimulateFeaturePolicyHeader(
              blink::WebFeaturePolicyFeature::kGeolocation,
              std::vector<url::Origin>{url::Origin::Create(kEmbeddedUrl)});
    }
    RenderFrameHost* embedded_rfh =
        RenderFrameHostTester::For(main_rfh())->AppendChild("");
    RenderFrameHostTester::For(embedded_rfh)->InitializeRenderFrameIfNeeded();
    auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
        kEmbeddedUrl, embedded_rfh);
    navigation_simulator->Commit();
    embedded_rfh = navigation_simulator->GetFinalRenderFrameHost();

    service_.reset(new GeolocationServiceImpl(
        &context_, permission_manager_.get(), embedded_rfh));
    service_->Bind(mojo::MakeRequest(&service_ptr_));
  }

  GeolocationServicePtr* service_ptr() { return &service_ptr_; }

  GeolocationService* service() { return &*service_ptr_; }

  GeolocationContext* context() { return &context_; }

  TestPermissionManager* permission_manager() {
    return permission_manager_.get();
  }

 private:
  std::unique_ptr<GeolocationServiceImpl> service_;
  std::unique_ptr<TestPermissionManager> permission_manager_;
  GeolocationServicePtr service_ptr_;
  GeolocationContext context_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationServiceTest);
};

}  // namespace

TEST_F(GeolocationServiceTest, PermissionGrantedPolicyViolation) {
  // The embedded frame is not whitelisted.
  ScopedFeatureList feature_list;
  feature_list.InitFromCommandLine(
      features::kUseFeaturePolicyForPermissions.name, std::string());
  CreateEmbeddedFrameAndGeolocationService(/*allow_via_feature_policy=*/false);

  permission_manager()->SetRequestCallback(
      base::Bind([](const PermissionCallback& callback) {
        ADD_FAILURE() << "Permissions checked unexpectedly.";
      }));
  GeolocationPtr geolocation;
  service()->CreateGeolocation(mojo::MakeRequest(&geolocation), true);

  base::RunLoop loop;
  geolocation.set_connection_error_handler(loop.QuitClosure());

  geolocation->QueryNextPosition(base::BindOnce([](GeopositionPtr geoposition) {
    ADD_FAILURE() << "Position updated unexpectedly";
  }));
  auto mock_geoposition = base::MakeUnique<Geoposition>();
  mock_geoposition->latitude = kMockLatitude;
  mock_geoposition->longitude = kMockLongitude;
  context()->SetOverride(std::move(mock_geoposition));
  loop.Run();
}

TEST_F(GeolocationServiceTest, PermissionGrantedNoPolicyViolation) {
  // Whitelist the embedded frame.
  ScopedFeatureList feature_list;
  feature_list.InitFromCommandLine(
      features::kUseFeaturePolicyForPermissions.name, std::string());
  CreateEmbeddedFrameAndGeolocationService(/*allow_via_feature_policy=*/true);

  permission_manager()->SetRequestCallback(
      base::Bind([](const PermissionCallback& callback) {
        callback.Run(PermissionStatus::GRANTED);
      }));
  GeolocationPtr geolocation;
  service()->CreateGeolocation(mojo::MakeRequest(&geolocation), true);

  base::RunLoop loop;
  geolocation.set_connection_error_handler(base::BindOnce(
      [] { ADD_FAILURE() << "Connection error handler called unexpectedly"; }));

  geolocation->QueryNextPosition(base::BindOnce(
      [](base::Closure callback, GeopositionPtr geoposition) {
        EXPECT_DOUBLE_EQ(kMockLatitude, geoposition->latitude);
        EXPECT_DOUBLE_EQ(kMockLongitude, geoposition->longitude);
        callback.Run();
      },
      loop.QuitClosure()));
  auto mock_geoposition = base::MakeUnique<Geoposition>();
  mock_geoposition->latitude = kMockLatitude;
  mock_geoposition->longitude = kMockLongitude;
  context()->SetOverride(std::move(mock_geoposition));
  loop.Run();
}

TEST_F(GeolocationServiceTest, PermissionGrantedSync) {
  CreateEmbeddedFrameAndGeolocationService(/*allow_via_feature_policy=*/true);
  permission_manager()->SetRequestCallback(
      base::Bind([](const PermissionCallback& callback) {
        callback.Run(PermissionStatus::GRANTED);
      }));
  GeolocationPtr geolocation;
  service()->CreateGeolocation(mojo::MakeRequest(&geolocation), true);

  base::RunLoop loop;
  geolocation.set_connection_error_handler(base::BindOnce(
      [] { ADD_FAILURE() << "Connection error handler called unexpectedly"; }));

  geolocation->QueryNextPosition(base::BindOnce(
      [](base::Closure callback, GeopositionPtr geoposition) {
        EXPECT_DOUBLE_EQ(kMockLatitude, geoposition->latitude);
        EXPECT_DOUBLE_EQ(kMockLongitude, geoposition->longitude);
        callback.Run();
      },
      loop.QuitClosure()));
  auto mock_geoposition = base::MakeUnique<Geoposition>();
  mock_geoposition->latitude = kMockLatitude;
  mock_geoposition->longitude = kMockLongitude;
  context()->SetOverride(std::move(mock_geoposition));
  loop.Run();
}

TEST_F(GeolocationServiceTest, PermissionDeniedSync) {
  CreateEmbeddedFrameAndGeolocationService(/*allow_via_feature_policy=*/true);
  permission_manager()->SetRequestCallback(
      base::Bind([](const PermissionCallback& callback) {
        callback.Run(PermissionStatus::DENIED);
      }));
  GeolocationPtr geolocation;
  service()->CreateGeolocation(mojo::MakeRequest(&geolocation), true);

  base::RunLoop loop;
  geolocation.set_connection_error_handler(loop.QuitClosure());

  geolocation->QueryNextPosition(base::BindOnce([](GeopositionPtr geoposition) {
    ADD_FAILURE() << "Position updated unexpectedly";
  }));
  auto mock_geoposition = base::MakeUnique<Geoposition>();
  mock_geoposition->latitude = kMockLatitude;
  mock_geoposition->longitude = kMockLongitude;
  context()->SetOverride(std::move(mock_geoposition));
  loop.Run();
}

TEST_F(GeolocationServiceTest, PermissionGrantedAsync) {
  CreateEmbeddedFrameAndGeolocationService(/*allow_via_feature_policy=*/true);
  permission_manager()->SetRequestId(42);
  permission_manager()->SetRequestCallback(
      base::Bind([](const PermissionCallback& permission_callback) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(
                           [](const PermissionCallback& callback) {
                             callback.Run(PermissionStatus::GRANTED);
                           },
                           permission_callback));
      }));
  GeolocationPtr geolocation;
  service()->CreateGeolocation(mojo::MakeRequest(&geolocation), true);

  base::RunLoop loop;
  geolocation.set_connection_error_handler(base::BindOnce(
      [] { ADD_FAILURE() << "Connection error handler called unexpectedly"; }));

  geolocation->QueryNextPosition(base::BindOnce(
      [](base::Closure callback, GeopositionPtr geoposition) {
        EXPECT_DOUBLE_EQ(kMockLatitude, geoposition->latitude);
        EXPECT_DOUBLE_EQ(kMockLongitude, geoposition->longitude);
        callback.Run();
      },
      loop.QuitClosure()));
  auto mock_geoposition = base::MakeUnique<Geoposition>();
  mock_geoposition->latitude = kMockLatitude;
  mock_geoposition->longitude = kMockLongitude;
  context()->SetOverride(std::move(mock_geoposition));
  loop.Run();
}

TEST_F(GeolocationServiceTest, PermissionDeniedAsync) {
  CreateEmbeddedFrameAndGeolocationService(/*allow_via_feature_policy=*/true);
  permission_manager()->SetRequestId(42);
  permission_manager()->SetRequestCallback(
      base::Bind([](const PermissionCallback& permission_callback) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(
                           [](const PermissionCallback& callback) {
                             callback.Run(PermissionStatus::DENIED);
                           },
                           permission_callback));
      }));
  GeolocationPtr geolocation;
  service()->CreateGeolocation(mojo::MakeRequest(&geolocation), true);

  base::RunLoop loop;
  geolocation.set_connection_error_handler(loop.QuitClosure());

  geolocation->QueryNextPosition(base::BindOnce([](GeopositionPtr geoposition) {
    ADD_FAILURE() << "Position updated unexpectedly";
  }));
  auto mock_geoposition = base::MakeUnique<Geoposition>();
  mock_geoposition->latitude = kMockLatitude;
  mock_geoposition->longitude = kMockLongitude;
  context()->SetOverride(std::move(mock_geoposition));
  loop.Run();
}

TEST_F(GeolocationServiceTest, ServiceClosedBeforePermissionResponse) {
  CreateEmbeddedFrameAndGeolocationService(/*allow_via_feature_policy=*/true);
  permission_manager()->SetRequestId(42);
  GeolocationPtr geolocation;
  service()->CreateGeolocation(mojo::MakeRequest(&geolocation), true);
  // Don't immediately respond to the request.
  permission_manager()->SetRequestCallback(
      base::Bind([](const PermissionCallback& callback) {}));

  base::RunLoop loop;
  permission_manager()->SetCancelCallback(loop.QuitClosure());
  service_ptr()->reset();

  geolocation->QueryNextPosition(base::BindOnce([](GeopositionPtr geoposition) {
    ADD_FAILURE() << "Position updated unexpectedly";
  }));
  auto mock_geoposition = base::MakeUnique<Geoposition>();
  mock_geoposition->latitude = kMockLatitude;
  mock_geoposition->longitude = kMockLongitude;
  context()->SetOverride(std::move(mock_geoposition));
  loop.Run();
}

}  // namespace content
