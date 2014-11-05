// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service_utils.h"

const int gMaxPersistNavigationCount = 6;

SessionWindow::WindowType WindowTypeForBrowserType(Browser::Type type) {
  switch (type) {
    case Browser::TYPE_POPUP:
      return SessionWindow::TYPE_POPUP;
    case Browser::TYPE_TABBED:
      return SessionWindow::TYPE_TABBED;
  }
  NOTREACHED();
  return SessionWindow::TYPE_TABBED;
}

Browser::Type BrowserTypeForWindowType(SessionWindow::WindowType type) {
  switch (type) {
    case SessionWindow::TYPE_POPUP:
      return Browser::TYPE_POPUP;
    case SessionWindow::TYPE_TABBED:
      return Browser::TYPE_TABBED;
  }
  NOTREACHED();
  return Browser::TYPE_TABBED;
}
