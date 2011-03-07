// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Currently this file is only used for the uninstall prompt. The install prompt
// code is in extension_install_prompt2_gtk.cc.

#include <gtk/gtk.h>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/gtk_util.h"

class Profile;

namespace {

// Left or right margin.
const int kPanelHorizMargin = 13;

void OnResponse(GtkWidget* dialog, int response_id,
                ExtensionInstallUI::Delegate* delegate) {
  if (response_id == GTK_RESPONSE_ACCEPT) {
    delegate->InstallUIProceed();
  } else {
    delegate->InstallUIAbort();
  }

  gtk_widget_destroy(dialog);
}

void ShowInstallPromptDialog(GtkWindow* parent, SkBitmap* skia_icon,
                             const Extension* extension,
                             ExtensionInstallUI::Delegate *delegate,
                             ExtensionInstallUI::PromptType type) {
  // Build the dialog.
  int title_id = ExtensionInstallUI::kTitleIds[type];
  int button_id = ExtensionInstallUI::kButtonIds[type];
  GtkWidget* dialog = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(title_id).c_str(),
      parent,
      GTK_DIALOG_MODAL,
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_CLOSE,
      l10n_util::GetStringUTF8(button_id).c_str(),
      GTK_RESPONSE_ACCEPT,
      NULL);
  gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

  // Create a two column layout.
  GtkWidget* content_area = GTK_DIALOG(dialog)->vbox;
  gtk_box_set_spacing(GTK_BOX(content_area), gtk_util::kContentAreaSpacing);

  GtkWidget* icon_hbox = gtk_hbox_new(FALSE, gtk_util::kContentAreaSpacing);
  gtk_box_pack_start(GTK_BOX(content_area), icon_hbox, TRUE, TRUE, 0);

  // Put Icon in the left column.
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(skia_icon);
  GtkWidget* icon = gtk_image_new_from_pixbuf(pixbuf);
  g_object_unref(pixbuf);
  gtk_box_pack_start(GTK_BOX(icon_hbox), icon, TRUE, TRUE, 0);

  // Create a new vbox for the right column.
  GtkWidget* right_column_area = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(icon_hbox), right_column_area, TRUE, TRUE, 0);

  int heading_id = ExtensionInstallUI::kHeadingIds[type];
  std::string heading_text = l10n_util::GetStringFUTF8(
      heading_id, UTF8ToUTF16(extension->name()));
  GtkWidget* heading_label = gtk_label_new(heading_text.c_str());
  gtk_misc_set_alignment(GTK_MISC(heading_label), 0.0, 0.5);
  gtk_box_pack_start(GTK_BOX(right_column_area), heading_label, TRUE, TRUE, 0);

  g_signal_connect(dialog, "response", G_CALLBACK(OnResponse), delegate);
  gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
  gtk_widget_show_all(dialog);
}

}  // namespace

void ExtensionInstallUI::ShowExtensionInstallUIPromptImpl(
    Profile* profile,
    Delegate* delegate,
    const Extension* extension,
    SkBitmap* icon,
    ExtensionInstallUI::PromptType type) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  if (!browser) {
    delegate->InstallUIAbort();
    return;
  }

  BrowserWindowGtk* browser_window = static_cast<BrowserWindowGtk*>(
      browser->window());
  if (!browser_window) {
    delegate->InstallUIAbort();
    return;
  }

  ShowInstallPromptDialog(browser_window->window(), icon, extension, delegate,
                          type);
}
