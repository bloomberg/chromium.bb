// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_common_win.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/theme_provider.h"

namespace chrome {

bool ShouldUseNativeFrame(const BrowserView* browser_view,
                          const ui::ThemeProvider* theme_provider) {
  // We don't theme popup or app windows, so regardless of whether or not a
  // theme is active for normal browser windows, we don't want to use the custom
  // frame for popups/apps.
  if (!browser_view->IsBrowserTypeNormal()) {
    return true;
  }
  // Otherwise, we use the native frame when we're told we should by the theme
  // provider (e.g. no custom theme is active).
  return theme_provider->ShouldUseNativeFrame();
}

}  // namespace browser
