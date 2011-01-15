// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MOCK_CALLBACK_H_
#define MEDIA_BASE_MOCK_CALLBACK_H_

#include "base/callback.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// Helper class used to test that callbacks are executed.
//
// In most cases NewExpectedCallback() can be used but if need be you can
// manually set expectations on an MockCallback object:
//
//   StrictMock<MockCallback>* callback =
//       new StrictMock<MockCallback>();
//   EXPECT_CALL(*callback, RunWithParams(_));
//   EXPECT_CALL(*callback, Destructor());
//
// ...or the equivalent and less verbose:
//   StrictMock<MockCallback>* callback =
//       new StrictMock<MockCallback>();
//   callback->ExpectRunAndDelete();
//
// ...or if you don't care about verifying callback deletion:
//
//   NiceMock<MockCallback>* callback =
//       new NiceMock<MockCallback>();
//   EXPECT_CALL(*callback, RunWithParams(_));
class MockCallback : public CallbackRunner<Tuple0> {
 public:
  MockCallback();
  virtual ~MockCallback();

  MOCK_METHOD1(RunWithParams, void(const Tuple0& params));

  // Can be used to verify the object is destroyed.
  MOCK_METHOD0(Destructor, void());

  // Convenience function to set expectations for the callback to execute and
  // deleted.
  void ExpectRunAndDelete();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCallback);
};

// Convenience function that automatically creates and sets an expectation for
// the callback to run.
MockCallback* NewExpectedCallback();

}  // namespace media

#endif  // MEDIA_BASE_MOCK_CALLBACK_H_
