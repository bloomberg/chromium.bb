// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_NAVIGATION_CONTROLLER_DELEGATE_H_
#define COMPONENTS_WEB_VIEW_NAVIGATION_CONTROLLER_DELEGATE_H_

namespace web_view {

class NavigationControllerDelegate {
 public:
  virtual ~NavigationControllerDelegate() {}

  // Called when the navigation controller starts a navigation.
  //
  // TODO(erg): This should be removed and most of the PendingWebViewLoad logic
  // should be moved to NavigationEntry and/or NavigationController.
  virtual void OnNavigate(mojo::URLRequestPtr request) = 0;

  // Notification that blink has committed a pending load.
  virtual void OnDidNavigate() = 0;
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_NAVIGATION_CONTROLLER_DELEGATE_H_
