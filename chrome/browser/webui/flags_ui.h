// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_FLAGS_UI_H_
#define CHROME_BROWSER_WEBUI_FLAGS_UI_H_
#pragma once

#include "chrome/browser/dom_ui/web_ui.h"

class PrefService;
class RefCountedMemory;

class FlagsUI : public WebUI {
 public:
  explicit FlagsUI(TabContents* contents);

  static RefCountedMemory* GetFaviconResourceBytes();
  static void RegisterPrefs(PrefService* prefs);

 private:
  DISALLOW_COPY_AND_ASSIGN(FlagsUI);
};

#endif  // CHROME_BROWSER_WEBUI_FLAGS_UI_H_
