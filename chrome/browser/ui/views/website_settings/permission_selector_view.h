// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSION_SELECTOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSION_SELECTOR_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/website_settings/permission_selector_view_observer.h"
#include "chrome/browser/ui/website_settings/permission_menu_model.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/view.h"

namespace internal {
class PermissionMenuButton;
}

namespace views {
class ImageView;
class MenuRunner;
}

// A custom view for selecting a permission setting for the given permission
// |type|.
class PermissionSelectorView : public views::View,
                               public PermissionMenuModel::Delegate {
 public:
  PermissionSelectorView(const GURL& url,
                         ContentSettingsType type,
                         ContentSetting default_setting,
                         ContentSetting current_setting,
                         content_settings::SettingSource source);

  void AddObserver(PermissionSelectorViewObserver* observer);

  // Returns the selected setting.
  ContentSetting current_setting() const { return current_setting_; }

  // Returns the permission type.
  ContentSettingsType type() const { return type_; }

 protected:
  // Overridden from views::View.
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;

 private:
  virtual ~PermissionSelectorView();

  // Overridden from PermissionMenuModel::Delegate.
  virtual void ExecuteCommand(int command_id) OVERRIDE;
  virtual bool IsCommandIdChecked(int command_id) OVERRIDE;

  views::ImageView* icon_;  // Owned by the views hierachy.
  internal::PermissionMenuButton* menu_button_;  // Owned by the views hierachy.
  scoped_ptr<PermissionMenuModel> menu_button_model_;

  // The site permission (the |ContentSettingsType|) for which the menu model
  // provides settings.
  ContentSettingsType type_;

  // The global default setting for the permission |type_|.
  ContentSetting default_setting_;

  // The currently active setting for the permission |type_|.
  ContentSetting current_setting_;

  ObserverList<PermissionSelectorViewObserver, false> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(PermissionSelectorView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSION_SELECTOR_VIEW_H_
