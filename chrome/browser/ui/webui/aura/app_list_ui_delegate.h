// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AURA_APP_LIST_UI_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_AURA_APP_LIST_UI_DELEGATE_H_
#pragma once

class AppListUIDelegate {
 public:
  // Close AppListUI.
  virtual void Close() = 0;

  // Invoked when apps are loaded.
  virtual void OnAppsLoaded() = 0;
};

#endif  // CHROME_BROWSER_UI_WEBUI_AURA_APP_LIST_UI_DELEGATE_H_
