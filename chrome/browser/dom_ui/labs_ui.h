// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_LABS_UI_H_
#define CHROME_BROWSER_DOM_UI_LABS_UI_H_
#pragma once

#include "chrome/browser/dom_ui/dom_ui.h"

class PrefService;
class RefCountedMemory;

class LabsUI : public DOMUI {
 public:
  explicit LabsUI(TabContents* contents);

  static RefCountedMemory* GetFaviconResourceBytes();
  static void RegisterUserPrefs(PrefService* prefs);

 private:
  DISALLOW_COPY_AND_ASSIGN(LabsUI);
};

#endif  // CHROME_BROWSER_DOM_UI_LABS_UI_H_
