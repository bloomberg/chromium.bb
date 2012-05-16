// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A FilterHost implementation based on gmock.  Combined with setting a message
// loop on a filter, permits single-threaded testing of filters without
// requiring a pipeline.

#ifndef MEDIA_BASE_MOCK_FILTER_HOST_H_
#define MEDIA_BASE_MOCK_FILTER_HOST_H_

#include <string>

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
  MOCK_METHOD1(SetNaturalVideoSize, void(const gfx::Size& size));
  MOCK_METHOD0(NotifyEnded, void());
  MOCK_METHOD0(DisableAudioRenderer, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFilterHost);
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_FILTER_HOST_H_
