// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOUCH_TABS_TOUCH_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOUCH_TABS_TOUCH_TAB_STRIP_CONTROLLER_H_
#pragma once

#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"

class TouchTab;
class TouchTabStrip;

class TouchTabStripController : public BrowserTabStripController {
 public:
  TouchTabStripController(Browser* browser, TabStripModel* model);
  virtual ~TouchTabStripController();

  virtual void TabDetachedAt(TabContentsWrapper* contents,
                             int model_index) OVERRIDE;

  virtual void TabChangedAt(TabContentsWrapper* contents,
                            int model_index,
                            TabChangeType change_type) OVERRIDE;

 protected:
  virtual void SetTabRendererDataFromModel(TabContents* contents,
                                           int model_index,
                                           TabRendererData* data,
                                           TabStatus tab_status) OVERRIDE;

  const TouchTabStrip* tabstrip() const;

 private:
  void OnTouchIconAvailable(FaviconService::Handle h,
                            history::FaviconData favicon);

  // Our consumer for touch icon requests.
  CancelableRequestConsumerTSimple<TouchTab*> consumer_;

  DISALLOW_COPY_AND_ASSIGN(TouchTabStripController);
};

#endif  // CHROME_BROWSER_UI_TOUCH_TABS_TOUCH_TAB_STRIP_CONTROLLER_H_
