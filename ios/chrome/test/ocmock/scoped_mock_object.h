// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_OCMOCK_SCOPED_MOCK_OBJECT_H_
#define IOS_CHROME_TEST_OCMOCK_SCOPED_MOCK_OBJECT_H_

#import "base/mac/scoped_nsobject.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Helper class that constructs an OCMock and manages ownership of it.
template <typename NST>
class ScopedMockObject : public base::scoped_nsobject<id> {
 public:
  ScopedMockObject()
      : base::scoped_nsobject<id>(
            [[OCMockObject mockForClass:[NST class]] retain]) {}
};

#endif  // IOS_CHROME_TEST_OCMOCK_SCOPED_MOCK_OBJECT_H_
