// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_ashwin.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/metro_utils/metro_chrome_win.h"
#include "ui/aura/remote_window_tree_host_win.h"

BrowserFrameAshWin::BrowserFrameAshWin(BrowserFrame* browser_frame,
                                       BrowserView* browser_view)
    : BrowserFrameAsh(browser_frame, browser_view) {
}

BrowserFrameAshWin::~BrowserFrameAshWin() {
}

void BrowserFrameAshWin::OnWindowFocused(aura::Window* gained_focus,
                                         aura::Window* lost_focus) {
  BrowserFrameAsh::OnWindowFocused(gained_focus, lost_focus);
  if (GetNativeWindow() != gained_focus)
    return;

  // TODO(shrikant): We need better way to handle chrome activation.
  // There may be cases where focus events do not follow a user
  // action to create or focus a window

  // If the activated window is in Metro mode, and the viewer process window is
  // not in the foreground, activate Metro Chrome.
  if (aura::RemoteWindowTreeHostWin::IsValid() &&
      !aura::RemoteWindowTreeHostWin::Instance()->IsForegroundWindow() &&
      !browser_shutdown::IsTryingToQuit()) {
    // PostTask because ActivateMetroChrome can not be called nested in another
    // ::SendMessage().
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(chrome::ActivateMetroChrome)));
  }
}
