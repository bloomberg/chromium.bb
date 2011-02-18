// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_REMOTING_UI_H_
#define CHROME_BROWSER_WEBUI_REMOTING_UI_H_
#pragma once

#include "chrome/browser/webui/web_ui.h"

class PrefService;
class RefCountedMemory;

class RemotingUI : public WebUI {
 public:
  explicit RemotingUI(TabContents* contents);

  static RefCountedMemory* GetFaviconResourceBytes();
  static void RegisterUserPrefs(PrefService* prefs);

 private:
  DISALLOW_COPY_AND_ASSIGN(RemotingUI);
};

#endif  // CHROME_BROWSER_WEBUI_REMOTING_UI_H_
