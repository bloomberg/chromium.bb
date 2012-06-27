// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSION_SELECTOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSION_SELECTOR_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/string16.h"
#include "chrome/browser/ui/views/website_settings/permission_selector_view_observer.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/view.h"

namespace internal {
class PermissionMenuModel;
class PermissionMenuButton;
}

namespace views {
class ImageView;
class MenuRunner;
}

// A custom view for selecting a permission setting for the given permission
// |type|.
class PermissionSelectorView : public views::View {
 public:
  PermissionSelectorView(ContentSettingsType type,
                         ContentSetting default_setting,
                         ContentSetting current_setting);

  void AddObserver(PermissionSelectorViewObserver* observer);

  // This method is called by the menu button model whenever the current setting
  // is changed.
  void SelectionChanged();

  // Returns the selected setting.
  ContentSetting GetSelectedSetting() const;

  // Returns the permission type.
  ContentSettingsType GetPermissionType() const;

 private:
  virtual ~PermissionSelectorView();

  views::ImageView* icon_;  // Owned by the views hierachy.
  internal::PermissionMenuButton* menu_button_;  // Owned by the views hierachy.
  scoped_ptr<internal::PermissionMenuModel> menu_button_model_;

  ObserverList<PermissionSelectorViewObserver, false> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(PermissionSelectorView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_PERMISSION_SELECTOR_VIEW_H_
