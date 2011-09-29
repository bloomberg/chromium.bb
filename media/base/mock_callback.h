// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MOCK_CALLBACK_H_
#define MEDIA_BASE_MOCK_CALLBACK_H_

#include "base/callback.h"
#include "media/base/pipeline_status.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// Utility class that presents a base::Closure interface (through as_closure())
// and the ability to set a gMock expectation of being called (through
// ExpectCall).
class MockCallback : public base::RefCountedThreadSafe<MockCallback> {
 public:
  MockCallback();
  virtual ~MockCallback();
  MOCK_METHOD0(Run, void());
 private:
  DISALLOW_COPY_AND_ASSIGN(MockCallback);
};

// Return a callback that expects to be run once.
base::Closure NewExpectedCallback();
base::Callback<void(PipelineStatus)> NewExpectedStatusCB(PipelineStatus status);

}  // namespace media

#endif  // MEDIA_BASE_MOCK_CALLBACK_H_
