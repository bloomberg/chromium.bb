// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_BROWSER_COMMAND_ITEM_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_BROWSER_COMMAND_ITEM_H_
#pragma once

#include "chrome/browser/ui/views/aura/app_list/chrome_app_list_item.h"

class Browser;

class BrowserCommandItem : public ChromeAppListItem {
 public:
  BrowserCommandItem(Browser* browser,
                     int command_id,
                     int title_id,
                     int icon_id);
  virtual ~BrowserCommandItem();

 private:
  // Overridden from ChromeAppListItem:
  virtual void Activate(int event_flags) OVERRIDE;

  Browser* browser_;
  int command_id_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCommandItem);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_BROWSER_COMMAND_ITEM_H_
