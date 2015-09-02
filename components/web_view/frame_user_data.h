// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_FRAME_USER_DATA_H_
#define COMPONENTS_WEB_VIEW_FRAME_USER_DATA_H_

namespace web_view {

// Arbitrary data that may be associated with each frame.
class FrameUserData {
 public:
  virtual ~FrameUserData() {}
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_FRAME_USER_DATA_H_
