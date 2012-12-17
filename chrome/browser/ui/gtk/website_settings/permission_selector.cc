// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/website_settings/permission_selector.h"

#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace {

ContentSetting CommandIdToContentSetting(int command_id) {
  switch (command_id) {
    case PermissionMenuModel::COMMAND_SET_TO_DEFAULT:
      return CONTENT_SETTING_DEFAULT;
    case PermissionMenuModel::COMMAND_SET_TO_ALLOW:
      return CONTENT_SETTING_ALLOW;
    case PermissionMenuModel::COMMAND_SET_TO_BLOCK:
      return CONTENT_SETTING_BLOCK;
    default:
      NOTREACHED();
      return CONTENT_SETTING_DEFAULT;
  }
}

}  // namespace

PermissionSelector::PermissionSelector(GtkThemeService* theme_service,
                                       const GURL& url,
                                       ContentSettingsType type,
                                       ContentSetting setting,
                                       ContentSetting default_setting,
                                       content_settings::SettingSource source)
    : widget_(NULL),
      menu_button_(NULL),
      icon_(NULL),
      type_(type),
      default_setting_(default_setting),
      setting_(setting) {
  DCHECK_NE(default_setting, CONTENT_SETTING_DEFAULT);

  // Create permission info box.
  const int kChildSpacing = 4;
  widget_ = gtk_hbox_new(FALSE, kChildSpacing);

  // Add permission type icon.
  ContentSetting effective_setting = setting;
  if (effective_setting == CONTENT_SETTING_DEFAULT)
    effective_setting = default_setting;
  GdkPixbuf* pixbuf = WebsiteSettingsUI::GetPermissionIcon(
      type, effective_setting).ToGdkPixbuf();
  icon_ = gtk_image_new_from_pixbuf(pixbuf);
  gtk_box_pack_start(GTK_BOX(widget_), icon_, FALSE, FALSE, 0);

  // Add a label for the permission type.
  GtkWidget* label = theme_service->BuildLabel(l10n_util::GetStringFUTF8(
      IDS_WEBSITE_SETTINGS_PERMISSION_TYPE,
      WebsiteSettingsUI::PermissionTypeToUIString(type)),
      ui::kGdkBlack);
  gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);

  gtk_box_pack_start(GTK_BOX(widget_), label, FALSE, FALSE, 0);

  // Add the menu button.
  menu_button_ = theme_service->BuildChromeButton();
  GtkWidget* button_hbox = gtk_hbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(menu_button_), button_hbox);

  GtkWidget* button_label = theme_service->BuildLabel(
      UTF16ToUTF8(WebsiteSettingsUI::PermissionActionToUIString(
          setting, default_setting, source)),
      ui::kGdkBlack);
  gtk_box_pack_start(GTK_BOX(button_hbox), button_label, FALSE, FALSE,
                     ui::kControlSpacing);

  bool user_setting = source == content_settings::SETTING_SOURCE_USER;
  gtk_widget_set_sensitive(GTK_WIDGET(menu_button_), user_setting);
  if (user_setting) {
    GtkWidget* arrow = NULL;
    // We don't handle theme changes, which is a bug but they are very unlikely
    // to occur while a bubble is grabbing input.
    if (theme_service->UsingNativeTheme()) {
      arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE);
    } else {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      arrow = gtk_image_new_from_pixbuf(
          rb.GetNativeImageNamed(IDR_APP_DROPARROW).ToGdkPixbuf());
    }
    gtk_box_pack_start(GTK_BOX(button_hbox), arrow, FALSE, FALSE, 0);
  }
  gtk_button_set_relief(GTK_BUTTON(menu_button_), GTK_RELIEF_NONE);
  gtk_box_pack_start(GTK_BOX(widget_), menu_button_, FALSE, FALSE, 0);

  menu_model_.reset(new PermissionMenuModel(this, url, type, default_setting,
                                            setting));
  MenuGtk::Delegate* delegate = new MenuGtk::Delegate();
  menu_.reset(new MenuGtk(delegate, menu_model_.get()));
  g_signal_connect(menu_button_, "button-press-event",
                   G_CALLBACK(OnMenuButtonPressEventThunk), this);
}

PermissionSelector::~PermissionSelector() {
}

void PermissionSelector::AddObserver(PermissionSelectorObserver* observer) {
  observer_list_.AddObserver(observer);
}

ContentSetting PermissionSelector::GetSetting() const {
  return setting_;
}

ContentSettingsType PermissionSelector::GetType() const {
  return type_;
}

gboolean PermissionSelector::OnMenuButtonPressEvent(GtkWidget* button,
                                                    GdkEventButton* event) {
  if (event->button != 1)
    return FALSE;
  menu_->PopupForWidget(button, event->button, event->time);
  return TRUE;
}

void PermissionSelector::ExecuteCommand(int command_id) {
  setting_ = CommandIdToContentSetting(command_id);

  // Change the permission icon to reflect the selected setting.
  ContentSetting effective_setting = setting_;
  if (effective_setting == CONTENT_SETTING_DEFAULT)
    effective_setting = default_setting_;
  GdkPixbuf* pixbuf = WebsiteSettingsUI::GetPermissionIcon(
      type_, effective_setting).ToGdkPixbuf();
  gtk_image_set_from_pixbuf(GTK_IMAGE(icon_), pixbuf);

  // Change the text of the menu button to reflect the selected setting.
  gtk_button_set_label(GTK_BUTTON(menu_button_), UTF16ToUTF8(
      WebsiteSettingsUI::PermissionActionToUIString(
          setting_,
          default_setting_,
          content_settings::SETTING_SOURCE_USER)).c_str());

  FOR_EACH_OBSERVER(PermissionSelectorObserver,
                    observer_list_,
                    OnPermissionChanged(this));
}

bool PermissionSelector::IsCommandIdChecked(int command_id) {
  return setting_ == CommandIdToContentSetting(command_id);
}
