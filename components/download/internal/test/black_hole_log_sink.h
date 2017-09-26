// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/log_sink.h"

namespace download {
namespace test {

// A LogSink that does nothing with the calls to the interface.
class BlackHoleLogSink : public LogSink {
 public:
  BlackHoleLogSink() = default;
  ~BlackHoleLogSink() override = default;

  // LogSink implementation.
  void OnServiceStatusChanged() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlackHoleLogSink);
};

}  // namespace test
}  // namespace download
