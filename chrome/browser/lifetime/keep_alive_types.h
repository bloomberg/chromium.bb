// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_KEEP_ALIVE_TYPES_H_
#define CHROME_BROWSER_LIFETIME_KEEP_ALIVE_TYPES_H_

#include <ostream>

enum class KeepAliveOrigin {
  // c/b/background
  BACKGROUND_MODE_MANAGER,

  // c/b/ui
  APP_LIST_SERVICE_VIEWS,
  APP_LIST_SHOWER,
  CHROME_APP_DELEGATE,
  PANEL_VIEW,
  PROFILE_LOADER,
  USER_MANAGER_VIEW
};

#ifndef NDEBUG
std::ostream& operator<<(std::ostream& out, const KeepAliveOrigin& origin);
#endif  // ndef NDEBUG

#endif  // CHROME_BROWSER_LIFETIME_KEEP_ALIVE_TYPES_H_
