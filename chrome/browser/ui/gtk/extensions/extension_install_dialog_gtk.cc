// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "base/i18n/rtl.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/extensions/extension_install_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"

using content::OpenURLParams;
using extensions::BundleInstaller;

namespace {

const int kLeftColumnMinWidth = 250;
const int kImageSize = 69;

// Additional padding (beyond on ui::kControlSpacing) all sides of each
// permission in the permissions list.
const int kPermissionsPadding = 2;
const int kExtensionsPadding = kPermissionsPadding;

const double kRatingTextSize = 12.1;  // 12.1px = 9pt @ 96dpi

// Adds a Skia image as an icon control to the given container.
void AddResourceIcon(const SkBitmap* icon, void* data) {
  GtkWidget* container = static_cast<GtkWidget*>(data);
  GdkPixbuf* icon_pixbuf = gfx::GdkPixbufFromSkBitmap(icon);
  GtkWidget* icon_widget = gtk_image_new_from_pixbuf(icon_pixbuf);
  g_object_unref(icon_pixbuf);
  gtk_box_pack_start(GTK_BOX(container), icon_widget, FALSE, FALSE, 0);
}

// Displays the dialog when constructed, deletes itself when dialog is
// dismissed. Success/failure is passed back through the ExtensionInstallUI::
// Delegate instance.
class ExtensionInstallDialog {
 public:
  ExtensionInstallDialog(GtkWindow* parent,
                         ExtensionInstallUI::Delegate *delegate,
                         const ExtensionInstallUI::Prompt& prompt);
 private:
  ~ExtensionInstallDialog();

  CHROMEGTK_CALLBACK_1(ExtensionInstallDialog, void, OnResponse, int);
  CHROMEGTK_CALLBACK_0(ExtensionInstallDialog, void, OnStoreLinkClick);

