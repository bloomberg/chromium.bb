// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_callback.h"

#include "base/bind.h"

using ::testing::_;
using ::testing::StrictMock;

namespace media {

MockClosure::MockClosure() {}
MockClosure::~MockClosure() {}

base::Closure NewExpectedClosure() {
  StrictMock<MockClosure>* callback = new StrictMock<MockClosure>();
  EXPECT_CALL(*callback, Run());
  return base::Bind(&MockClosure::Run, callback);
}

class MockStatusCB : public base::RefCountedThreadSafe<MockStatusCB> {
 public:
  MockStatusCB() {}
  virtual ~MockStatusCB() {}
  MOCK_METHOD1(Run, void(PipelineStatus));
 private:
  DISALLOW_COPY_AND_ASSIGN(MockStatusCB);
};

base::Callback<void(PipelineStatus)> NewExpectedStatusCB(
    PipelineStatus status) {
  StrictMock<MockStatusCB>* callback = new StrictMock<MockStatusCB>();
  EXPECT_CALL(*callback, Run(status));
  return base::Bind(&MockStatusCB::Run, callback);
}

}  // namespace media
