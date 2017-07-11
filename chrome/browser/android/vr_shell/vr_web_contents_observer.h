// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_WEB_CONTENTS_OBSERVER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace vr {
class ToolbarHelper;
class UiInterface;
}  // namespace vr

namespace vr_shell {

class VrShell;

class CONTENT_EXPORT VrWebContentsObserver
    : public content::WebContentsObserver {
 public:
  VrWebContentsObserver(content::WebContents* web_contents,
                        VrShell* vr_shell,
                        vr::UiInterface* ui_interface,
                        vr::ToolbarHelper* toolbar);
  ~VrWebContentsObserver() override;

  void SetUiInterface(vr::UiInterface* ui_interface);

 private:
  // WebContentsObserver implementation.
  void DidStartLoading() override;
  void DidStopLoading() override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;
  void DidChangeVisibleSecurityState() override;
  void WebContentsDestroyed() override;
  void WasHidden() override;
  void WasShown() override;
  void MainFrameWasResized(bool width_changed) override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;

  // This class does not own these pointers.
  VrShell* vr_shell_;
  vr::UiInterface* ui_interface_;
  vr::ToolbarHelper* toolbar_;

  DISALLOW_COPY_AND_ASSIGN(VrWebContentsObserver);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_WEB_CONTENTS_OBSERVER_H_
