// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_web_contents_observer.h"

#include "chrome/browser/android/vr_shell/ui_interface.h"
#include "content/public/browser/navigation_handle.h"

namespace vr_shell {

VrWebContentsObserver::VrWebContentsObserver(content::WebContents* web_contents,
                                             UiInterface* ui_interface)
    : WebContentsObserver(web_contents),
      ui_interface_(ui_interface) {}

VrWebContentsObserver::~VrWebContentsObserver() {}

void VrWebContentsObserver::SetUiInterface(UiInterface* ui_interface) {
  ui_interface_ = ui_interface;
}

void VrWebContentsObserver::DidStartLoading() {
  ui_interface_->SetLoading(true);
}

void VrWebContentsObserver::DidStopLoading() {
  ui_interface_->SetLoading(false);
}

void VrWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  ui_interface_->SetURL(navigation_handle->GetURL());
}

void VrWebContentsObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  ui_interface_->SetURL(navigation_handle->GetURL());
}

void VrWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  ui_interface_->SetURL(navigation_handle->GetURL());
}

void VrWebContentsObserver::DidToggleFullscreenModeForTab(
    bool entered_fullscreen, bool will_cause_resize) {
  // TODO(amp): Use will_cause_resize to signal ui of pending size changes.
  if (entered_fullscreen) {
    ui_interface_->SetMode(UiInterface::Mode::CINEMA);
  } else {
    ui_interface_->SetMode(UiInterface::Mode::STANDARD);
  }
}

}  // namespace vr_shell
