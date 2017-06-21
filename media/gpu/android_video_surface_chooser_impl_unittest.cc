// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android_video_surface_chooser_impl.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "media/base/android/mock_android_overlay.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Values;

namespace {
using ::media::AndroidOverlay;
using ::media::MockAndroidOverlay;

class MockClient {
 public:
  MOCK_METHOD1(UseOverlay, void(AndroidOverlay*));

  void UseOverlayImpl(std::unique_ptr<AndroidOverlay> overlay) {
    UseOverlay(overlay.get());

    // Also take ownership of the overlay, so that it's not destroyed.
    overlay_ = std::move(overlay);
  }

  // Note that this won't clear |overlay_|, which is helpful.
  MOCK_METHOD0(UseSurfaceTexture, void(void));

 private:
  std::unique_ptr<AndroidOverlay> overlay_;
};

// Strongly-typed enums for TestParams.  It would be nice if Values() didn't
// do something that causes these to work anyway if you mis-match them.  Maybe
// it'll work better in a future gtest.  At the very least, it's a lot more
// readable than 'true' and 'false' in the test instantiations.
enum class ShouldUseOverlay { No, Yes };
enum class AllowDynamic { No, Yes };
enum class IsFullscreen { No, Yes };
enum class IsSecure { No, Yes };

using TestParams =
    std::tuple<ShouldUseOverlay, AllowDynamic, IsFullscreen, IsSecure>;

// Useful macro for instantiating tests.
#define Either(x) Values(x::No, x::Yes)

}  // namespace

