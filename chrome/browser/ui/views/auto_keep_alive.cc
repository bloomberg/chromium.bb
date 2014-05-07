// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/auto_keep_alive.h"

#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/host_desktop.h"
#include "ui/aura/window.h"
#include "ui/views/view_constants_aura.h"

AutoKeepAlive::AutoKeepAlive(gfx::NativeWindow window)
    : keep_alive_available_(false) {
  // In case of aura we want default to be keep alive not available for ash
  // because ash has keep alive set and we don't want additional keep alive
  // count. If there is a |window|, use its root window's kDesktopRootWindow
  // to test whether we are on desktop. Otherwise, use GetActiveDesktop().
  if (window) {
    gfx::NativeWindow native_window = window->GetRootWindow();
    if (native_window->GetProperty(views::kDesktopRootWindow))
      keep_alive_available_ = true;
  } else if (chrome::GetActiveDesktop() != chrome::HOST_DESKTOP_TYPE_ASH) {
    keep_alive_available_ = true;
  }

  if (keep_alive_available_)
    chrome::IncrementKeepAliveCount();
}

AutoKeepAlive::~AutoKeepAlive() {
  if (keep_alive_available_)
    chrome::DecrementKeepAliveCount();
}
