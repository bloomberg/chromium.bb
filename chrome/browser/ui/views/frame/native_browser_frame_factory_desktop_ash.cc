// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"

#include "chrome/browser/ui/ash/ash_init.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

bool NativeBrowserFrameFactory::ShouldCreateForAshDesktop(
    BrowserView* browser_view) {
  return chrome::ShouldOpenAshOnStartup() ||
      browser_view->browser()->host_desktop_type() ==
          chrome::HOST_DESKTOP_TYPE_ASH;
}
