// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_icon_win.h"

#include "chrome/installer/util/install_util.h"
#include "grit/theme_resources.h"

int GetAppListIconResourceId() {
  int icon_id = IDR_APP_LIST;
#if defined(GOOGLE_CHROME_BUILD)
  if (InstallUtil::IsChromeSxSProcess())
    icon_id = IDR_APP_LIST_SXS;
#endif
  return icon_id;
}
