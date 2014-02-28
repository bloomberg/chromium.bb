// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/screen_orientation/screen_orientation_dispatcher_host.h"
#include "content/browser/screen_orientation/screen_orientation_provider.h"
#include "content/common/screen_orientation_messages.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MockScreenOrientationProvider : public ScreenOrientationProvider {
 public:
  MockScreenOrientationProvider() : orientations_(0), unlock_called_(false) {}

  virtual void LockOrientation(blink::WebScreenOrientations orientations)
      OVERRIDE {
    orientations_ = orientations;

  }

  virtual void UnlockOrientation() OVERRIDE {
    unlock_called_ = true;
  }

  blink::WebScreenOrientations orientations() const {
    return orientations_;
  }

  bool unlock_called() const {
    return unlock_called_;
  }

  virtual ~MockScreenOrientationProvider() {}

 private:
  blink::WebScreenOrientations orientations_;
  bool unlock_called_;

  DISALLOW_COPY_AND_ASSIGN(MockScreenOrientationProvider);
};

class ScreenOrientationDispatcherHostTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    provider_ = new MockScreenOrientationProvider();

    dispatcher_ = new ScreenOrientationDispatcherHost();
    dispatcher_->SetProviderForTests(provider_);
  }

  // The dispatcher_ owns the provider_ but we still want to access it.
  MockScreenOrientationProvider* provider_;
  scoped_refptr<ScreenOrientationDispatcherHost> dispatcher_;
};

// Test that a NULL provider is correctly handled.
TEST_F(ScreenOrientationDispatcherHostTest, NullProvider) {
  dispatcher_->SetProviderForTests(NULL);

  bool message_was_ok = false;
  bool message_was_handled = dispatcher_->OnMessageReceived(
      ScreenOrientationHostMsg_Lock(blink::WebScreenOrientationPortraitPrimary),
          &message_was_ok);

  EXPECT_TRUE(message_was_ok);
  EXPECT_TRUE(message_was_handled);
}

// Test that when receiving a lock message, it is correctly dispatched to the
// ScreenOrientationProvider.
TEST_F(ScreenOrientationDispatcherHostTest, ProviderLock) {
  // If we change this array, update |orientationsToTestCount| below.
  blink::WebScreenOrientations orientationsToTest[] = {
    // The basic types.
    blink::WebScreenOrientationPortraitPrimary,
    blink::WebScreenOrientationPortraitSecondary,
    blink::WebScreenOrientationLandscapePrimary,
    blink::WebScreenOrientationLandscapeSecondary,
    // Some unions.
    blink::WebScreenOrientationLandscapePrimary |
        blink::WebScreenOrientationPortraitPrimary,
    blink::WebScreenOrientationLandscapePrimary |
        blink::WebScreenOrientationPortraitSecondary,
    blink::WebScreenOrientationPortraitPrimary |
        blink::WebScreenOrientationPortraitSecondary |
        blink::WebScreenOrientationLandscapePrimary |
        blink::WebScreenOrientationLandscapeSecondary,
    // Garbage values.
    0,
    100,
    42
  };

  // Unfortunately, initializer list constructor for std::list is not yet
  // something we can use.
  // Keep this in sync with |orientationsToTest|.
  int orientationsToTestCount = 10;

  for (int i = 0; i < orientationsToTestCount; ++i) {
    bool message_was_ok = false;
    bool message_was_handled = false;
    blink::WebScreenOrientations orientations = orientationsToTest[i];

    message_was_handled = dispatcher_->OnMessageReceived(
        ScreenOrientationHostMsg_Lock(orientations), &message_was_ok);

    EXPECT_TRUE(message_was_ok);
    EXPECT_TRUE(message_was_handled);
    EXPECT_EQ(orientations, provider_->orientations());
  }
}

// Test that when receiving an unlock message, it is correctly dispatched to the
// ScreenOrientationProvider.
TEST_F(ScreenOrientationDispatcherHostTest, ProviderUnlock) {
    bool message_was_ok = false;
    bool message_was_handled = dispatcher_->OnMessageReceived(
        ScreenOrientationHostMsg_Unlock(), &message_was_ok);

    EXPECT_TRUE(message_was_ok);
    EXPECT_TRUE(message_was_handled);
    EXPECT_TRUE(provider_->unlock_called());
}

} // namespace content
