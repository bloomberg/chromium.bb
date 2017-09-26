// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/log_sink.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace download {
namespace test {

class MockLogSink : public LogSink {
 public:
  MockLogSink();
  ~MockLogSink() override;

  // LogSink implementation.
  MOCK_METHOD0(OnServiceStatusChanged, void());
};

}  // namespace test
}  // namespace download
