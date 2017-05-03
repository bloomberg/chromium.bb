// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android_video_surface_chooser_impl.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "media/base/android/android_overlay_factory.h"
#include "media/base/android/mock_android_overlay.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;

namespace {
using ::media::AndroidOverlay;
using ::media::AndroidOverlayFactory;
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
  MOCK_METHOD1(StopUsingOverlayImmediately, void(AndroidOverlay*));

 private:
  std::unique_ptr<AndroidOverlay> overlay_;
};

// Mock factory which will return one overlay.
class MockAndroidOverlayFactory : public AndroidOverlayFactory {
 public:
  MockAndroidOverlayFactory()
      : overlay_(base::MakeUnique<MockAndroidOverlay>()),
        weak_this_factory_(this) {}

  // Called by CreateOverlay.
  MOCK_METHOD0(MockCreateOverlay, void());

  std::unique_ptr<AndroidOverlay> CreateOverlay(
      const AndroidOverlay::Config& config) override {
    MockCreateOverlay();

    // Notify the overlay  about the config that was used to create it.  We
    // can't do this during overlay construction since we might want the
    // overlay to set test expectations.
    if (overlay_)
      overlay_->SetConfig(config);

    return std::move(overlay_);
  }

  // Return the overlay, if we still own it.  One the client creates an
  // overlay, we'll re-assign ownership.
  MockAndroidOverlay* overlay() { return overlay_.get(); }

  // Set the overlay that we'll provide next, or nullptr.
  void SetOverlay(std::unique_ptr<MockAndroidOverlay> overlay) {
    overlay_ = std::move(overlay);
  }

  base::WeakPtr<MockAndroidOverlayFactory> GetWeakPtr() {
    return weak_this_factory_.GetWeakPtr();
  }

 private:
  std::unique_ptr<MockAndroidOverlay> overlay_;

  base::WeakPtrFactory<MockAndroidOverlayFactory> weak_this_factory_;
};
}  // namespace

namespace media {

// Unit tests for AndroidVideoSurfaceChooserImpl
class AndroidVideoSurfaceChooserImplTest : public testing::Test {
 public:
  ~AndroidVideoSurfaceChooserImplTest() override {}

  void SetUp() override {
    chooser_ = base::MakeUnique<AndroidVideoSurfaceChooserImpl>();
    factory_ = base::MakeUnique<NiceMock<MockAndroidOverlayFactory>>();
    factory_weak_ = factory_->GetWeakPtr();

    // We create a destruction observer.  By default, the overlay must not be
    // destroyed until the test completes.  Of course, the test may ask the
    // observer to expect something else.
    destruction_observer_ = factory_->overlay()->CreateDestructionObserver();
    destruction_observer_->DoNotAllowDestruction();
    overlay_callbacks_ = factory_->overlay()->GetCallbacks();
  }

  void TearDown() override {
    // If we get this far, the assume that whatever |destruction_observer_|
    // was looking for should have already happened.  We don't want the
    // lifetime of the observer to matter with respect to the overlay when
    // checking expectations.
    // Note that it might already be null.
    destruction_observer_ = nullptr;
  }

  void StartHelper(std::unique_ptr<MockAndroidOverlayFactory> factory) {
    chooser_->Initialize(
        base::Bind(&MockClient::UseOverlayImpl, base::Unretained(&client_)),
        base::Bind(&MockClient::UseSurfaceTexture, base::Unretained(&client_)),
        base::Bind(&MockClient::StopUsingOverlayImmediately,
                   base::Unretained(&client_)),
        std::move(factory));
  }

  std::unique_ptr<AndroidVideoSurfaceChooserImpl> chooser_;
  StrictMock<MockClient> client_;
  std::unique_ptr<NiceMock<MockAndroidOverlayFactory>> factory_;
  base::WeakPtr<MockAndroidOverlayFactory> factory_weak_;

  // Callbacks to control the overlay that will be vended by |factory_|
  MockAndroidOverlay::Callbacks overlay_callbacks_;

