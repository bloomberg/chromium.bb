// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_browsing/mac/test_utils.h"

#include "base/logging.h"

namespace safe_browsing {
namespace dmg {
namespace test {

bool ReadEntireStream(ReadStream* stream, std::vector<uint8_t>* data) {
  DCHECK(data->empty());
  uint8_t buffer[1024];
  size_t bytes_read = 0;
  do {
    bytes_read = 0;

    if (!stream->Read(buffer, sizeof(buffer), &bytes_read))
      return false;

    data->insert(data->end(), buffer, &buffer[bytes_read]);
  } while (bytes_read != 0);

  return true;
}

}  // namespace test
}  // namespace dmg
}  // namespace safe_browsing
