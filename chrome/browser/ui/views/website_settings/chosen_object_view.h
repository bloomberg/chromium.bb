// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_CHOSEN_OBJECT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_CHOSEN_OBJECT_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class ImageView;
}

class ChosenObjectViewObserver;

class ChosenObjectView : public views::View, public views::ButtonListener {
 public:
  explicit ChosenObjectView(
      std::unique_ptr<WebsiteSettingsUI::ChosenObjectInfo> info);

  void AddObserver(ChosenObjectViewObserver* observer);

 private:
  ~ChosenObjectView() override;

  // views::ButtonListener implementation.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  views::ImageView* icon_;             // Owned by the views hierarchy.
  views::ImageButton* delete_button_;  // Owned by the views hierarchy.

  base::ObserverList<ChosenObjectViewObserver> observer_list_;
  std::unique_ptr<WebsiteSettingsUI::ChosenObjectInfo> info_;

  DISALLOW_COPY_AND_ASSIGN(ChosenObjectView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_CHOSEN_OBJECT_VIEW_H_
