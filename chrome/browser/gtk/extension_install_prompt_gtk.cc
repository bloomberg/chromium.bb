// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"

class Profile;

namespace {

const int kRightColumnWidth = 290;

// Left or right margin.
const int kPanelHorizMargin = 13;

GtkWidget* MakeMarkupLabel(const char* format, const std::string& str) {
  GtkWidget* label = gtk_label_new(NULL);
  char* markup = g_markup_printf_escaped(format, str.c_str());
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);

  // Left align it.
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

  return label;
}

void OnDialogResponse(GtkDialog* dialog, int response_id,
                      ExtensionInstallUI::Delegate* delegate) {
  if (response_id == GTK_RESPONSE_ACCEPT) {
    delegate->InstallUIProceed();
  } else {
    delegate->InstallUIAbort();
  }

  gtk_widget_destroy(GTK_WIDGET(dialog));
}

void ShowInstallPromptDialog(GtkWindow* parent, SkBitmap* skia_icon,
                             Extension *extension,
                             ExtensionInstallUI::Delegate *delegate,
                             const string16& warning_text,
                             bool is_uninstall) {
  // Build the dialog.
  int title_id = is_uninstall ? IDS_EXTENSION_UNINSTALL_PROMPT_TITLE :
                                IDS_EXTENSION_INSTALL_PROMPT_TITLE;
  int button_id = is_uninstall ? IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON :
                                 IDS_EXTENSION_PROMPT_INSTALL_BUTTON;
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
  gtk_box_pack_start(GTK_BOX(icon_hbox), icon, TRUE, TRUE, 0);

  // Create a new vbox for the right column.
  GtkWidget* right_column_area = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(icon_hbox), right_column_area, TRUE, TRUE, 0);

  int heading_id = is_uninstall ? IDS_EXTENSION_UNINSTALL_PROMPT_HEADING :
                                  IDS_EXTENSION_INSTALL_PROMPT_HEADING;
  std::string heading_text = WideToUTF8(l10n_util::GetStringF(
      heading_id, UTF8ToWide(extension->name())));
  GtkWidget* heading_label = MakeMarkupLabel("<span weight=\"bold\">%s</span>",
                                             heading_text);
  gtk_misc_set_alignment(GTK_MISC(heading_label), 0.0, 0.5);
  gtk_label_set_selectable(GTK_LABEL(heading_label), TRUE);
  gtk_box_pack_start(GTK_BOX(right_column_area), heading_label, TRUE, TRUE, 0);

  GtkWidget* warning_label = gtk_label_new(UTF16ToUTF8(warning_text).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(warning_label), TRUE);
  gtk_widget_set_size_request(warning_label, kRightColumnWidth, -1);
  gtk_misc_set_alignment(GTK_MISC(warning_label), 0.0, 0.5);
  gtk_label_set_selectable(GTK_LABEL(warning_label), TRUE);
  gtk_box_pack_start(GTK_BOX(right_column_area), warning_label, TRUE, TRUE, 0);

  g_signal_connect(dialog, "response", G_CALLBACK(OnDialogResponse), delegate);
  gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
  gtk_widget_show_all(dialog);
}

}  // namespace

void ExtensionInstallUI::ShowExtensionInstallUIPromptImpl(
    Profile* profile, Delegate* delegate, Extension* extension, SkBitmap* icon,
    const string16& warning_text, bool is_uninstall) {
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

  ShowInstallPromptDialog(browser_window->window(), icon, extension,
      delegate, warning_text, is_uninstall);
}
