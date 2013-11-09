// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigator.h"

#include "content/browser/frame_host/navigator_delegate.h"

namespace content {

Navigator::Navigator(
    NavigationControllerImpl* nav_controller,
    NavigatorDelegate* delegate)
    : controller_(nav_controller),
      delegate_(delegate) {
}

}  // namespace content
