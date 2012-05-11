// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_TAB_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_TAB_H_
#pragma once

#include <string>
#include <vector>

#include "googleurl/src/gurl.h"

// Represents tab data at startup.
struct StartupTab {
  StartupTab();
  ~StartupTab();

  // The url to load.
  GURL url;

  // If true, the tab corresponds to an app an |app_id| gives the id of the
  // app.
  bool is_app;

  // True if the is tab pinned.
  bool is_pinned;

  // Id of the app.
  std::string app_id;
};

typedef std::vector<StartupTab> StartupTabs;

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_TAB_H_
