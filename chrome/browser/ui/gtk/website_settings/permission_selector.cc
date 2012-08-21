// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/website_settings/permission_selector.h"

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

namespace {

GtkWidget* CreateTextLabel(const std::string& text,
                           GtkThemeService* theme_service) {
  GtkWidget* label = theme_service->BuildLabel(text, ui::kGdkBlack);
  gtk_label_set_selectable(GTK_LABEL(label), TRUE);
  gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
  return label;
}

}  // namespace

PermissionSelector::PermissionSelector(GtkThemeService* theme_service,
                                       ContentSettingsType type,
                                       ContentSetting setting,
                                       ContentSetting default_setting)
    : theme_service_(theme_service),
      type_(type),
      setting_(setting),
      default_setting_(default_setting),
      icon_(NULL) {
  DCHECK_NE(default_setting, CONTENT_SETTING_DEFAULT);
}

PermissionSelector::~PermissionSelector() {
}

GtkWidget* PermissionSelector::CreateUI() {
  // Create permission info box.
  const int kChildSpacing = 4;
  GtkWidget* hbox = gtk_hbox_new(FALSE, kChildSpacing);

  // Add permission type icon.
  GdkPixbuf* pixbuf = WebsiteSettingsUI::GetPermissionIcon(
      type_, setting_).ToGdkPixbuf();
  icon_ = gtk_image_new_from_pixbuf(pixbuf);
  gtk_box_pack_start(GTK_BOX(hbox), icon_, FALSE, FALSE, 0);

  // Add a label for the permission type.
  GtkWidget* label = CreateTextLabel(
      l10n_util::GetStringFUTF8(
          IDS_WEBSITE_SETTINGS_PERMISSION_TYPE,
          WebsiteSettingsUI::PermissionTypeToUIString(type_)),
      theme_service_);
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

  GtkListStore* store =
      gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
  GtkTreeIter iter;
  // Add option for permission "Global Default" to the combobox model.
  std::string setting_str = l10n_util::GetStringFUTF8(
      IDS_WEBSITE_SETTINGS_DEFAULT_PERMISSION_LABEL,
      WebsiteSettingsUI::PermissionValueToUIString(default_setting_));
  gtk_list_store_append(store, &iter);
  gtk_list_store_set(store, &iter, 0, setting_str.c_str(), 1,
                     CONTENT_SETTING_DEFAULT, -1);
  GtkWidget* combo_box = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
  // Add option for permission "Allow" to the combobox model.
  setting_str = l10n_util::GetStringFUTF8(
      IDS_WEBSITE_SETTINGS_PERMISSION_LABEL,
      WebsiteSettingsUI::PermissionValueToUIString(CONTENT_SETTING_ALLOW));
  gtk_list_store_append(store, &iter);
  gtk_list_store_set(store, &iter, 0, setting_str.c_str(), 1,
                     CONTENT_SETTING_ALLOW, -1);
  // The content settings type fullscreen does not support the concept of
  // blocking.
  if (type_ != CONTENT_SETTINGS_TYPE_FULLSCREEN) {
    // Add option for permission "BLOCK" to the combobox model.
    setting_str = l10n_util::GetStringFUTF8(
        IDS_WEBSITE_SETTINGS_PERMISSION_LABEL,
        WebsiteSettingsUI::PermissionValueToUIString(CONTENT_SETTING_BLOCK));
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, setting_str.c_str(), 1,
                       CONTENT_SETTING_BLOCK, -1);
  }
  // Remove reference to the store to prevent leaking.
  g_object_unref(G_OBJECT(store));

  GtkCellRenderer* cell = gtk_cell_renderer_text_new();
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo_box), cell, TRUE );
  gtk_cell_layout_set_attributes(
      GTK_CELL_LAYOUT(combo_box), cell, "text", 0, NULL);

  // Select the combobox entry for the currently configured permission value.
  int active = -1;
  switch (setting_) {
    case CONTENT_SETTING_DEFAULT: active = 0;
      break;
    case CONTENT_SETTING_ALLOW: active = 1;
      break;
    case CONTENT_SETTING_BLOCK: active = 2;
      break;
    default:
      NOTREACHED() << "Bad content setting:" << setting_;
      break;
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), active);

  // Add change listener to the combobox.
  g_signal_connect(combo_box, "changed",
                   G_CALLBACK(OnPermissionChangedThunk), this);
  // Once the popup (window) for a combobox is shown, the bubble container
  // (window) loses it's focus. Therefore it necessary to reset the focus to
  // the bubble container after the combobox popup is closed.
  g_signal_connect(combo_box, "notify::popup-shown",
                   G_CALLBACK(OnComboBoxShownThunk), this);
  gtk_box_pack_start(GTK_BOX(hbox), combo_box, FALSE, FALSE, 0);

  return hbox;
}

void PermissionSelector::AddObserver(PermissionSelectorObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PermissionSelector::OnPermissionChanged(GtkWidget* widget) {
  // Get the selected setting.
  GtkTreeIter it;
  bool has_active = gtk_combo_box_get_active_iter(GTK_COMBO_BOX(widget), &it);
  DCHECK(has_active);
  GtkTreeModel* store =
      GTK_TREE_MODEL(gtk_combo_box_get_model(GTK_COMBO_BOX(widget)));
  int value = -1;
  gtk_tree_model_get(store, &it, 1, &value, -1);
  setting_ = ContentSetting(value);

  // Change the permission icon according the the selected setting.
  GdkPixbuf* pixbuf = WebsiteSettingsUI::GetPermissionIcon(
      type_, setting_).ToGdkPixbuf();
  gtk_image_set_from_pixbuf(GTK_IMAGE(icon_), pixbuf);

  FOR_EACH_OBSERVER(PermissionSelectorObserver,
                    observer_list_,
                    OnPermissionChanged(this));
}

void PermissionSelector::OnComboBoxShown(GtkWidget* widget,
                                         GParamSpec* property) {
  // GtkComboBox grabs the keyboard and pointer when it displays its popup,
  // which steals the grabs that BubbleGtk had installed. When the popup is
  // hidden, we notify BubbleGtk so it can try to reacquire the grabs
  // (otherwise, GTK won't activate our widgets when the user clicks in them).
  // When then combobox popup is closed we notify the
  // |PermissionSelectorObserver|s so that BubbleGtk can grab the keyboard and
  // pointer.
  gboolean popup_shown = FALSE;
  g_object_get(G_OBJECT(GTK_COMBO_BOX(widget)), "popup-shown", &popup_shown,
               NULL);
  if (!popup_shown) {
    FOR_EACH_OBSERVER(PermissionSelectorObserver,
                      observer_list_,
                      OnComboboxShown());
  }
}
