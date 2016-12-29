// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_OCMOCK_SCOPED_VERIFYING_MOCK_OBJECT_H_
#define IOS_CHROME_TEST_OCMOCK_SCOPED_VERIFYING_MOCK_OBJECT_H_

#import "ios/chrome/test/ocmock/scoped_mock_object.h"
#include "third_party/ocmock/gtest_support.h"

// Helper class that constructs an OCMock and automatically verifies the mock
// upon destruction.
template <typename NST>
class ScopedVerifyingMockObject : public ScopedMockObject<NST> {
 public:
  ScopedVerifyingMockObject() {}

  ~ScopedVerifyingMockObject() {
    EXPECT_OCMOCK_VERIFY(ScopedMockObject<NST>::get());
  }
};

#endif  // IOS_CHROME_TEST_OCMOCK_SCOPED_VERIFYING_MOCK_OBJECT_H_
