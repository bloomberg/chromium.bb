// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_WINDOW_PROPERTY_MANAGER_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_WINDOW_PROPERTY_MANAGER_WIN_H_

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"

class BrowserView;

// This class is resposible for updating the app id and relaunch details of a
// browser frame.
class BrowserWindowPropertyManager {
 public:
  virtual ~BrowserWindowPropertyManager();

  void UpdateWindowProperties(HWND hwnd);

  static scoped_ptr<BrowserWindowPropertyManager>
      CreateBrowserWindowPropertyManager(BrowserView* view);

 private:
  explicit BrowserWindowPropertyManager(BrowserView* view);

  void OnProfileIconVersionChange();

  PrefChangeRegistrar profile_pref_registrar_;

  BrowserView* view_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWindowPropertyManager);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_WINDOW_PROPERTY_MANAGER_WIN_H_
