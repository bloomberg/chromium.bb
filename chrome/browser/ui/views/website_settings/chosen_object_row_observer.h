// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_CHOSEN_OBJECT_ROW_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_CHOSEN_OBJECT_ROW_OBSERVER_H_

#include "chrome/browser/ui/website_settings/website_settings_ui.h"

class ChosenObjectRowObserver {
 public:
  // This method is called when permission for the object is revoked.
  virtual void OnChosenObjectDeleted(
      const WebsiteSettingsUI::ChosenObjectInfo& info) = 0;

 protected:
  virtual ~ChosenObjectRowObserver() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_CHOSEN_OBJECT_ROW_OBSERVER_H_
