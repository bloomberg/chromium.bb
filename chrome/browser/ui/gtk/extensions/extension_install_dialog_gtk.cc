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
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/page_navigator.h"
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
const int kDetailIndent = 20;

// Additional padding (beyond on ui::kControlSpacing) all sides of each
// permission in the permissions list.
const int kPermissionsPadding = 2;
const int kExtensionsPadding = kPermissionsPadding;

const double kRatingTextSize = 12.1;  // 12.1px = 9pt @ 96dpi

// Adds a Skia image as an icon control to the given container.
void AddResourceIcon(const gfx::ImageSkia* icon, void* data) {
  GtkWidget* container = static_cast<GtkWidget*>(data);
  GdkPixbuf* icon_pixbuf = gfx::GdkPixbufFromSkBitmap(*icon->bitmap());
  GtkWidget* icon_widget = gtk_image_new_from_pixbuf(icon_pixbuf);
  g_object_unref(icon_pixbuf);
  gtk_box_pack_start(GTK_BOX(container), icon_widget, FALSE, FALSE, 0);
}

void OnZippyButtonRealize(GtkWidget* event_box, gpointer unused) {
  gdk_window_set_cursor(event_box->window, gfx::GetCursor(GDK_HAND2));
}

gboolean OnZippyButtonRelease(GtkWidget* event_box,
                              GdkEvent* event,
                              GtkWidget* detail_box) {
  if (event->button.button != 1)
    return FALSE;

  GtkWidget* arrow =
      GTK_WIDGET(gtk_object_get_data(GTK_OBJECT(event_box), "arrow"));

  if (gtk_widget_get_visible(detail_box)) {
    gtk_widget_hide(detail_box);
    gtk_arrow_set(GTK_ARROW(arrow), GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
  } else {
    gtk_widget_set_no_show_all(detail_box, FALSE);
    gtk_widget_show_all(detail_box);
    gtk_widget_set_no_show_all(detail_box, TRUE);
    gtk_arrow_set(GTK_ARROW(arrow), GTK_ARROW_DOWN, GTK_SHADOW_OUT);
  }

  return TRUE;
}

}  // namespace

namespace browser {

// Displays the dialog when constructed, deletes itself when dialog is
// dismissed. Success/failure is passed back through the
// ExtensionInstallPrompt::Delegate instance.
class ExtensionInstallDialog {
 public:
  ExtensionInstallDialog(gfx::NativeWindow parent,
                         content::PageNavigator* navigator,
                         ExtensionInstallPrompt::Delegate* delegate,
                         const ExtensionInstallPrompt::Prompt& prompt);
 private:
  ~ExtensionInstallDialog();

  CHROMEGTK_CALLBACK_1(ExtensionInstallDialog, void, OnResponse, int);
  CHROMEGTK_CALLBACK_0(ExtensionInstallDialog, void, OnStoreLinkClick);

  GtkWidget* CreateWidgetForIssueAdvice(
      const IssueAdviceInfoEntry& issue_advice, int pixel_width);

  content::PageNavigator* navigator_;
  ExtensionInstallPrompt::Delegate* delegate_;
  std::string extension_id_;  // Set for INLINE_INSTALL_PROMPT.
  GtkWidget* dialog_;
};

ExtensionInstallDialog::ExtensionInstallDialog(
    gfx::NativeWindow parent,
    content::PageNavigator* navigator,
    ExtensionInstallPrompt::Delegate *delegate,
    const ExtensionInstallPrompt::Prompt& prompt)
    : navigator_(navigator),
      delegate_(delegate),
      dialog_(NULL) {
  bool show_permissions = prompt.GetPermissionCount() > 0;
  bool show_oauth_issues = prompt.GetOAuthIssueCount() > 0;
  bool is_inline_install =
      prompt.type() == ExtensionInstallPrompt::INLINE_INSTALL_PROMPT;
  bool is_bundle_install =
      prompt.type() == ExtensionInstallPrompt::BUNDLE_INSTALL_PROMPT;

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

  // We don't show the image for bunle installs, so let the left column take up
  // that space.
  const int left_column_min_width =
      kLeftColumnMinWidth + (is_bundle_install ? kImageSize : 0);

  // Create a new vbox for the left column.
  GtkWidget* left_column_area = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(top_content_hbox), left_column_area,
                     TRUE, TRUE, 0);
  gtk_widget_set_size_request(left_column_area, left_column_min_width, -1);

  GtkWidget* heading_vbox = gtk_vbox_new(FALSE, 0);
  // If we are not going to show anything else, vertically center the title.
  bool center_heading =
      !show_permissions && !show_oauth_issues && !is_inline_install;
  gtk_box_pack_start(GTK_BOX(left_column_area), heading_vbox, center_heading,
                     center_heading, 0);

