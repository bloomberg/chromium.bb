// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_callback.h"

using ::testing::_;
using ::testing::StrictMock;

namespace media {

MockCallback::MockCallback() {}

MockCallback::~MockCallback() {
  Destructor();
}

void MockCallback::ExpectRunAndDelete() {
  EXPECT_CALL(*this, RunWithParams(_));
  EXPECT_CALL(*this, Destructor());
}

MockStatusCallback::MockStatusCallback() {}

MockStatusCallback::~MockStatusCallback() {
  Destructor();
}

// Required by GMock to allow the RunWithParams() expectation
// in ExpectRunAndDelete() to compile.
bool operator==(const Tuple1<PipelineError>& lhs,
                const Tuple1<PipelineError>& rhs) {
  return lhs.a == rhs.a;
}

void MockStatusCallback::ExpectRunAndDelete(PipelineError error) {
  EXPECT_CALL(*this, RunWithParams(Tuple1<PipelineError>(error)));
  EXPECT_CALL(*this, Destructor());
}

MockCallback* NewExpectedCallback() {
  StrictMock<MockCallback>* callback = new StrictMock<MockCallback>();
  callback->ExpectRunAndDelete();
  return callback;
}

MockStatusCallback* NewExpectedStatusCallback(PipelineError error) {
  StrictMock<MockStatusCallback>* callback =
      new StrictMock<MockStatusCallback>();
  callback->ExpectRunAndDelete(error);
  return callback;
}

}  // namespace media
