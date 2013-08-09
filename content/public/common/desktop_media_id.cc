// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/desktop_media_id.h"

namespace content {

// static
DesktopMediaID DesktopMediaID::Parse(const std::string& str) {
  if (str == "screen")
    return DesktopMediaID(TYPE_SCREEN, 0);

  std::string window_prefix("window:");
  if (StartsWithASCII(str, window_prefix, true)) {
    int64 id;
    if (!base::StringToInt64(str.substr(window_prefix.size()), &id))
      return DesktopMediaID(TYPE_NONE, 0);
    return DesktopMediaID(TYPE_WINDOW, id);
  }

  return DesktopMediaID(TYPE_NONE, 0);
}

std::string DesktopMediaID::ToString() {
  switch (type) {
    case TYPE_NONE:
      NOTREACHED();
      return std::string();

    case TYPE_SCREEN:
      return "screen";

    case TYPE_WINDOW:
      return "window:" + base::Int64ToString(id);
  }
  NOTREACHED();
  return std::string();
}

}  // namespace content
