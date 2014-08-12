// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/theme_image_mapper.h"

#include "grit/theme_resources.h"

namespace chrome {

int MapThemeImage(HostDesktopType desktop_type, int resource) {
#if !defined(OS_CHROMEOS)
  if (desktop_type != HOST_DESKTOP_TYPE_NATIVE)
    return resource;

  switch (resource) {
    case IDR_CONTENT_TOP_CENTER:
      return IDR_CONTENT_TOP_CENTER_DESKTOP;
    case IDR_OTR_ICON:
      return IDR_OTR_ICON_DESKTOP;
    case IDR_THEME_FRAME:
      return IDR_THEME_FRAME_DESKTOP;
    case IDR_THEME_FRAME_INACTIVE:
      return IDR_THEME_FRAME_INACTIVE_DESKTOP;
    case IDR_THEME_FRAME_INCOGNITO:
      return IDR_THEME_FRAME_INCOGNITO_DESKTOP;
    case IDR_THEME_FRAME_INCOGNITO_INACTIVE:
      return IDR_THEME_FRAME_INCOGNITO_INACTIVE_DESKTOP;
    case IDR_THEME_TAB_BACKGROUND:
      return IDR_THEME_TAB_BACKGROUND_DESKTOP;
    case IDR_THEME_TAB_BACKGROUND_INCOGNITO:
      return IDR_THEME_TAB_BACKGROUND_INCOGNITO_DESKTOP;
    case IDR_THEME_TOOLBAR:
      return IDR_THEME_TOOLBAR_DESKTOP;
    default:
      break;
  }
#endif  // !defined(OS_CHROMEOS)

  return resource;
}

}  // namespace chrome
