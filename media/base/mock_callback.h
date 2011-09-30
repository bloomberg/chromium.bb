// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MOCK_CALLBACK_H_
#define MEDIA_BASE_MOCK_CALLBACK_H_

#include "base/callback.h"
#include "media/base/pipeline_status.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// Utility mock for testing methods expecting Closures.  See
// NewExpectedClosure() below for a helper suitable when expectation order is
// not checked (or when the expectation can be set at mock construction time).
class MockClosure : public base::RefCountedThreadSafe<MockClosure> {
 public:
  MockClosure();
  virtual ~MockClosure();
  MOCK_METHOD0(Run, void());
 private:
  DISALLOW_COPY_AND_ASSIGN(MockClosure);
};

// Return a callback that expects to be run once.
base::Closure NewExpectedClosure();
base::Callback<void(PipelineStatus)> NewExpectedStatusCB(PipelineStatus status);

}  // namespace media

#endif  // MEDIA_BASE_MOCK_CALLBACK_H_
