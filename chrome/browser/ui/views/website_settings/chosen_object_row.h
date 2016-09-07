// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_CHOSEN_OBJECT_ROW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_CHOSEN_OBJECT_ROW_H_

#include "base/macros.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class ImageView;
}

class ChosenObjectRowObserver;

// A ChosenObjectRow is a row in the Page Info bubble that shows an individual
// object (e.g. a Bluetooth device, a USB device) that the current site has
// access to.
class ChosenObjectRow : public views::View, public views::ButtonListener {
 public:
  explicit ChosenObjectRow(
      std::unique_ptr<WebsiteSettingsUI::ChosenObjectInfo> info);

  void AddObserver(ChosenObjectRowObserver* observer);

 private:
  ~ChosenObjectRow() override;

  // views::ButtonListener implementation.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  views::ImageView* icon_;             // Owned by the views hierarchy.
  views::ImageButton* delete_button_;  // Owned by the views hierarchy.

  base::ObserverList<ChosenObjectRowObserver> observer_list_;
  std::unique_ptr<WebsiteSettingsUI::ChosenObjectInfo> info_;

  DISALLOW_COPY_AND_ASSIGN(ChosenObjectRow);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_CHOSEN_OBJECT_ROW_H_
