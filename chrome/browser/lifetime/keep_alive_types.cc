// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/keep_alive_types.h"
#include "base/logging.h"

#ifndef NDEBUG
std::ostream& operator<<(std::ostream& out, const KeepAliveOrigin& origin) {
  switch (origin) {
    case KeepAliveOrigin::BACKGROUND_MODE_MANAGER:
      return out << "BACKGROUND_MODE_MANAGER";
    case KeepAliveOrigin::APP_LIST_SERVICE_VIEWS:
      return out << "APP_LIST_SERVICE_VIEWS";
    case KeepAliveOrigin::APP_LIST_SHOWER:
      return out << "APP_LIST_SHOWER";
    case KeepAliveOrigin::CHROME_APP_DELEGATE:
      return out << "CHROME_APP_DELEGATE";
    case KeepAliveOrigin::PANEL_VIEW:
      return out << "PANEL_VIEW";
    case KeepAliveOrigin::PROFILE_LOADER:
      return out << "PROFILE_LOADER";
    case KeepAliveOrigin::USER_MANAGER_VIEW:
      return out << "USER_MANAGER_VIEW";
  }

  NOTREACHED();
  return out << static_cast<int>(origin);
}
#endif  // ndef DEBUG
