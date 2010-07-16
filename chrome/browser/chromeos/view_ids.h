// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VIEW_IDS_H_
#define CHROME_BROWSER_CHROMEOS_VIEW_IDS_H_

#include "chrome/browser/view_ids.h"

// View ID used in ChromeOS.
enum ChromeOSViewIds {
  // Start with the offset that is big enough to avoid possible
  // collison.
  VIEW_ID_STATUS_AREA = VIEW_ID_PREDEFINED_COUNT + 10000,
  VIEW_ID_OTR_AVATAR,
};

#endif  // CHROME_BROWSER_CHROMEOS_VIEW_IDS_H_
