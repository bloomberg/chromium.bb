// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "media/base/mock_filters.h"
#include "media/mojo/clients/mojo_android_overlay.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom.h"

#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::StrictMock;

namespace media {

class MockAndroidOverlay : public StrictMock<mojom::AndroidOverlay> {
 public:
  MOCK_METHOD1(ScheduleLayout, void(const gfx::Rect& rect));
};

// Handy class with client-level callback mocks.
class ClientCallbacks {
 public:
  virtual void OnReady() = 0;
  virtual void OnFailed() = 0;
  virtual void OnDestroyed() = 0;
};

class MockClientCallbacks : public StrictMock<ClientCallbacks> {
 public:
  MOCK_METHOD0(OnReady, void());
  MOCK_METHOD0(OnFailed, void());
  MOCK_METHOD0(OnDestroyed, void());
};

class MockAndroidOverlayProvider
    : public StrictMock<mojom::AndroidOverlayProvider> {
 public:
  // These argument types lack default constructors, so gmock can't mock them.
  void CreateOverlay(mojom::AndroidOverlayRequest overlay_request,
                     mojom::AndroidOverlayClientPtr client,
                     mojom::AndroidOverlayConfigPtr config) override {
    overlay_request_ = std::move(overlay_request);
    client_ = std::move(client);
    config_ = std::move(config);
    OverlayCreated();
  }

  MOCK_METHOD0(OverlayCreated, void(void));

  mojom::AndroidOverlayRequest overlay_request_;
  mojom::AndroidOverlayClientPtr client_;
  mojom::AndroidOverlayConfigPtr config_;
};

// When the overlay client needs the provider interface, it'll ask us.
class MockInterfaceProvider
    : public StrictMock<service_manager::mojom::InterfaceProvider> {
 public:
  // |provider| is the provider that we'll provide, provided that we're only
  // asked to provide it once.  Savvy?
  MockInterfaceProvider(mojom::AndroidOverlayProvider& provider)
      : provider_binding_(&provider) {}

  // We can't mock GetInterface directly because of |handle|'s deleted ctor.
  MOCK_METHOD1(InterfaceGotten, void(const std::string&));

  void GetInterface(const std::string& name,
                    mojo::ScopedMessagePipeHandle handle) override {
    // Let the mock know.
    InterfaceGotten(name);

    // Actually do the work.
    provider_binding_.Bind(
        mojo::MakeRequest<mojom::AndroidOverlayProvider>(std::move(handle)));
  }

  mojo::Binding<mojom::AndroidOverlayProvider> provider_binding_;
};

class MojoAndroidOverlayTest : public ::testing::Test {
 public:
  MojoAndroidOverlayTest()
      : overlay_binding_(&mock_overlay_), interface_provider_(mock_provider_) {}

  ~MojoAndroidOverlayTest() override {}

  void SetUp() override {
    // Set up default config.
    config_.routing_token = base::UnguessableToken::Create();
    config_.rect = gfx::Rect(100, 200, 300, 400);
    config_.ready_cb = base::Bind(&MockClientCallbacks::OnReady,
                                  base::Unretained(&callbacks_));
    config_.failed_cb = base::Bind(&MockClientCallbacks::OnFailed,
                                   base::Unretained(&callbacks_));
    config_.destroyed_cb = base::Bind(&MockClientCallbacks::OnDestroyed,
                                      base::Unretained(&callbacks_));
  }

  void TearDown() override {
    overlay_client_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // Create an overlay in |overlay_client_| using the current config, but do
  // not bind anything to |overlay_request_| yet.
  void CreateOverlay() {
    EXPECT_CALL(interface_provider_,
                InterfaceGotten(mojom::AndroidOverlayProvider::Name_))
        .Times(1);
    EXPECT_CALL(mock_provider_, OverlayCreated());

    overlay_client_.reset(
        new MojoAndroidOverlay(&interface_provider_, config_));
    base::RunLoop().RunUntilIdle();
  }

  // Create an overlay, then provide it with |mock_overlay_|.
  void CreateAndInitializeOverlay() {
    CreateOverlay();

    // Bind an overlay to the request.
    overlay_binding_.Bind(std::move(mock_provider_.overlay_request_));
    base::RunLoop().RunUntilIdle();
  }

  // Notify |overlay_client_| that the surface is ready.
  void CreateSurface() {
    EXPECT_CALL(callbacks_, OnReady());
    const int surface_key = 123;
    mock_provider_.client_->OnSurfaceReady(surface_key);
    base::RunLoop().RunUntilIdle();
  }

  // Destroy the overlay.  This includes onSurfaceDestroyed cases.
  void DestroyOverlay() {
    mock_provider_.client_->OnDestroyed();
    base::RunLoop().RunUntilIdle();
  }

  // Mojo stuff.
  base::MessageLoop loop;

  // The mock provider that |overlay_client_| will talk to.
  // |interface_provider_| will bind it.
  MockAndroidOverlayProvider mock_provider_;

  // The mock overlay impl that |mock_provider_| will provide.
  MockAndroidOverlay mock_overlay_;
  mojo::Binding<mojom::AndroidOverlay> overlay_binding_;

  // The InterfaceProvider impl that will provide |mock_provider_| to the
  // overlay client |overlay_client_|.
  MockInterfaceProvider interface_provider_;

  // The client under test.
  std::unique_ptr<AndroidOverlay> overlay_client_;

  // Inital config for |CreateOverlay|.
  // Set to sane values, but feel free to modify before CreateOverlay().
  AndroidOverlay::Config config_;
  MockClientCallbacks callbacks_;
};

// Verify basic create => init => ready => destroyed.
TEST_F(MojoAndroidOverlayTest, CreateInitReadyDestroy) {
  CreateAndInitializeOverlay();
  CreateSurface();
  EXPECT_CALL(callbacks_, OnDestroyed());
  DestroyOverlay();
}

// Verify that initialization failure results in an onDestroyed callback.
TEST_F(MojoAndroidOverlayTest, InitFailure) {
  CreateOverlay();
  EXPECT_CALL(callbacks_, OnFailed());
  DestroyOverlay();
}

// Verify that we can destroy the overlay before providing a surface.
TEST_F(MojoAndroidOverlayTest, CreateInitDestroy) {
  CreateAndInitializeOverlay();
  EXPECT_CALL(callbacks_, OnFailed());
  DestroyOverlay();
}

// Test that layouts happen.
TEST_F(MojoAndroidOverlayTest, LayoutOverlay) {
  CreateAndInitializeOverlay();
  CreateSurface();

  gfx::Rect new_layout(5, 6, 7, 8);
  EXPECT_CALL(mock_overlay_, ScheduleLayout(new_layout));
  overlay_client_->ScheduleLayout(new_layout);
}

// Test that layouts are ignored before the client is notified about a surface.
TEST_F(MojoAndroidOverlayTest, LayoutBeforeSurfaceIsIgnored) {
  CreateAndInitializeOverlay();

  gfx::Rect new_layout(5, 6, 7, 8);
  EXPECT_CALL(mock_overlay_, ScheduleLayout(_)).Times(0);
  overlay_client_->ScheduleLayout(new_layout);
}

}  // namespace media
