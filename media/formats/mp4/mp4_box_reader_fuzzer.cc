// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "media/base/media_log.h"
#include "media/formats/mp4/box_reader.h"

class NullMediaLog : public media::MediaLog {
 public:
  NullMediaLog() = default;
  ~NullMediaLog() override = default;

  void AddEventLocked(std::unique_ptr<media::MediaLogEvent> event) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NullMediaLog);
};

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  NullMediaLog media_log;
  std::unique_ptr<media::mp4::BoxReader> reader;
  if (media::mp4::BoxReader::ReadTopLevelBox(data, size, &media_log, &reader) ==
      media::mp4::ParseResult::kOk) {
    CHECK(reader);
    ignore_result(reader->ScanChildren());
  }
  return 0;
}
