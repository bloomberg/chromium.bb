// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HOST_DESKTOP_H_
#define CHROME_BROWSER_UI_HOST_DESKTOP_H_

namespace chrome {

// A value that specifies what desktop environment hosts a particular piece of
// UI.
// Note that HOST_DESKTOP_TYPE_ASH is always used on ChromeOS.
enum HostDesktopType {
  // The UI is hosted on the system native desktop.
  HOST_DESKTOP_TYPE_NATIVE = 0,

  // The UI is hosted in the synthetic Ash desktop.
  HOST_DESKTOP_TYPE_ASH,

  HOST_DESKTOP_TYPE_COUNT
};

/*
TODO(beng): implement utilities as needed, e.g.:
HostDesktopType GetActiveDesktop();
HostDesktopType GetHostDesktopTypeForNativeView(gfx::NativeView native_view);
HostDesktopType GetHostDesktopTypeForNativeWindow(
    gfx::NativeWindow native_window);
HostDesktopType GetHostDesktopTypeForBrowser(Browser* browser);
*/

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_HOST_DESKTOP_H_
