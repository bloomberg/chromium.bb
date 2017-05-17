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
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;

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

}  // namespace

namespace media {

// Unit tests for AndroidVideoSurfaceChooserImpl
class AndroidVideoSurfaceChooserImplTest : public testing::Test {
 public:
  ~AndroidVideoSurfaceChooserImplTest() override {}

  void SetUp() override {
    chooser_ = base::MakeUnique<AndroidVideoSurfaceChooserImpl>();
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
    chooser_->Initialize(
        base::Bind(&MockClient::UseOverlayImpl, base::Unretained(&client_)),
        base::Bind(&MockClient::UseSurfaceTexture, base::Unretained(&client_)),
        std::move(factory));
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
};

TEST_F(AndroidVideoSurfaceChooserImplTest,
       InitializeWithoutFactoryUsesSurfaceTexture) {
  // Calling Initialize() with no factory should result in a callback to use
  // surface texture.
  EXPECT_CALL(client_, UseSurfaceTexture());
  StartChooser(AndroidOverlayFactoryCB());
}

TEST_F(AndroidVideoSurfaceChooserImplTest, ProvideInitialOverlaySuccessfully) {
  // Providing a factory at startup should result in a switch to overlay.  It
  // should not switch to SurfaceTexture initially, since pre-M requires it.
  // Note that post-M (especially DS), it might be fine to start with ST.  We
  // just don't differentiate those cases yet in the impl.

  EXPECT_CALL(client_, UseSurfaceTexture()).Times(0);
  StartChooser(FactoryFor(std::move(overlay_)));

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
  EXPECT_CALL(*this, MockOnOverlayCreated());
  StartChooser(FactoryFor(std::move(overlay_)));
}

TEST_F(AndroidVideoSurfaceChooserImplTest,
       NullInitialOverlayUsesSurfaceTexture) {
  // If we provide a factory, but it fails to create an overlay, then |client_|
  // should be notified to use a surface texture.

  EXPECT_CALL(*this, MockOnOverlayCreated());
  EXPECT_CALL(client_, UseSurfaceTexture());
  StartChooser(FactoryFor(nullptr));
}

TEST_F(AndroidVideoSurfaceChooserImplTest,
       FailedInitialOverlayUsesSurfaceTexture) {
  // If we provide a factory, but the overlay that it provides returns 'failed',
  // then |client_| should use surface texture.
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
  EXPECT_CALL(client_, UseSurfaceTexture());
  StartChooser(AndroidOverlayFactoryCB());
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Provide a factory that will return a null overlay.
  EXPECT_CALL(*this, MockOnOverlayCreated());
  EXPECT_CALL(client_, UseSurfaceTexture()).Times(AnyNumber());
  chooser_->ReplaceOverlayFactory(FactoryFor(nullptr));
}

TEST_F(AndroidVideoSurfaceChooserImplTest, FailedLaterOverlayDoesNothing) {
  // If we send an overlay factory that returns an overlay, and that overlay
  // fails, then the client should not be notified except for zero or more
  // callbacks to switch to surface texture.

  // Start with SurfaceTexture.
  EXPECT_CALL(client_, UseSurfaceTexture());
  StartChooser(AndroidOverlayFactoryCB());
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Provide a factory.
  EXPECT_CALL(*this, MockOnOverlayCreated());
  EXPECT_CALL(client_, UseSurfaceTexture()).Times(AnyNumber());
  chooser_->ReplaceOverlayFactory(FactoryFor(std::move(overlay_)));
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
  StartChooser(AndroidOverlayFactoryCB());
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Provide a factory.  |chooser_| should try to create an overlay.  We don't
  // care if a call to UseSurfaceTexture is elided or not.  Note that AVDA will
  // ignore duplicate calls anyway (MultipleSurfaceTextureCallbacksAreIgnored).
  EXPECT_CALL(*this, MockOnOverlayCreated());
  EXPECT_CALL(client_, UseSurfaceTexture()).Times(AnyNumber());
  chooser_->ReplaceOverlayFactory(FactoryFor(std::move(overlay_)));
  testing::Mock::VerifyAndClearExpectations(&client_);
  testing::Mock::VerifyAndClearExpectations(this);

  // Notify |chooser_| that the overlay is ready.
  EXPECT_CALL(client_, UseOverlay(NotNull()));
  overlay_callbacks_.OverlayReady.Run();
}

}  // namespace media
