// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/gtk_util.h"

class Profile;

namespace {

const int kRightColumnMinWidth = 290;

const int kImageSize = 69;

// Padding on all sides of each permission in the permissions list.
const int kPermissionsPadding = 8;

// Make a GtkLabel with |str| as its text, using the formatting in |format|.
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

void ShowInstallPromptDialog2(GtkWindow* parent, SkBitmap* skia_icon,
                              const Extension* extension,
                              ExtensionInstallUI::Delegate *delegate,
                              const std::vector<string16>& permissions,
                              ExtensionInstallUI::PromptType type) {
  // Build the dialog.
  GtkWidget* dialog = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(ExtensionInstallUI::kTitleIds[type]).c_str(),
      parent,
      GTK_DIALOG_MODAL,
      NULL);
  GtkWidget* close_button = gtk_dialog_add_button(GTK_DIALOG(dialog),
      GTK_STOCK_CANCEL, GTK_RESPONSE_CLOSE);
  gtk_dialog_add_button(
      GTK_DIALOG(dialog),
      l10n_util::GetStringUTF8(ExtensionInstallUI::kButtonIds[type]).c_str(),
      GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

  // Create a two column layout.
  GtkWidget* content_area = GTK_DIALOG(dialog)->vbox;
  gtk_box_set_spacing(GTK_BOX(content_area), gtk_util::kContentAreaSpacing);

  GtkWidget* icon_hbox = gtk_hbox_new(FALSE, gtk_util::kContentAreaSpacing);
  gtk_box_pack_start(GTK_BOX(content_area), icon_hbox, TRUE, TRUE, 0);

  // Resize the icon if necessary.
  SkBitmap scaled_icon = *skia_icon;
  if (scaled_icon.width() > kImageSize || scaled_icon.height() > kImageSize) {
    scaled_icon = skia::ImageOperations::Resize(scaled_icon,
        skia::ImageOperations::RESIZE_LANCZOS3,
        kImageSize, kImageSize);
  }

  // Put Icon in the left column.
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&scaled_icon);
  GtkWidget* icon = gtk_image_new_from_pixbuf(pixbuf);
  g_object_unref(pixbuf);
  gtk_box_pack_start(GTK_BOX(icon_hbox), icon, FALSE, FALSE, 0);
  // Top justify the image.
  gtk_misc_set_alignment(GTK_MISC(icon), 0.5, 0.0);

  // Create a new vbox for the right column.
  GtkWidget* right_column_area = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(icon_hbox), right_column_area, TRUE, TRUE, 0);

  std::string heading_text = l10n_util::GetStringFUTF8(
      ExtensionInstallUI::kHeadingIds[type], UTF8ToUTF16(extension->name()));
  GtkWidget* heading_label = MakeMarkupLabel("<span weight=\"bold\">%s</span>",
                                             heading_text);
  gtk_label_set_line_wrap(GTK_LABEL(heading_label), true);
  gtk_misc_set_alignment(GTK_MISC(heading_label), 0.0, 0.5);
  bool show_permissions = !permissions.empty();
  // If we are not going to show the permissions, vertically center the title.
  gtk_box_pack_start(GTK_BOX(right_column_area), heading_label,
                     !show_permissions, !show_permissions, 0);

  if (show_permissions) {
    GtkWidget* warning_label = gtk_label_new(l10n_util::GetStringUTF8(
        ExtensionInstallUI::kWarningIds[type]).c_str());
    gtk_util::SetLabelWidth(warning_label, kRightColumnMinWidth);

    gtk_box_pack_start(GTK_BOX(right_column_area), warning_label,
                       FALSE, FALSE, 0);

    GtkWidget* frame = gtk_frame_new(NULL);
    gtk_box_pack_start(GTK_BOX(right_column_area), frame, FALSE, FALSE, 0);

    GtkWidget* text_view = gtk_text_view_new();
    gtk_container_add(GTK_CONTAINER(frame), text_view);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text_view),
                                  kPermissionsPadding);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text_view),
                                   kPermissionsPadding);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    GtkTextTagTable* tag_table = gtk_text_buffer_get_tag_table(buffer);

    GtkTextTag* padding_below_tag = gtk_text_tag_new(NULL);
    g_object_set(G_OBJECT(padding_below_tag), "pixels-below-lines",
                 kPermissionsPadding, NULL);
    g_object_set(G_OBJECT(padding_below_tag), "pixels-below-lines-set",
                 TRUE, NULL);
    gtk_text_tag_table_add(tag_table, padding_below_tag);
    g_object_unref(padding_below_tag);
    GtkTextTag* padding_above_tag = gtk_text_tag_new(NULL);
    g_object_set(G_OBJECT(padding_above_tag), "pixels-above-lines",
                 kPermissionsPadding, NULL);
    g_object_set(G_OBJECT(padding_above_tag), "pixels-above-lines-set",
                 TRUE, NULL);
    gtk_text_tag_table_add(tag_table, padding_above_tag);
    g_object_unref(padding_above_tag);

    GtkTextIter end_iter;
    gtk_text_buffer_get_end_iter(buffer, &end_iter);
    for (std::vector<string16>::const_iterator iter = permissions.begin();
         iter != permissions.end(); ++iter) {
      if (iter != permissions.begin())
        gtk_text_buffer_insert(buffer, &end_iter, "\n", -1);
      gtk_text_buffer_insert_with_tags(
          buffer, &end_iter, UTF16ToUTF8(*iter).c_str(), -1,
          padding_below_tag,
          iter == permissions.begin() ? padding_above_tag : NULL,
          NULL);
    }
  }

  g_signal_connect(dialog, "response", G_CALLBACK(OnDialogResponse), delegate);
  gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);
  gtk_widget_show_all(dialog);
  gtk_widget_grab_focus(close_button);
}

}  // namespace

void ExtensionInstallUI::ShowExtensionInstallUIPrompt2Impl(
    Profile* profile,
    Delegate* delegate,
    const Extension* extension,
    SkBitmap* icon,
    const std::vector<string16>& permissions,
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

  ShowInstallPromptDialog2(browser_window->window(), icon, extension,
                           delegate, permissions, type);
}
