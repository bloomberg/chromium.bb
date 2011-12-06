// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_icon_win.h"

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_constants.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/installer/util/install_util.h"
#endif

HICON GetAppIcon() {
  int icon_id = IDR_MAINFRAME;
#if defined(GOOGLE_CHROME_BUILD)
  if (InstallUtil::IsChromeSxSProcess())
    icon_id = IDR_SXS;
#endif
  return LoadIcon(GetModuleHandle(chrome::kBrowserResourcesDll),
                  MAKEINTRESOURCE(icon_id));
}

HICON GetAppIconForSize(int size) {
  int icon_id = IDR_MAINFRAME;
#if defined(GOOGLE_CHROME_BUILD)
  if (InstallUtil::IsChromeSxSProcess())
    icon_id = IDR_SXS;
#endif
  return static_cast<HICON>(
      LoadImage(GetModuleHandle(chrome::kBrowserResourcesDll),
                MAKEINTRESOURCE(icon_id),
                IMAGE_ICON,
                size,
                size,
                LR_DEFAULTCOLOR | LR_DEFAULTSIZE));
}
