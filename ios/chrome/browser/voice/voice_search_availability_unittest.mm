// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/voice_search_availability.h"

#include "base/scoped_observer.h"
#import "ios/chrome/browser/voice/fake_voice_search_availability.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for VoiceSearchAvailability.
class VoiceSearchAvailabilityTest : public PlatformTest {
 public:
  VoiceSearchAvailabilityTest()
      : mock_observer_(
            OCMStrictProtocolMock(@protocol(VoiceSearchAvailabilityObserver))) {
    availability_.AddObserver(mock_observer_);
  }
  ~VoiceSearchAvailabilityTest() override {
    availability_.RemoveObserver(mock_observer_);
    EXPECT_OCMOCK_VERIFY(mock_observer_);
  }

 protected:
  FakeVoiceSearchAvailability availability_;
  id<VoiceSearchAvailabilityObserver> mock_observer_ = nil;
};

// Tests that the voice search availability properly checks the voice over
// status and executes observer callbacks.
TEST_F(VoiceSearchAvailabilityTest, UnavailableDuringVoiceOver) {
  availability_.SetVoiceProviderEnabled(true);
  availability_.SetVoiceOverEnabled(false);
  ASSERT_TRUE(availability_.IsVoiceSearchAvailable());

  // Enable voice search in the fake checker, verifying that the value is
  // updated and that the observer is notified.
  OCMExpect([mock_observer_ voiceSearchAvailability:&availability_
                                updatedAvailability:false]);
  availability_.SetVoiceOverEnabled(true);
  EXPECT_FALSE(availability_.IsVoiceSearchAvailable());

  // Disable voice search in the fake checker, verifying that the value is
  // updated and that the observer is notified.
  OCMExpect([mock_observer_ voiceSearchAvailability:&availability_
                                updatedAvailability:true]);
  availability_.SetVoiceOverEnabled(false);
  EXPECT_TRUE(availability_.IsVoiceSearchAvailable());
}

// Tests that the voice search availability properly checks the voice provider
// status.
TEST_F(VoiceSearchAvailabilityTest, UnavailableForDisabledProvider) {
  // Disable VoiceOver so that it has no effect on voice search availability.
  availability_.SetVoiceOverEnabled(false);

  availability_.SetVoiceProviderEnabled(true);
  EXPECT_TRUE(availability_.IsVoiceSearchAvailable());

  availability_.SetVoiceProviderEnabled(false);
  EXPECT_FALSE(availability_.IsVoiceSearchAvailable());
}