namespace media {

// Unit tests for AndroidVideoSurfaceChooserImpl
class AndroidVideoSurfaceChooserImplTest
    : public testing::TestWithParam<TestParams> {
 public:
  ~AndroidVideoSurfaceChooserImplTest() override {}

  void SetUp() override {
    overlay_ = base::MakeUnique<MockAndroidOverlay>();

    // We create a destruction observer.  By default, the overlay must not be
    // destroyed until the test completes.  Of course, the test may ask the
    // observer to expect something else.
    destruction_observer_ = overlay_->CreateDestructionObserver();
    destruction_observer_->DoNotAllowDestruction();
    overlay_callbacks_ = overlay_->GetCallbacks();
  }

  void TearDown() override {
    // If we get this far, the assume that whatever |destruction_observer_|
    // was looking for should have already happened.  We don't want the
    // lifetime of the observer to matter with respect to the overlay when
    // checking expectations.
    // Note that it might already be null.
    destruction_observer_ = nullptr;
  }

  // Start the chooser, providing |factory| as the initial factory.
  void StartChooser(AndroidOverlayFactoryCB factory) {
    chooser_ = base::MakeUnique<AndroidVideoSurfaceChooserImpl>(allow_dynamic_);
    chooser_->Initialize(
        base::Bind(&MockClient::UseOverlayImpl, base::Unretained(&client_)),
        base::Bind(&MockClient::UseSurfaceTexture, base::Unretained(&client_)),
        std::move(factory), chooser_state_);
  }

  // AndroidOverlayFactoryCB is a RepeatingCallback, so we can't just bind
  // something that uses unique_ptr.  RepeatingCallback needs to copy it.
  class Factory {
   public:
    Factory(std::unique_ptr<MockAndroidOverlay> overlay,
            base::RepeatingCallback<void()> create_overlay_cb)
        : overlay_(std::move(overlay)),
          create_overlay_cb_(std::move(create_overlay_cb)) {}

    // Return whatever overlay we're given.  This is used to construct factory
    // callbacks for the chooser.
    std::unique_ptr<AndroidOverlay> ReturnOverlay(AndroidOverlayConfig config) {
      // Notify the mock.
      create_overlay_cb_.Run();
      if (overlay_)
        overlay_->SetConfig(std::move(config));
      return std::move(overlay_);
    }

   private:
    std::unique_ptr<MockAndroidOverlay> overlay_;
    base::RepeatingCallback<void()> create_overlay_cb_;
  };

  // Create a factory that will return |overlay| when run.
  AndroidOverlayFactoryCB FactoryFor(
      std::unique_ptr<MockAndroidOverlay> overlay) {
    Factory* factory = new Factory(
        std::move(overlay),
        base::Bind(&AndroidVideoSurfaceChooserImplTest::MockOnOverlayCreated,
                   base::Unretained(this)));

    // Leaky!
    return base::Bind(&Factory::ReturnOverlay, base::Unretained(factory));
  }

  // Called by the factory when it's run.
  MOCK_METHOD0(MockOnOverlayCreated, void());

  std::unique_ptr<AndroidVideoSurfaceChooserImpl> chooser_;
  StrictMock<MockClient> client_;
  std::unique_ptr<MockAndroidOverlay> overlay_;

  // Callbacks to control the overlay that will be vended by |factory_|
  MockAndroidOverlay::Callbacks overlay_callbacks_;

  std::unique_ptr<MockAndroidOverlay::DestructionObserver>
      destruction_observer_;

  // Will the chooser created by StartChooser() support dynamic surface changes?
  bool allow_dynamic_ = true;

  AndroidVideoSurfaceChooser::State chooser_state_;
};

TEST_F(AndroidVideoSurfaceChooserImplTest,
       InitializeWithoutFactoryUsesSurfaceTexture) {
  // Calling Initialize() with no factory should result in a callback to use
  // surface texture.
  EXPECT_CALL(client_, UseSurfaceTexture());
  StartChooser(AndroidOverlayFactoryCB());
}

TEST_F(AndroidVideoSurfaceChooserImplTest,
       NullInitialOverlayUsesSurfaceTexture) {
  // If we provide a factory, but it fails to create an overlay, then |client_|
  // should be notified to use a surface texture.

  chooser_state_.is_fullscreen = true;
  EXPECT_CALL(*this, MockOnOverlayCreated());
  EXPECT_CALL(client_, UseSurfaceTexture());
  StartChooser(FactoryFor(nullptr));
}

TEST_F(AndroidVideoSurfaceChooserImplTest,
       FailedInitialOverlayUsesSurfaceTexture) {
  // If we provide a factory, but the overlay that it provides returns 'failed',
  // then |client_| should use surface texture.
  chooser_state_.is_fullscreen = true;
  EXPECT_CALL(*this, MockOnOverlayCreated());
  StartChooser(FactoryFor(std::move(overlay_)));

  testing::Mock::VerifyAndClearExpectations(&client_);
  testing::Mock::VerifyAndClearExpectations(this);

  // The overlay may be destroyed at any time after we send OverlayFailed.  It
  // doesn't have to be destroyed.  We just care that it hasn't been destroyed
  // before now.
  destruction_observer_ = nullptr;
  EXPECT_CALL(client_, UseSurfaceTexture());
  overlay_callbacks_.OverlayFailed.Run();
}

TEST_F(AndroidVideoSurfaceChooserImplTest, NullLaterOverlayUsesSurfaceTexture) {
  // If an overlay factory is provided after startup that returns a null overlay
  // from CreateOverlay, |chooser_| should, at most, notify |client_| to use
  // SurfaceTexture zero or more times.

  // Start with SurfaceTexture.
  chooser_state_.is_fullscreen = true;
  EXPECT_CALL(client_, UseSurfaceTexture());
  allow_dynamic_ = true;
  StartChooser(AndroidOverlayFactoryCB());
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Provide a factory that will return a null overlay.
  EXPECT_CALL(*this, MockOnOverlayCreated());
  EXPECT_CALL(client_, UseSurfaceTexture()).Times(AnyNumber());
  chooser_->UpdateState(FactoryFor(nullptr), chooser_state_);
}

TEST_F(AndroidVideoSurfaceChooserImplTest, FailedLaterOverlayDoesNothing) {
  // If we send an overlay factory that returns an overlay, and that overlay
  // fails, then the client should not be notified except for zero or more
  // callbacks to switch to surface texture.

  // Start with SurfaceTexture.
  chooser_state_.is_fullscreen = true;
  EXPECT_CALL(client_, UseSurfaceTexture());
  StartChooser(AndroidOverlayFactoryCB());
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Provide a factory.
  EXPECT_CALL(*this, MockOnOverlayCreated());
  EXPECT_CALL(client_, UseSurfaceTexture()).Times(AnyNumber());
  chooser_->UpdateState(FactoryFor(std::move(overlay_)), chooser_state_);
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Fail the overlay.  We don't care if it's destroyed after that, as long as
  // it hasn't been destroyed yet.
  destruction_observer_ = nullptr;
  overlay_callbacks_.OverlayFailed.Run();
}

TEST_F(AndroidVideoSurfaceChooserImplTest,
       SuccessfulLaterOverlayNotifiesClient) {
  // |client_| is notified if we provide a factory that gets an overlay.

  // Start with SurfaceTexture.
  chooser_state_.is_fullscreen = true;
  EXPECT_CALL(client_, UseSurfaceTexture());
  StartChooser(AndroidOverlayFactoryCB());
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Provide a factory.  |chooser_| should try to create an overlay.  We don't
  // care if a call to UseSurfaceTexture is elided or not.  Note that AVDA will
  // ignore duplicate calls anyway (MultipleSurfaceTextureCallbacksAreIgnored).
  EXPECT_CALL(*this, MockOnOverlayCreated());
  EXPECT_CALL(client_, UseSurfaceTexture()).Times(AnyNumber());
  chooser_->UpdateState(FactoryFor(std::move(overlay_)), chooser_state_);
  testing::Mock::VerifyAndClearExpectations(&client_);
  testing::Mock::VerifyAndClearExpectations(this);

  // Notify |chooser_| that the overlay is ready.
  EXPECT_CALL(client_, UseOverlay(NotNull()));
  overlay_callbacks_.OverlayReady.Run();
}

TEST_P(AndroidVideoSurfaceChooserImplTest, OverlayIsUsedOrNotBasedOnState) {
// Provide a factory, and verify that it is used when the state says that it
// should be.  If the overlay is used, then we also verify that it does not
// switch to SurfaceTexture first, since pre-M requires it.

// c++14 can remove |n|, and std::get() by type.
#define IsTrue(x, n) (::testing::get<n>(GetParam()) == x::Yes);
  const bool should_use_overlay = IsTrue(ShouldUseOverlay, 0);
  allow_dynamic_ = IsTrue(AllowDynamic, 1);
  chooser_state_.is_fullscreen = IsTrue(IsFullscreen, 2);
  chooser_state_.is_secure = IsTrue(IsSecure, 3);

  if (should_use_overlay) {
    EXPECT_CALL(client_, UseSurfaceTexture()).Times(0);
    EXPECT_CALL(*this, MockOnOverlayCreated());
  } else {
    EXPECT_CALL(client_, UseSurfaceTexture());
    EXPECT_CALL(*this, MockOnOverlayCreated()).Times(0);
  }

  StartChooser(FactoryFor(std::move(overlay_)));

  // Verify that the overlay is provided when it becomes ready.
  if (should_use_overlay) {
    EXPECT_CALL(client_, UseOverlay(NotNull()));
    overlay_callbacks_.OverlayReady.Run();
  }
}

INSTANTIATE_TEST_CASE_P(NoFullscreenUsesSurfaceTexture,
                        AndroidVideoSurfaceChooserImplTest,
                        Combine(Values(ShouldUseOverlay::No),
                                Either(AllowDynamic),
                                Values(IsFullscreen::No),
                                Values(IsSecure::No)));
INSTANTIATE_TEST_CASE_P(FullscreenUsesOverlay,
                        AndroidVideoSurfaceChooserImplTest,
                        Combine(Values(ShouldUseOverlay::Yes),
                                Either(AllowDynamic),
                                Values(IsFullscreen::Yes),
                                Values(IsSecure::No)));
INSTANTIATE_TEST_CASE_P(SecureUsesOverlay,
                        AndroidVideoSurfaceChooserImplTest,
                        Combine(Values(ShouldUseOverlay::Yes),
                                Either(AllowDynamic),
                                Either(IsFullscreen),
                                Values(IsSecure::Yes)));

}  // namespace media
