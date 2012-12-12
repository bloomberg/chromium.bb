// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HUNG_RENDERER_VIEW_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_HUNG_RENDERER_VIEW_WIN_H_

#include "base/basictypes.h"
#include "content/public/browser/web_contents.h"

// This class provides functionality to display a Windows 8 metro style hung
// renderer dialog.
class HungRendererDialogMetro {
 public:
  // Creates or returns the global instance of the HungRendererDialogMetro
  // class
  static HungRendererDialogMetro* Create();
  static HungRendererDialogMetro* GetInstance();

  void Show(content::WebContents* contents);
  void Hide(content::WebContents* contents);

 private:
  HungRendererDialogMetro();
  ~HungRendererDialogMetro();

  // Handlers for the hang monitor dialog displayed in Windows 8 metro.
  static void OnMetroKillProcess();
  static void OnMetroWait();

  // Resets Windows 8 metro specific state like whether the dialog was
  // displayed, etc.
  void ResetMetroState();

  content::WebContents* contents_;

  // Set to true if the metro version of the hang dialog is displayed.
  // Helps ensure that only one instance of the dialog is displayed at any
  // given time.
  bool metro_dialog_displayed_;

  // Pointer to the global instance of the HungRendererDialogMetro class.
  static HungRendererDialogMetro* g_instance_;

  DISALLOW_COPY_AND_ASSIGN(HungRendererDialogMetro);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HUNG_RENDERER_VIEW_WIN_H_

