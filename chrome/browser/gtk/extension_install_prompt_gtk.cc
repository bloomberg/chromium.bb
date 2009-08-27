// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "base/gfx/gtk_util.h"
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
    delegate->ContinueInstall();
  } else {
    delegate->AbortInstall();
  }

  gtk_widget_destroy(GTK_WIDGET(dialog));
}

void ShowInstallPromptDialog(GtkWindow* parent, SkBitmap* skia_icon,
                             Extension *extension,
                             ExtensionInstallUI::Delegate *delegate) {
  // Build the dialog.
  GtkWidget* dialog = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_EXTENSION_PROMPT_TITLE).c_str(),
      parent,
      GTK_DIALOG_MODAL,
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_CLOSE,
      "Install",
      GTK_RESPONSE_ACCEPT,
      NULL);
  gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

  GtkWidget* content_area = GTK_DIALOG(dialog)->vbox;
  gtk_box_set_spacing(GTK_BOX(content_area), gtk_util::kContentAreaSpacing);

  GtkWidget* right_column_area;
  if (skia_icon) {
    // Create a two column layout.
    GtkWidget* icon_hbox = gtk_hbox_new(FALSE, gtk_util::kContentAreaSpacing);
    gtk_box_pack_start(GTK_BOX(content_area), icon_hbox, TRUE, TRUE, 0);

    // Put Icon in the left column.
    GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(skia_icon);
    GtkWidget* icon = gtk_image_new_from_pixbuf(pixbuf);
    gtk_box_pack_start(GTK_BOX(icon_hbox), icon, TRUE, TRUE, 0);

    // Create a new vbox for the right column.
    right_column_area = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(icon_hbox), right_column_area, TRUE, TRUE, 0);
  } else {
    right_column_area = content_area;
  }

  // Prompt.
  std::string prompt_text = WideToUTF8(l10n_util::GetStringF(
      IDS_EXTENSION_PROMPT_HEADING, UTF8ToWide(extension->name())));
  GtkWidget* prompt_label = MakeMarkupLabel("<span weight=\"bold\">%s</span>",
                                            prompt_text);
  gtk_misc_set_alignment(GTK_MISC(prompt_label), 0.0, 0.5);
  gtk_label_set_selectable(GTK_LABEL(prompt_label), TRUE);
  gtk_box_pack_start(GTK_BOX(right_column_area), prompt_label, TRUE, TRUE, 0);

  // Pick a random warning.
  std::string warnings[] = {
      l10n_util::GetStringUTF8(IDS_EXTENSION_PROMPT_WARNING_1),
      l10n_util::GetStringUTF8(IDS_EXTENSION_PROMPT_WARNING_2),
      l10n_util::GetStringUTF8(IDS_EXTENSION_PROMPT_WARNING_3)
  };
  std::string warning_text = warnings[
      base::RandInt(0, arraysize(warnings) - 1)];
  GtkWidget* warning_label = gtk_label_new(warning_text.c_str());
  gtk_label_set_line_wrap(GTK_LABEL(warning_label), TRUE);
  gtk_widget_set_size_request(warning_label, kRightColumnWidth, -1);
  gtk_misc_set_alignment(GTK_MISC(warning_label), 0.0, 0.5);
  gtk_label_set_selectable(GTK_LABEL(warning_label), TRUE);
  gtk_box_pack_start(GTK_BOX(right_column_area), warning_label, TRUE, TRUE, 0);

  // Severe label
  std::string severe_text = l10n_util::GetStringUTF8(
      IDS_EXTENSION_PROMPT_WARNING_SEVERE);
  GtkWidget* severe_label = MakeMarkupLabel("<span weight=\"bold\">%s</span>",
                                            severe_text);
  GdkColor red = GDK_COLOR_RGB(0xff, 0x00, 0x00);
  gtk_widget_modify_fg(severe_label, GTK_STATE_NORMAL, &red);
  gtk_misc_set_alignment(GTK_MISC(severe_label), 0.0, 0.5);
  gtk_label_set_selectable(GTK_LABEL(severe_label), TRUE);
  gtk_box_pack_start(GTK_BOX(right_column_area), severe_label, TRUE, TRUE, 0);

  g_signal_connect(dialog, "response", G_CALLBACK(OnDialogResponse), delegate);
  gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
  gtk_widget_show_all(dialog);
}

}  // namespace

void ExtensionInstallUI::ShowExtensionInstallPrompt(
    Profile* profile, Delegate* delegate, Extension* extension, SkBitmap* icon,
    const std::wstring& warning_text) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  if (!browser) {
    delegate->ContinueInstall();
    return;
  }

  BrowserWindowGtk* browser_window = static_cast<BrowserWindowGtk*>(
      browser->window());
  if (!browser_window) {
    delegate->AbortInstall();
    return;
  }

  ShowInstallPromptDialog(browser_window->window(), icon, extension, delegate);
}
