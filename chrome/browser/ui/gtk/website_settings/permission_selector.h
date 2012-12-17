// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_WEBSITE_SETTINGS_PERMISSION_SELECTOR_H_
#define CHROME_BROWSER_UI_GTK_WEBSITE_SETTINGS_PERMISSION_SELECTOR_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/website_settings/permission_menu_model.h"
#include "chrome/browser/ui/gtk/website_settings/permission_selector_observer.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "ui/base/gtk/gtk_signal.h"

class MenuGtk;
class GtkThemeService;

// The class |PermissionSelector| allows to change the permission |setting| of a
// given permission |type|.
class PermissionSelector : public PermissionMenuModel::Delegate {
 public:
  // Creates a |PermissionSelector| for the given permission |type|. |setting|
  // is the current permissions setting. It is possible to pass
  // |CONTENT_SETTING_DEFAULT| as value for |setting|. |default_setting| is the
  // effective default setting for the given permission |type|. It is not
  // allowed to pass |CONTENT_SETTING_DEFAULT| as value for |default_setting|.
  PermissionSelector(GtkThemeService* theme_service,
                     const GURL& url,
                     ContentSettingsType type,
                     ContentSetting setting,
                     ContentSetting default_setting,
                     content_settings::SettingSource source);
  virtual ~PermissionSelector();

  // Returns the container widget that contains all |PermissionSelector| UI
  // widgets.
  GtkWidget* widget() { return widget_; }

  // Adds an |observer|.
  void AddObserver(PermissionSelectorObserver* observer);

  // Returns the current permission setting.
  ContentSetting GetSetting() const;

  // Returns the permissions type.
  ContentSettingsType GetType() const;

 private:
  // Gtk callback to intercept mouse clicks to the menu buttons.
  CHROMEGTK_CALLBACK_1(PermissionSelector, gboolean, OnMenuButtonPressEvent,
                       GdkEventButton*);

  // PermissionMenuModel::Delegate implementation.
  virtual void ExecuteCommand(int command_id) OVERRIDE;
  virtual bool IsCommandIdChecked(int command_id) OVERRIDE;

  // The container widget that contains the PermissionSelecter UI widgets.
  GtkWidget* widget_;  // Owned by the widget hierarchy.

  // The button that toggles the permission |menu_|.
  GtkWidget* menu_button_;  // Owned by the widget hierarchy.

  // The menu that provides the permission settings.
  scoped_ptr<MenuGtk> menu_;

  // The model for the permission |menu_|.
  scoped_ptr<PermissionMenuModel> menu_model_;

  // The permission type icon. The icon is changed depending on the selected
  // setting.
  // Owned by the widget hierarchy created by the CreateUI method.
  GtkWidget* icon_;

  // The site permission (the |ContentSettingsType|) for which the menu model
  // provides settings.
  ContentSettingsType type_;

  // The global default setting for the permission |type_|.
  ContentSetting default_setting_;

  // The currently active setting for the permission |type_|.
  ContentSetting setting_;

  ObserverList<PermissionSelectorObserver, false> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(PermissionSelector);
};

#endif  // CHROME_BROWSER_UI_GTK_WEBSITE_SETTINGS_PERMISSION_SELECTOR_H_
