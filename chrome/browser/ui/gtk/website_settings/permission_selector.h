// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_WEBSITE_SETTINGS_PERMISSION_SELECTOR_H_
#define CHROME_BROWSER_UI_GTK_WEBSITE_SETTINGS_PERMISSION_SELECTOR_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/gtk/website_settings/permission_selector_observer.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "ui/base/gtk/gtk_signal.h"

class GtkThemeService;
typedef struct _GParamSpec GParamSpec;
typedef struct _GtkWidget GtkWidget;

// The class |PermissionSelector| allows to change the permission |setting| of a
// given permission |type|.
class PermissionSelector {
 public:
  // Creates a |PermissionSelector| for the given permission |type|. |setting|
  // is the current permissions setting. It is possible to pass
  // |CONTENT_SETTING_DEFAULT| as value for |setting|. |default_setting| is the
  // effective default setting for the given permission |type|. It is not
  // allowed to pass |CONTENT_SETTING_DEFAULT| as value for |default_setting|.
  PermissionSelector(GtkThemeService* theme_service_,
                     ContentSettingsType type,
                     ContentSetting setting,
                     ContentSetting default_setting);
  ~PermissionSelector();

  // Creates the UI for the |PermissionSelector| and returns the widget that
  // contains the UI elements. The ownership of the widget is transfered to the
  // caller.
  GtkWidget* CreateUI();

  // Adds an |observer|.
  void AddObserver(PermissionSelectorObserver* observer);

  // Returns the current permission setting.
  ContentSetting setting() const { return setting_; }

  // Returns the permissions type.
  ContentSettingsType type() const { return type_; }

 private:
  CHROMEGTK_CALLBACK_0(PermissionSelector, void, OnPermissionChanged);
  CHROMEGTK_CALLBACK_1(PermissionSelector, void, OnComboBoxShown, GParamSpec*);

  GtkThemeService* theme_service_;

  // The permission type.
  ContentSettingsType type_;
  // The current permission setting.
  ContentSetting setting_;
  // The effective default setting. This is required to create the proper UI
  // string for the default setting.
  ContentSetting default_setting_;

  // The permission type icon. The icon is changed depending on the selected
  // setting.
  // Owned by the widget hierarchy created by the CreateUI method.
  GtkWidget* icon_;

  ObserverList<PermissionSelectorObserver, false> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(PermissionSelector);
};

#endif  // CHROME_BROWSER_UI_GTK_WEBSITE_SETTINGS_PERMISSION_SELECTOR_H_
