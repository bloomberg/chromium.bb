// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_INSTANT_PREVIEW_CONTROLLER_GTK_H_
#define CHROME_BROWSER_UI_GTK_INSTANT_PREVIEW_CONTROLLER_GTK_H_

#include "base/compiler_specific.h"
#include "chrome/browser/instant/instant_model_observer.h"
#include "chrome/browser/instant/instant_preview_controller.h"

class Browser;
class BrowserWindowGtk;
class InstantModel;
class TabContents;
class TabContentsContainerGtk;

class InstantPreviewControllerGtk : public InstantPreviewController {
 public:
  InstantPreviewControllerGtk(Browser* browser,
                              BrowserWindowGtk* window,
                              TabContentsContainerGtk* contents);
  virtual ~InstantPreviewControllerGtk();

  // InstantModelObserver overrides:
  virtual void DisplayStateChanged(const InstantModel& model) OVERRIDE;

 private:
  void ShowInstant(TabContents* preview, int height, InstantSizeUnits units);
  void HideInstant();

  // Weak.
  BrowserWindowGtk* window_;

  // Weak.
  TabContentsContainerGtk* contents_;

  DISALLOW_COPY_AND_ASSIGN(InstantPreviewControllerGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_INSTANT_PREVIEW_CONTROLLER_GTK_H_
