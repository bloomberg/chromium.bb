// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_icon_win.h"

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_constants.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/installer/util/browser_distribution.h"
#endif

HICON GetAppIcon() {
  int icon_id = IDR_MAINFRAME;
#if defined(GOOGLE_CHROME_BUILD)
  if (BrowserDistribution::GetDistribution()->GetIconIndex())
    icon_id = IDR_SXS;
#endif
  return LoadIcon(GetModuleHandle(chrome::kBrowserResourcesDll),
                  MAKEINTRESOURCE(icon_id));
}
