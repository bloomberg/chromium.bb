// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_CHROME_APP_LIST_ITEM_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_CHROME_APP_LIST_ITEM_H_
#pragma once

#include "ash/app_list/app_list_item_model.h"

// Base class of all chrome app list items. Chrome's AppListViewDelegate assumes
// all items are derived from this class and calls Activate when an item is
// activated.
class ChromeAppListItem : public ash::AppListItemModel {
 public:
  // Activates the item. |event_flags| holds flags of a mouse/keyboard event
  // associated with this activation.
  virtual void Activate(int event_flags) = 0;

 protected:
  virtual ~ChromeAppListItem() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_CHROME_APP_LIST_ITEM_H_
