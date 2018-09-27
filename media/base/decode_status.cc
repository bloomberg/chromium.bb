// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decode_status.h"

#include <ostream>

namespace media {

const char* GetDecodeStatusString(DecodeStatus status) {
  switch (status) {
    case DecodeStatus::OK:
      return "DecodeStatus::OK";
    case DecodeStatus::ABORTED:
      return "DecodeStatus::ABORTED";
    case DecodeStatus::DECODE_ERROR:
      return "DecodeStatus::DECODE_ERROR";
  }
}

std::ostream& operator<<(std::ostream& os, const DecodeStatus& status) {
  os << GetDecodeStatusString(status);
  return os;
}

}  // namespace media