  std::unique_ptr<MockAndroidOverlay::DestructionObserver>
      destruction_observer_;
};

TEST_F(AndroidVideoSurfaceChooserImplTest,
       InitializeWithoutFactoryUsesSurfaceTexture) {
  // Calling Initialize() with no factory should result in a callback to use
  // surface texture.
  EXPECT_CALL(client_, UseSurfaceTexture());
  StartHelper(nullptr);
}

TEST_F(AndroidVideoSurfaceChooserImplTest, ProvideInitialOverlaySuccessfully) {
  // Providing a factory at startup should result in a switch to overlay.  It
  // should not switch to SurfaceTexture initially, since pre-M requires it.
  // Note that post-M (especially DS), it might be fine to start with ST.  We
  // just don't differentiate those cases yet in the impl.

  EXPECT_CALL(client_, UseSurfaceTexture()).Times(0);
  StartHelper(std::move(factory_));

  // Notify the chooser that the overlay is ready.  Expect that |client_| will
  // be told to use it.
  EXPECT_CALL(client_, UseOverlay(NotNull()));
  overlay_callbacks_.OverlayReady.Run();
}

TEST_F(AndroidVideoSurfaceChooserImplTest,
       InitializingWithFactoryCreatesOverlay) {
  // Providing a factory at startup should result in a switch to overlay.  It
  // should not switch to SurfaceTexture initially, since pre-M requires it.
  // Note that post-M (especially DS), it might be fine to start with ST.  We
  // just don't differentiate those cases yet in the impl.

  // Initially, there should be no callback into |client_|, since we haven't
  // told |chooser_| that the overlay is ready.  It should, however, request the
  // overlay from |factory_|.
  EXPECT_CALL(*factory_, MockCreateOverlay());
  StartHelper(std::move(factory_));
}

TEST_F(AndroidVideoSurfaceChooserImplTest,
       NullInitialOverlayUsesSurfaceTexture) {
  // If we provide a factory, but it fails to create an overlay, then |client_|
  // should be notified to use a surface texture.

  EXPECT_CALL(*factory_, MockCreateOverlay());
  EXPECT_CALL(client_, UseSurfaceTexture());
  // Replacing the overlay with null will destroy it.
  destruction_observer_->ExpectDestruction();
  factory_->SetOverlay(nullptr);
  StartHelper(std::move(factory_));
}

TEST_F(AndroidVideoSurfaceChooserImplTest,
       FailedInitialOverlayUsesSurfaceTexture) {
  // If we provide a factory, but the overlay that it provides returns 'failed',
  // then |client_| should use surface texture.
  EXPECT_CALL(*factory_, MockCreateOverlay());
  StartHelper(std::move(factory_));

  testing::Mock::VerifyAndClearExpectations(&client_);
  testing::Mock::VerifyAndClearExpectations(factory_weak_.get());

  // The overlay may be destroyed at any time after we send OverlayFailed.  It
  // doesn't have to be destroyed.  We just care that it hasn't been destroyed
  // before now.
  destruction_observer_ = nullptr;
  EXPECT_CALL(client_, UseSurfaceTexture());
  overlay_callbacks_.OverlayFailed.Run();
}

TEST_F(AndroidVideoSurfaceChooserImplTest,
       OnSurfaceDestroyedSendsNotification) {
  // If |chooser_| is notified about OnSurfaceDestroyed, then |client_| should
  // also be notified.

  EXPECT_CALL(*factory_, MockCreateOverlay());
  StartHelper(std::move(factory_));
  EXPECT_CALL(client_, UseOverlay(NotNull()));
  overlay_callbacks_.OverlayReady.Run();

  testing::Mock::VerifyAndClearExpectations(&client_);
  testing::Mock::VerifyAndClearExpectations(factory_weak_.get());

  // Switch to a surface texture.  OnSurfaceDestroyed should still be sent.
  EXPECT_CALL(client_, UseSurfaceTexture());
  chooser_->ReplaceOverlayFactory(nullptr);
  testing::Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, StopUsingOverlayImmediately(NotNull()));
  overlay_callbacks_.SurfaceDestroyed.Run();
}

