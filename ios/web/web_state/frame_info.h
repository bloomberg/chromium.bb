// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_FRAME_INFO_H_
#define IOS_WEB_WEB_STATE_FRAME_INFO_H_

namespace web {

// Data Transfer Object which encapsulates information about a frame.
struct FrameInfo {
  explicit FrameInfo(bool main_frame) : is_main_frame(main_frame) {}
  // true if frame is main frame, false if subframe.
  bool is_main_frame;
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_FRAME_INFO_H_
