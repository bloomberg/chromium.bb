// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_install_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"

namespace {

const int kLeftColumnMinWidth = 250;
const int kImageSize = 69;

// Additional padding (beyond on ui::kControlSpacing) all sides of each
// permission in the permissions list.
const int kPermissionsPadding = 2;

const double kRatingTextSize = 12.1;  // 12.1px = 9pt @ 96dpi

// Loads an image and adds it as an icon control to the given container.
GtkWidget* AddResourceIcon(int resource_id, GtkWidget* container) {
  const SkBitmap* icon = ResourceBundle::GetSharedInstance().GetBitmapNamed(
    resource_id);
  GdkPixbuf* icon_pixbuf = gfx::GdkPixbufFromSkBitmap(icon);
  GtkWidget* icon_widget = gtk_image_new_from_pixbuf(icon_pixbuf);
  g_object_unref(icon_pixbuf);
  gtk_box_pack_start(GTK_BOX(container), icon_widget, FALSE, FALSE, 0);
  return icon_widget;
}

// Adds star icons corresponding to a rating. Replicates the CWS stars display
// logic (from components.ratingutils.setFractionalYellowStars).
void AddRatingStars(double average_rating, GtkWidget* container) {
  int rating_integer = floor(average_rating);
  double rating_fractional = average_rating - rating_integer;

  if (rating_fractional > 0.66) {
    rating_integer++;
  }

  if (rating_fractional < 0.33 || rating_fractional > 0.66) {
    rating_fractional = 0;
  }

  int i = 0;
  while (i++ < rating_integer) {
    AddResourceIcon(IDR_EXTENSIONS_RATING_STAR_ON, container);
  }
  if (rating_fractional) {
    AddResourceIcon(IDR_EXTENSIONS_RATING_STAR_HALF_LEFT, container);
    i++;
  }
  while (i++ < ExtensionInstallUI::kMaxExtensionRating) {
    AddResourceIcon(IDR_EXTENSIONS_RATING_STAR_OFF, container);
  }
}

// Displays the dialog when constructed, deletes itself when dialog is
// dismissed. Success/failure is passed back through the ExtensionInstallUI::
// Delegate instance.
class ExtensionInstallDialog {
 public:
  ExtensionInstallDialog(GtkWindow* parent,
                         ExtensionInstallUI::Delegate *delegate,
                         const Extension* extension,
                         SkBitmap* skia_icon,
                         const ExtensionInstallUI::Prompt& prompt);
 private:
  virtual ~ExtensionInstallDialog();

  CHROMEGTK_CALLBACK_1(ExtensionInstallDialog, void, OnResponse, int);
  CHROMEGTK_CALLBACK_0(ExtensionInstallDialog, void, OnStoreLinkClick);

