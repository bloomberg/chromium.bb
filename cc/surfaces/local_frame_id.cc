// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/local_frame_id.h"

#include "base/strings/stringprintf.h"

namespace cc {

std::string LocalFrameId::ToString() const {
  return base::StringPrintf("LocalFrameId(%d, %s" PRIu64 ")", local_id_,
                            nonce_.ToString().c_str());
}

std::ostream& operator<<(std::ostream& out,
                         const LocalFrameId& local_frame_id) {
  return out << local_frame_id.ToString();
}

}  // namespace cc
