// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_CRASHES_UI_H_
#define CHROME_BROWSER_WEBUI_CRASHES_UI_H_
#pragma once

#include "chrome/browser/webui/web_ui.h"

class RefCountedMemory;

class CrashesUI : public WebUI {
 public:
  explicit CrashesUI(TabContents* contents);

  static RefCountedMemory* GetFaviconResourceBytes();

 private:
  DISALLOW_COPY_AND_ASSIGN(CrashesUI);
};

#endif  // CHROME_BROWSER_WEBUI_CRASHES_UI_H_
