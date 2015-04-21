// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_FOCUS_CONTROLLER_DELEGATE_H_
#define COMPONENTS_VIEW_MANAGER_FOCUS_CONTROLLER_DELEGATE_H_

namespace view_manager {

class ServerView;

class FocusControllerDelegate {
 public:
  virtual void OnFocusChanged(ServerView* old_focused_view,
                              ServerView* new_focused_view) = 0;

 protected:
  ~FocusControllerDelegate() {}
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_FOCUS_CONTROLLER_DELEGATE_H_
