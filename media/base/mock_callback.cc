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

MockCallback* NewExpectedCallback() {
  StrictMock<MockCallback>* callback = new StrictMock<MockCallback>();
  callback->ExpectRunAndDelete();
  return callback;
}

}  // namespace media
