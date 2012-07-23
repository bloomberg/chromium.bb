// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HUNG_RENDERER_VIEW_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_HUNG_RENDERER_VIEW_WIN_H_

#include "chrome/browser/ui/views/hung_renderer_view.h"

class HungRendererDialogViewWin : public HungRendererDialogView {
 public:
  HungRendererDialogViewWin() {}
  virtual ~HungRendererDialogViewWin() {}

  // HungRendererDialogView overrides.
  virtual void ShowForWebContents(WebContents* contents) OVERRIDE;
  virtual void EndForWebContents(WebContents* contents) OVERRIDE;
};

#endif  // CHROME_BROWSER_UI_VIEWS_HUNG_RENDERER_VIEW_WIN_H_

