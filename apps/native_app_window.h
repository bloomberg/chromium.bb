// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_NATIVE_APP_WINDOW_H_
#define APPS_NATIVE_APP_WINDOW_H_

#include "apps/shell_window.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/base/base_window.h"
#include "ui/gfx/insets.h"

namespace apps {

// This is an interface to a native implementation of a shell window, used for
// new-style packaged apps. Shell windows contain a web contents, but no tabs
// or URL bar.
class NativeAppWindow : public ui::BaseWindow,
                        public web_modal::WebContentsModalDialogHost {
 public:
  // Called when the draggable regions are changed.
  virtual void UpdateDraggableRegions(
      const std::vector<extensions::DraggableRegion>& regions) = 0;

  virtual void SetFullscreen(bool fullscreen) = 0;
  virtual bool IsFullscreenOrPending() const = 0;

  // Returns true if the window is a panel that has been detached.
  virtual bool IsDetached() const = 0;

  // Called when the icon of the window changes.
  virtual void UpdateWindowIcon() = 0;

  // Called when the title of the window changes.
  virtual void UpdateWindowTitle() = 0;

  // Allows the window to handle unhandled keyboard messages coming back from
  // the renderer.
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) = 0;

  // TODO(jianli): once http://crbug.com/123007 is fixed, we'll no longer need
  // this.
  virtual void RenderViewHostChanged() = 0;

  // Returns the difference between the window bounds (including titlebar and
  // borders) and the content bounds, if any.
  virtual gfx::Insets GetFrameInsets() const = 0;

  virtual ~NativeAppWindow() {}
};

}  // namespace apps

#endif  // APPS_NATIVE_APP_WINDOW_H_
