// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_THEME_IMAGE_MAPPER_H_
#define CHROME_BROWSER_UI_VIEWS_THEME_IMAGE_MAPPER_H_

#include "chrome/browser/ui/host_desktop.h"

namespace chrome {

// Maps the specified resource id to the appropriate one based on the supplied
// HostDesktopType.
int MapThemeImage(HostDesktopType desktop_type, int resource);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_THEME_IMAGE_MAPPER_H_