  ExtensionInstallUI::Delegate* delegate_;
  std::string extension_id_;  // Set for INLINE_INSTALL_PROMPT.
  GtkWidget* dialog_;
};

ExtensionInstallDialog::ExtensionInstallDialog(
    GtkWindow* parent,
    ExtensionInstallUI::Delegate *delegate,
    const ExtensionInstallUI::Prompt& prompt)
    : delegate_(delegate),
      dialog_(NULL) {
  bool show_permissions = prompt.GetPermissionCount() > 0;
  bool is_inline_install =
      prompt.type() == ExtensionInstallUI::INLINE_INSTALL_PROMPT;
  bool is_bundle_install =
      prompt.type() == ExtensionInstallUI::BUNDLE_INSTALL_PROMPT;

  if (is_inline_install)
    extension_id_ = prompt.extension()->id();

  // Build the dialog.
  dialog_ = gtk_dialog_new_with_buttons(
      UTF16ToUTF8(prompt.GetDialogTitle()).c_str(),
      parent,
      GTK_DIALOG_MODAL,
      NULL);
  GtkWidget* close_button = gtk_dialog_add_button(
      GTK_DIALOG(dialog_),
      prompt.HasAbortButtonLabel() ?
          UTF16ToUTF8(prompt.GetAbortButtonLabel()).c_str() : GTK_STOCK_CANCEL,
      GTK_RESPONSE_CLOSE);
  gtk_dialog_add_button(
      GTK_DIALOG(dialog_),
      UTF16ToUTF8(prompt.GetAcceptButtonLabel()).c_str(),
      GTK_RESPONSE_ACCEPT);
#if !GTK_CHECK_VERSION(2, 22, 0)
  gtk_dialog_set_has_separator(GTK_DIALOG(dialog_), FALSE);
#endif

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
  // If we are not going to show anything else, vertically center the title.
  bool center_heading = !show_permissions && !is_inline_install;
  gtk_box_pack_start(GTK_BOX(left_column_area), heading_vbox, center_heading,
                     center_heading, 0);

  // Heading
  GtkWidget* heading_label = gtk_util::CreateBoldLabel(
      UTF16ToUTF8(prompt.GetHeading().c_str()));
  gtk_label_set_line_wrap(GTK_LABEL(heading_label), true);
  gtk_misc_set_alignment(GTK_MISC(heading_label), 0.0, 0.5);
  gtk_box_pack_start(GTK_BOX(heading_vbox), heading_label, center_heading,
                     center_heading, 0);

  if (is_inline_install) {
    // Average rating (as stars) and number of ratings.
    GtkWidget* stars_hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(heading_vbox), stars_hbox, FALSE, FALSE, 0);
    prompt.AppendRatingStars(AddResourceIcon, stars_hbox);
    GtkWidget* rating_label = gtk_label_new(UTF16ToUTF8(
        prompt.GetRatingCount()).c_str());
    gtk_util::ForceFontSizePixels(rating_label, kRatingTextSize);
    gtk_box_pack_start(GTK_BOX(stars_hbox), rating_label,
                       FALSE, FALSE, 3);

    // User count.
    GtkWidget* users_label = gtk_label_new(UTF16ToUTF8(
        prompt.GetUserCount()).c_str());
    gtk_util::SetLabelWidth(users_label, kLeftColumnMinWidth);
    gtk_util::SetLabelColor(users_label, &ui::kGdkGray);
    gtk_util::ForceFontSizePixels(rating_label, kRatingTextSize);
    gtk_box_pack_start(GTK_BOX(heading_vbox), users_label,
                       FALSE, FALSE, 0);

    // Store link.
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

  if (is_bundle_install) {
    // Add the list of extensions to be installed.
    GtkWidget* extensions_vbox = gtk_vbox_new(FALSE, ui::kControlSpacing);
    gtk_box_pack_start(GTK_BOX(heading_vbox), extensions_vbox, FALSE, FALSE,
                       ui::kControlSpacing);

    BundleInstaller::ItemList items = prompt.bundle()->GetItemsWithState(
        BundleInstaller::Item::STATE_PENDING);
    for (size_t i = 0; i < items.size(); ++i) {
      GtkWidget* extension_label = gtk_label_new(UTF16ToUTF8(
          items[i].GetNameForDisplay()).c_str());
      gtk_util::SetLabelWidth(extension_label, kLeftColumnMinWidth);
      gtk_box_pack_start(GTK_BOX(extensions_vbox), extension_label,
                         FALSE, FALSE, kExtensionsPadding);
    }
  }

  if (!is_bundle_install) {
    // Resize the icon if necessary.
    SkBitmap scaled_icon = *prompt.icon().ToSkBitmap();
    if (scaled_icon.width() > kImageSize || scaled_icon.height() > kImageSize) {
      scaled_icon = skia::ImageOperations::Resize(
          scaled_icon, skia::ImageOperations::RESIZE_LANCZOS3,
          kImageSize, kImageSize);
    }

    // Put icon in the right column.
    GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&scaled_icon);
    GtkWidget* icon = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);
    gtk_box_pack_start(GTK_BOX(top_content_hbox), icon, FALSE, FALSE, 0);
    // Top justify the image.
    gtk_misc_set_alignment(GTK_MISC(icon), 0.5, 0.0);
  }

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

    GtkWidget* permissions_header = gtk_util::CreateBoldLabel(
        UTF16ToUTF8(prompt.GetPermissionsHeading()).c_str());
    gtk_util::SetLabelWidth(permissions_header, kLeftColumnMinWidth);
    gtk_box_pack_start(GTK_BOX(permissions_container), permissions_header,
                       FALSE, FALSE, 0);

    for (size_t i = 0; i < prompt.GetPermissionCount(); ++i) {
      GtkWidget* permission_label = gtk_label_new(UTF16ToUTF8(
          prompt.GetPermission(i)).c_str());
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
      extension_urls::GetWebstoreItemDetailURLPrefix() + extension_id_);
  BrowserList::GetLastActive()->OpenURL(OpenURLParams(
      store_url, content::Referrer(), NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK, false));

  OnResponse(dialog_, GTK_RESPONSE_CLOSE);
}

}  // namespace

void ShowExtensionInstallDialogImpl(
    Profile* profile,
    ExtensionInstallUI::Delegate* delegate,
    const ExtensionInstallUI::Prompt& prompt) {
  Browser* browser = browser::FindLastActiveWithProfile(profile);
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

  new ExtensionInstallDialog(browser_window->window(), delegate, prompt);
}
