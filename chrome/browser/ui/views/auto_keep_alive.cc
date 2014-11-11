// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/auto_keep_alive.h"

#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/host_desktop.h"

AutoKeepAlive::AutoKeepAlive(gfx::NativeWindow window)
    : keep_alive_available_(false) {
  // In case of aura we want default to be keep alive not available for ash
  // because ash has keep alive set and we don't want additional keep alive
  // count.
  chrome::HostDesktopType desktop_type =
      window ? chrome::GetHostDesktopTypeForNativeWindow(window)
             : chrome::GetActiveDesktop();
  if (desktop_type != chrome::HOST_DESKTOP_TYPE_ASH) {
    keep_alive_available_ = true;
    chrome::IncrementKeepAliveCount();
  }
}

AutoKeepAlive::~AutoKeepAlive() {
  if (keep_alive_available_)
    chrome::DecrementKeepAliveCount();
}
