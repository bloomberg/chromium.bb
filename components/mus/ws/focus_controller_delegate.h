// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_FOCUS_CONTROLLER_DELEGATE_H_
#define COMPONENTS_MUS_WS_FOCUS_CONTROLLER_DELEGATE_H_

namespace mus {

class ServerView;

class FocusControllerDelegate {
 public:
  virtual void OnFocusChanged(ServerView* old_focused_view,
                              ServerView* new_focused_view) = 0;

 protected:
  ~FocusControllerDelegate() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_FOCUS_CONTROLLER_DELEGATE_H_