  ExtensionInstallUI::Delegate* delegate_;
  const Extension* extension_;
  GtkWidget* dialog_;
};

ExtensionInstallDialog::ExtensionInstallDialog(
    GtkWindow* parent,
    ExtensionInstallUI::Delegate *delegate,
    const Extension* extension,
    SkBitmap* skia_icon,
    const ExtensionInstallUI::Prompt& prompt)
    : delegate_(delegate),
      extension_(extension) {
  ExtensionInstallUI::PromptType type = prompt.type;
  const std::vector<string16>& permissions = prompt.permissions;
  bool show_permissions = !permissions.empty();
  bool is_inline_install = type == ExtensionInstallUI::INLINE_INSTALL_PROMPT;

  // Build the dialog.
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(ExtensionInstallUI::kTitleIds[type]).c_str(),
      parent,
      GTK_DIALOG_MODAL,
      NULL);
  int cancel = ExtensionInstallUI::kAbortButtonIds[type];
  GtkWidget* close_button = gtk_dialog_add_button(
      GTK_DIALOG(dialog_),
      cancel > 0 ? l10n_util::GetStringUTF8(cancel).c_str(): GTK_STOCK_CANCEL,
      GTK_RESPONSE_CLOSE);
  gtk_dialog_add_button(
      GTK_DIALOG(dialog_),
      l10n_util::GetStringUTF8(ExtensionInstallUI::kButtonIds[type]).c_str(),
      GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_has_separator(GTK_DIALOG(dialog_), FALSE);

  GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog_));
  gtk_box_set_spacing(GTK_BOX(content_area), ui::kContentAreaSpacing);

  // Divide the dialog vertically (item data and icon on the top, permissions
  // on the bottom).
  GtkWidget* content_vbox = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(content_area), content_vbox, TRUE, TRUE, 0);

  // Create a two column layout for the top (item data on the left, icon on
  // the right).
  GtkWidget* top_content_hbox = gtk_hbox_new(FALSE, ui::kContentAreaSpacing);
  gtk_box_pack_start(GTK_BOX(content_vbox), top_content_hbox, TRUE, TRUE, 0);

  // Create a new vbox for the left column.
  GtkWidget* left_column_area = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(top_content_hbox), left_column_area,
                     TRUE, TRUE, 0);

  GtkWidget* heading_vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(left_column_area), heading_vbox, FALSE, FALSE, 0);

  // Heading (name-only for inline installs)
  std::string heading_text = is_inline_install ? extension_->name() :
      l10n_util::GetStringFUTF8(ExtensionInstallUI::kHeadingIds[type],
                                UTF8ToUTF16(extension_->name()));
  GtkWidget* heading_label = gtk_util::CreateBoldLabel(heading_text);
  gtk_label_set_line_wrap(GTK_LABEL(heading_label), true);
  gtk_misc_set_alignment(GTK_MISC(heading_label), 0.0, 0.5);
  // If we are not going to show anything else, vertically center the title.
  bool center_heading = !show_permissions && !is_inline_install;
  gtk_box_pack_start(GTK_BOX(heading_vbox), heading_label, center_heading,
                     center_heading, 0);

  if (is_inline_install) {
    // Average rating (as stars) and number of ratings
    GtkWidget* stars_hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(heading_vbox), stars_hbox, FALSE, FALSE, 0);
    AddRatingStars(prompt.average_rating, stars_hbox);
    GtkWidget* rating_label = gtk_label_new(l10n_util::GetStringFUTF8(
        IDS_EXTENSION_RATING_COUNT,
        UTF8ToUTF16(base::IntToString(prompt.rating_count))).c_str());
    gtk_util::ForceFontSizePixels(rating_label, kRatingTextSize);
    gtk_box_pack_start(GTK_BOX(stars_hbox), rating_label,
                       FALSE, FALSE, 3);

    // User count
    GtkWidget* users_label = gtk_label_new(l10n_util::GetStringFUTF8(
        IDS_EXTENSION_USER_COUNT,
        UTF8ToUTF16(prompt.localized_user_count)).c_str());
    gtk_util::SetLabelWidth(users_label, kLeftColumnMinWidth);
    gtk_util::SetLabelColor(users_label, &ui::kGdkGray);
    gtk_util::ForceFontSizePixels(rating_label, kRatingTextSize);
    gtk_box_pack_start(GTK_BOX(heading_vbox), users_label,
                       FALSE, FALSE, 0);

    // Store link
    GtkWidget* store_link = gtk_chrome_link_button_new(
        l10n_util::GetStringUTF8(IDS_EXTENSION_PROMPT_STORE_LINK).c_str());
    gtk_util::ForceFontSizePixels(store_link, kRatingTextSize);
    GtkWidget* store_link_hbox = gtk_hbox_new(FALSE, 0);
    // Stick it in an hbox so it doesn't expand to the whole width.
    gtk_box_pack_start(GTK_BOX(store_link_hbox), store_link, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(heading_vbox), store_link_hbox, FALSE, FALSE, 0);
    g_signal_connect(store_link, "clicked",
                     G_CALLBACK(OnStoreLinkClickThunk), this);
  }

  // Resize the icon if necessary.
  SkBitmap scaled_icon = *skia_icon;
  if (scaled_icon.width() > kImageSize || scaled_icon.height() > kImageSize) {
    scaled_icon = skia::ImageOperations::Resize(scaled_icon,
        skia::ImageOperations::RESIZE_LANCZOS3,
        kImageSize, kImageSize);
  }

  // Put icon in the right column.
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&scaled_icon);
  GtkWidget* icon = gtk_image_new_from_pixbuf(pixbuf);
  g_object_unref(pixbuf);
  gtk_box_pack_start(GTK_BOX(top_content_hbox), icon, FALSE, FALSE, 0);
  // Top justify the image.
  gtk_misc_set_alignment(GTK_MISC(icon), 0.5, 0.0);

  // Permissions are shown separated by a divider for inline installs, or
  // directly under the heading for regular installs (where we don't have
  // the store data)
  if (show_permissions) {
    GtkWidget* permissions_container;
    if (is_inline_install) {
      permissions_container = content_vbox;
      gtk_box_pack_start(GTK_BOX(content_vbox), gtk_hseparator_new(),
                         FALSE, FALSE, ui::kControlSpacing);
    } else {
      permissions_container = left_column_area;
    }

    GtkWidget* warning_label = gtk_util::CreateBoldLabel(
        l10n_util::GetStringUTF8(
            ExtensionInstallUI::kWarningIds[type]).c_str());
    gtk_util::SetLabelWidth(warning_label, kLeftColumnMinWidth);
    gtk_box_pack_start(GTK_BOX(permissions_container), warning_label, FALSE,
                       FALSE, is_inline_install ? 0 : ui::kControlSpacing);

    for (std::vector<string16>::const_iterator iter = permissions.begin();
         iter != permissions.end(); ++iter) {
      GtkWidget* permission_label = gtk_label_new(l10n_util::GetStringFUTF8(
          IDS_EXTENSION_PERMISSION_LINE, *iter).c_str());
      gtk_util::SetLabelWidth(permission_label, kLeftColumnMinWidth);
      gtk_box_pack_start(GTK_BOX(permissions_container), permission_label,
                         FALSE, FALSE, kPermissionsPadding);
    }
  }

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);
  gtk_window_set_resizable(GTK_WINDOW(dialog_), FALSE);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_CLOSE);
  gtk_widget_show_all(dialog_);
  gtk_widget_grab_focus(close_button);
}

