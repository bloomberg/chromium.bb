// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_WEB_FRAMES_MANAGER_H_
#define IOS_WEB_PUBLIC_WEB_STATE_WEB_FRAMES_MANAGER_H_

#include <string>
#include <vector>

#include "base/macros.h"

namespace web {

class WebFrame;

// Stores and provides access to all WebFrame objects associated with a
// particular WebState.
// NOTE: WebFrame objects should be used directly from this manager and not
// stored elsewhere for later use becase WebFrames are frequently replaced.
// For example, a navigation will invalidate the WebFrame object for that frame.
class WebFramesManager {
 public:
  // Returns a list of all the web frames associated with WebState.
  // NOTE: Due to the asynchronous nature of renderer, this list may be
  // outdated.
  virtual const std::vector<WebFrame*>& GetAllWebFrames() = 0;
  // Returns the web frame for the main frame associated with WebState or null
  // if unknown.
  // NOTE: Due to the asynchronous nature of JavaScript to native messsaging,
  // this object may be outdated.
  virtual WebFrame* GetMainWebFrame() = 0;

  virtual ~WebFramesManager() {}

 protected:
  WebFramesManager() {}

  DISALLOW_COPY_AND_ASSIGN(WebFramesManager);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_WEB_FRAMES_MANAGER_H_
