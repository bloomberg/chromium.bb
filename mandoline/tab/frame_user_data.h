// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_TAB_FRAME_USER_DATA_H_
#define MANDOLINE_TAB_FRAME_USER_DATA_H_

namespace mandoline {

// Arbitrary data that may be associated with each frame.
class FrameUserData {
 public:
  virtual ~FrameUserData() {}
};

}  // namespace mandoline

#endif  // MANDOLINE_TAB_FRAME_USER_DATA_H_
