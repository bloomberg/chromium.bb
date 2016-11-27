// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSION_SELECTOR_ROW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSION_SELECTOR_ROW_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/website_settings/permission_selector_row_observer.h"
#include "chrome/browser/ui/website_settings/permission_menu_model.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/view.h"

class Profile;

namespace internal {
class ComboboxModelAdapter;
class PermissionCombobox;
class PermissionMenuButton;
}

namespace views {
class GridLayout;
class ImageView;
}

// A PermissionSelectorRow is a row in the Page Info bubble that shows a
// permission that a site can have ambient access to, and allows the user to
// control whether that access is granted.
class PermissionSelectorRow : public views::View {
 public:
  PermissionSelectorRow(Profile* profile,
                        const GURL& url,
                        const WebsiteSettingsUI::PermissionInfo& permission);

  void AddObserver(PermissionSelectorRowObserver* observer);

  void PermissionChanged(const WebsiteSettingsUI::PermissionInfo& permission);

 protected:
  // Overridden from views::View.
  void ChildPreferredSizeChanged(View* child) override;

 private:
  ~PermissionSelectorRow() override;

  void InitializeMenuButtonView(
      views::GridLayout* layout,
      const WebsiteSettingsUI::PermissionInfo& permission);
  void InitializeComboboxView(
      views::GridLayout* layout,
      const WebsiteSettingsUI::PermissionInfo& permission);

  Profile* profile_;

  // Model for the permission's menu.
  std::unique_ptr<PermissionMenuModel> menu_model_;
  std::unique_ptr<internal::ComboboxModelAdapter> combobox_model_adapter_;

  views::ImageView* icon_;  // Owned by the views hierachy.
  internal::PermissionMenuButton* menu_button_;  // Owned by the views hierachy.
  internal::PermissionCombobox* combobox_;  // Owned by the views hierarchy.

  base::ObserverList<PermissionSelectorRowObserver, false> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(PermissionSelectorRow);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSION_SELECTOR_ROW_H_