TEST_F(AndroidVideoSurfaceChooserImplTest,
       OnSurfaceDestroyedSendsNotificationAfterSwitch) {
  // This tests two things.  First:
  // If |chooser_| is notified about OnSurfaceDestroyed, then |client_| should
  // also be notified even if |chooser_| has already told |client_| to
  // transition to SurfaceTexture.  It has no idea if |client_| has actually
  // transitioned, so it has to notify it to stop immediately, in case it
  // hasn't. Second: |chooser_| should notify |client_| to switch to surface
  // texture if it provided an overlay, and the factory is changed.  This
  // indicates that whoever provided the factory is revoking it, so we shouldn't
  // be using overlays from that factory anymore. We specifically test overlay
  // => no factory, since |chooser_| could elide multiple calls to switch to
  // surface texture.
  //
  // We test these together, since switching the factory is the only way we have
  // to make |chooser_| transition to SurfaceTexture without sending destroyed.

  EXPECT_CALL(*factory_, MockCreateOverlay());
  StartHelper(std::move(factory_));
  EXPECT_CALL(client_, UseOverlay(NotNull()));
  overlay_callbacks_.OverlayReady.Run();

  testing::Mock::VerifyAndClearExpectations(&client_);
  testing::Mock::VerifyAndClearExpectations(factory_weak_.get());

  // Switch factories, to notify the client back to switch to SurfaceTexture.
  EXPECT_CALL(client_, UseSurfaceTexture());
  chooser_->ReplaceOverlayFactory(nullptr);
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Destroy the original surface.
  EXPECT_CALL(client_, StopUsingOverlayImmediately(NotNull()));
  overlay_callbacks_.SurfaceDestroyed.Run();
}

TEST_F(AndroidVideoSurfaceChooserImplTest, NullLaterOverlayUsesSurfaceTexture) {
  // If an overlay factory is provided after startup that returns a null overlay
  // from CreateOverlay, |chooser_| should, at most, notify |client_| to use
  // SurfaceTexture zero or more times.

  // Start with SurfaceTexture.
  EXPECT_CALL(client_, UseSurfaceTexture());
  StartHelper(nullptr);
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Provide a factory that will return a null overlay.
  EXPECT_CALL(*factory_, MockCreateOverlay());
  EXPECT_CALL(client_, UseSurfaceTexture()).Times(AnyNumber());
  chooser_->ReplaceOverlayFactory(std::move(factory_));
  factory_weak_->SetOverlay(nullptr);
}

TEST_F(AndroidVideoSurfaceChooserImplTest, FailedLaterOverlayDoesNothing) {
  // If we send an overlay factory that returns an overlay, and that overlay
  // fails, then the client should not be notified except for zero or more
  // callbacks to switch to surface texture.

  // Start with SurfaceTexture.
  EXPECT_CALL(client_, UseSurfaceTexture());
  StartHelper(nullptr);
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Provide a factory.
  EXPECT_CALL(*factory_, MockCreateOverlay());
  EXPECT_CALL(client_, UseSurfaceTexture()).Times(AnyNumber());
  chooser_->ReplaceOverlayFactory(std::move(factory_));
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
  EXPECT_CALL(client_, UseSurfaceTexture());
  StartHelper(nullptr);
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Provide a factory.  |chooser_| should try to create an overlay.  We don't
  // care if a call to UseSurfaceTexture is elided or not.  Note that AVDA will
  // ignore duplicate calls anyway (MultipleSurfaceTextureCallbacksAreIgnored).
  EXPECT_CALL(*factory_, MockCreateOverlay());
  EXPECT_CALL(client_, UseSurfaceTexture()).Times(AnyNumber());
  chooser_->ReplaceOverlayFactory(std::move(factory_));
  testing::Mock::VerifyAndClearExpectations(&client_);
  testing::Mock::VerifyAndClearExpectations(factory_weak_.get());

  // Notify |chooser_| that the overlay is ready.
  EXPECT_CALL(client_, UseOverlay(NotNull()));
  overlay_callbacks_.OverlayReady.Run();
}

}  // namespace media
