// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A FilterHost implementation based on gmock.  Combined with setting a message
// loop on a filter, permits single-threaded testing of filters without
// requiring a pipeline.

#ifndef MEDIA_BASE_MOCK_FILTER_HOST_H_
#define MEDIA_BASE_MOCK_FILTER_HOST_H_

#include <string>

#include "base/scoped_ptr.h"
#include "media/base/filter_host.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockFilterHost : public FilterHost {
 public:
  MockFilterHost();
  virtual ~MockFilterHost();

  // FilterHost implementation.
  MOCK_METHOD0(InitializationComplete, void());
  MOCK_METHOD1(SetError, void(PipelineStatus error));
  MOCK_CONST_METHOD0(GetDuration, base::TimeDelta());
  MOCK_CONST_METHOD0(GetTime, base::TimeDelta());
  MOCK_METHOD1(SetTime, void(base::TimeDelta time));
  MOCK_METHOD1(SetDuration, void(base::TimeDelta duration));
  MOCK_METHOD1(SetBufferedTime, void(base::TimeDelta buffered_time));
  MOCK_METHOD1(SetTotalBytes, void(int64 total_bytes));
  MOCK_METHOD1(SetBufferedBytes, void(int64 buffered_bytes));
  MOCK_METHOD2(SetVideoSize, void(size_t width, size_t height));
  MOCK_METHOD1(SetStreaming, void(bool streamed));
  MOCK_METHOD1(SetLoaded, void(bool loaded));
  MOCK_METHOD1(SetNetworkActivity, void(bool network_activity));
  MOCK_METHOD0(NotifyEnded, void());
  MOCK_METHOD0(DisableAudioRenderer, void());
  MOCK_METHOD1(SetCurrentReadPosition, void(int64 offset));
  MOCK_METHOD0(GetCurrentReadPosition, int64());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFilterHost);
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_FILTER_HOST_H_
