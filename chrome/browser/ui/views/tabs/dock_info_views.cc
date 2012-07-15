// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/dock_info.h"

#include "chrome/browser/ui/views/tabs/tab.h"
#include "build/build_config.h"

#if defined(USE_AURA) || defined(USE_ASH) || defined(OS_CHROMEOS)
// static
int DockInfo::GetHotSpotDeltaY() {
  return Tab::GetMinimumUnselectedSize().height() - 1;
}
#endif