  // Heading
  GtkWidget* heading_label = gtk_util::CreateBoldLabel(
      UTF16ToUTF8(prompt.GetHeading().c_str()));
  gtk_util::SetLabelWidth(heading_label, left_column_min_width);
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
    gtk_util::SetLabelWidth(users_label, left_column_min_width);
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
      gtk_util::SetLabelWidth(extension_label, left_column_min_width);
      gtk_box_pack_start(GTK_BOX(extensions_vbox), extension_label,
                         FALSE, FALSE, kExtensionsPadding);
    }
  } else {
    // Resize the icon if necessary.
    SkBitmap scaled_icon = *prompt.icon().ToSkBitmap();
    if (scaled_icon.width() > kImageSize || scaled_icon.height() > kImageSize) {
      scaled_icon = skia::ImageOperations::Resize(
          scaled_icon, skia::ImageOperations::RESIZE_LANCZOS3,
          kImageSize, kImageSize);
    }

    // Put icon in the right column.
    GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(scaled_icon);
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
    gtk_util::SetLabelWidth(permissions_header, left_column_min_width);
    gtk_box_pack_start(GTK_BOX(permissions_container), permissions_header,
                       FALSE, FALSE, 0);

    for (size_t i = 0; i < prompt.GetPermissionCount(); ++i) {
      std::string permission = l10n_util::GetStringFUTF8(
          IDS_EXTENSION_PERMISSION_LINE, prompt.GetPermission(i));
      GtkWidget* permission_label = gtk_label_new(permission.c_str());
      gtk_util::SetLabelWidth(permission_label, left_column_min_width);
      gtk_box_pack_start(GTK_BOX(permissions_container), permission_label,
                         FALSE, FALSE, kPermissionsPadding);
    }
  }

  if (show_oauth_issues) {
    // If permissions are shown, then the scopes will go below them and take
    // up the entire width of the dialog. Otherwise the scopes will go where
    // the permissions usually go.
    GtkWidget* oauth_issues_container =
        show_permissions ? content_vbox : left_column_area;
    int pixel_width = left_column_min_width +
        (show_permissions ? kImageSize : 0);

    GtkWidget* oauth_issues_header = gtk_util::CreateBoldLabel(
        UTF16ToUTF8(prompt.GetOAuthHeading()).c_str());
    gtk_util::SetLabelWidth(oauth_issues_header, pixel_width);
    gtk_box_pack_start(GTK_BOX(oauth_issues_container), oauth_issues_header,
                       FALSE, FALSE, 0);

    for (size_t i = 0; i < prompt.GetOAuthIssueCount(); ++i) {
      GtkWidget* issue_advice_widget =
          CreateWidgetForIssueAdvice(prompt.GetOAuthIssue(i), pixel_width);
      gtk_box_pack_start(GTK_BOX(oauth_issues_container), issue_advice_widget,
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
  if (response_id == GTK_RESPONSE_ACCEPT)
    delegate_->InstallUIProceed();
  else
    delegate_->InstallUIAbort(true);

  gtk_widget_destroy(dialog_);
  delete this;
}

void ExtensionInstallDialog::OnStoreLinkClick(GtkWidget* sender) {
  GURL store_url(
      extension_urls::GetWebstoreItemDetailURLPrefix() + extension_id_);
  navigator_->OpenURL(OpenURLParams(
      store_url, content::Referrer(), NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK, false));

  OnResponse(dialog_, GTK_RESPONSE_CLOSE);
}

GtkWidget* ExtensionInstallDialog::CreateWidgetForIssueAdvice(
    const IssueAdviceInfoEntry& issue_advice, int pixel_width) {
  GtkWidget* box = gtk_vbox_new(FALSE, ui::kControlSpacing);
  GtkWidget* header = gtk_hbox_new(FALSE, 0);
  GtkWidget* event_box = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(event_box), header);
  gtk_box_pack_start(GTK_BOX(box), event_box, FALSE, FALSE,
                     kPermissionsPadding);

  GtkWidget* arrow = NULL;
  GtkWidget* label = NULL;
  int label_pixel_width = pixel_width;

  if (issue_advice.details.empty()) {
    label = gtk_label_new(l10n_util::GetStringFUTF8(
        IDS_EXTENSION_PERMISSION_LINE,
        issue_advice.description).c_str());
  } else {
    arrow = gtk_arrow_new(GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
    GtkRequisition req;
    gtk_widget_size_request(arrow, &req);
    label_pixel_width -= req.width;

    label = gtk_label_new(UTF16ToUTF8(issue_advice.description).c_str());

    GtkWidget* detail_box = gtk_vbox_new(FALSE, ui::kControlSpacing);
    gtk_box_pack_start(GTK_BOX(box), detail_box, FALSE, FALSE, 0);
    gtk_widget_set_no_show_all(detail_box, TRUE);
    gtk_object_set_data(GTK_OBJECT(event_box), "arrow", arrow);

    for (size_t i = 0; i < issue_advice.details.size(); ++i) {
      std::string text = l10n_util::GetStringFUTF8(
          IDS_EXTENSION_PERMISSION_LINE, issue_advice.details[i]);
      GtkWidget* label = gtk_label_new(text.c_str());
      gtk_util::SetLabelWidth(label, pixel_width - kDetailIndent);

      GtkWidget* align = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
      gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, kDetailIndent, 0);
      gtk_container_add(GTK_CONTAINER(align), label);
      gtk_box_pack_start(GTK_BOX(detail_box), align, FALSE, FALSE,
                         kPermissionsPadding);
    }

    g_signal_connect(event_box, "realize",
                     G_CALLBACK(OnZippyButtonRealize), NULL);
    g_signal_connect(event_box, "button-release-event",
                     G_CALLBACK(OnZippyButtonRelease), detail_box);
  }

  gtk_util::SetLabelWidth(label, label_pixel_width);
  if (arrow)
    gtk_box_pack_start(GTK_BOX(header), arrow, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(header), label, TRUE, TRUE, 0);

  return box;
}

}  // namespace browser

void ShowExtensionInstallDialogImpl(
    gfx::NativeWindow parent,
    content::PageNavigator* navigator,
    ExtensionInstallPrompt::Delegate* delegate,
    const ExtensionInstallPrompt::Prompt& prompt) {
  new browser::ExtensionInstallDialog(parent, navigator, delegate, prompt);
}