ExtensionInstallDialog::~ExtensionInstallDialog() {
}

void ExtensionInstallDialog::OnResponse(GtkWidget* dialog, int response_id) {
  if (response_id == GTK_RESPONSE_ACCEPT) {
    delegate_->InstallUIProceed();
  } else {
    delegate_->InstallUIAbort(true);
  }

  gtk_widget_destroy(dialog_);
  delete this;
}

void ExtensionInstallDialog::OnStoreLinkClick(GtkWidget* sender) {
  GURL store_url(
      extension_urls::GetWebstoreItemDetailURLPrefix() + extension_->id());
  BrowserList::GetLastActive()->OpenURL(OpenURLParams(
      store_url, GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK));

  OnResponse(dialog_, GTK_RESPONSE_CLOSE);
}

}  // namespace

void ShowExtensionInstallDialog(
    Profile* profile,
    ExtensionInstallUI::Delegate* delegate,
    const Extension* extension,
    SkBitmap* icon,
    const ExtensionInstallUI::Prompt& prompt) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  if (!browser) {
    delegate->InstallUIAbort(false);
    return;
  }

  BrowserWindowGtk* browser_window = static_cast<BrowserWindowGtk*>(
      browser->window());
  if (!browser_window) {
    delegate->InstallUIAbort(false);
    return;
  }

  new ExtensionInstallDialog(browser_window->window(),
                             delegate,
                             extension,
                             icon,
                             prompt);
}
