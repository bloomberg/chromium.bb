// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/auto_keep_alive.h"

#include "chrome/browser/lifetime/application_lifetime.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/views/view_constants_aura.h"
#endif

AutoKeepAlive::AutoKeepAlive(gfx::NativeWindow window)
    : keep_alive_available_(true) {
#if defined(USE_AURA)
  // In case of aura we want default to be keep alive not available.
  keep_alive_available_ = false;
  if (window) {
    gfx::NativeWindow native_window = window->GetRootWindow();
    if (native_window->GetProperty(views::kDesktopRootWindow))
      keep_alive_available_ = true;
  }
#endif
  if (keep_alive_available_)
    chrome::IncrementKeepAliveCount();
}

AutoKeepAlive::~AutoKeepAlive() {
  if (keep_alive_available_)
    chrome::DecrementKeepAliveCount();
}
