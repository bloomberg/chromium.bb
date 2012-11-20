// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_INSTANT_PREVIEW_CONTROLLER_GTK_H_
#define CHROME_BROWSER_UI_GTK_INSTANT_PREVIEW_CONTROLLER_GTK_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/instant/instant_preview_controller.h"

class BrowserWindowGtk;
class TabContentsContainerGtk;

class InstantPreviewControllerGtk : public InstantPreviewController {
 public:
  InstantPreviewControllerGtk(BrowserWindowGtk* window,
                              TabContentsContainerGtk* contents);
  virtual ~InstantPreviewControllerGtk();

 private:
  // Overridden from InstantPreviewController:
  virtual void PreviewStateChanged(const InstantModel& model) OVERRIDE;

  BrowserWindowGtk* const window_;
  TabContentsContainerGtk* const contents_;

  DISALLOW_COPY_AND_ASSIGN(InstantPreviewControllerGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_INSTANT_PREVIEW_CONTROLLER_GTK_H_
