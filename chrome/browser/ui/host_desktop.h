// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HOST_DESKTOP_H_
#define CHROME_BROWSER_UI_HOST_DESKTOP_H_

#include "ui/gfx/native_widget_types.h"

class Browser;

namespace chrome {

// A value that specifies what desktop environment hosts a particular piece of
// UI. Please take a look at the following document for details on choosing the
// right HostDesktopType:
// http://sites.google.com/a/chromium.org/dev/developers/design-documents/aura/multi-desktop
enum HostDesktopType {
  HOST_DESKTOP_TYPE_FIRST = 0,

  // The UI is hosted on the system native desktop.
  HOST_DESKTOP_TYPE_NATIVE = HOST_DESKTOP_TYPE_FIRST,

  // The UI is hosted in the synthetic Ash desktop.
#if defined(OS_CHROMEOS)
  HOST_DESKTOP_TYPE_ASH = HOST_DESKTOP_TYPE_NATIVE,
#else
  HOST_DESKTOP_TYPE_ASH,
#endif

  HOST_DESKTOP_TYPE_COUNT
};

HostDesktopType GetHostDesktopTypeForNativeView(gfx::NativeView native_view);
HostDesktopType GetHostDesktopTypeForNativeWindow(
    gfx::NativeWindow native_window);

// Returns the type of host desktop most likely to be in use.  This is the one
// most recently activated by the user.
HostDesktopType GetActiveDesktop();

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_HOST_DESKTOP_H_
