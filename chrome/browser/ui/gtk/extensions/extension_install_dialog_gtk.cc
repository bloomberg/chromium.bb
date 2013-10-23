// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
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
// External installs have more text, so use a wider dialog.
const int kExternalInstallLeftColumnWidth = 350;
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

// Returns an expander with the lines in |details|.
GtkWidget* CreateDetailsWidget(const std::vector<string16>& details,
                               int width,
                               bool show_bullets) {
  GtkWidget* expander = gtk_expander_new(
      l10n_util::GetStringUTF8(IDS_EXTENSIONS_DETAILS).c_str());
  GtkWidget* align = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_container_add(GTK_CONTAINER(expander), align);
  GtkWidget* details_vbox = gtk_vbox_new(FALSE, kPermissionsPadding);
  gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, kDetailIndent, 0);
  gtk_container_add(GTK_CONTAINER(align), details_vbox);

  for (size_t i = 0; i < details.size(); ++i) {
    std::string detail = show_bullets ?
        l10n_util::GetStringFUTF8(IDS_EXTENSION_PERMISSION_LINE, details[0]) :
        base::UTF16ToUTF8(details[i]);
    GtkWidget* detail_label = gtk_label_new(detail.c_str());
    gtk_label_set_line_wrap(GTK_LABEL(detail_label), true);
    gtk_util::SetLabelWidth(detail_label, width - kDetailIndent);
    gtk_box_pack_start(
        GTK_BOX(details_vbox), detail_label, FALSE, FALSE, kPermissionsPadding);
  }
  return expander;
}

}  // namespace

namespace chrome {

// Displays the dialog when constructed, deletes itself when dialog is
// dismissed. Success/failure is passed back through the
// ExtensionInstallPrompt::Delegate instance.
class ExtensionInstallDialog {
 public:
  ExtensionInstallDialog(const ExtensionInstallPrompt::ShowParams& show_params,
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
    const ExtensionInstallPrompt::ShowParams& show_params,
    ExtensionInstallPrompt::Delegate *delegate,
    const ExtensionInstallPrompt::Prompt& prompt)
    : navigator_(show_params.navigator),
      delegate_(delegate),
      dialog_(NULL) {
  bool show_permissions = prompt.ShouldShowPermissions();
  bool show_oauth_issues = prompt.GetOAuthIssueCount() > 0;
  bool show_retained_files = prompt.GetRetainedFileCount() > 0;
  bool is_inline_install =
      prompt.type() == ExtensionInstallPrompt::INLINE_INSTALL_PROMPT;
  bool is_bundle_install =
      prompt.type() == ExtensionInstallPrompt::BUNDLE_INSTALL_PROMPT;
  bool is_external_install =
      prompt.type() == ExtensionInstallPrompt::EXTERNAL_INSTALL_PROMPT;

  if (is_inline_install)
    extension_id_ = prompt.extension()->id();

  // Build the dialog.
  gfx::NativeWindow parent = show_params.parent_window;
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
  if (prompt.HasAcceptButtonLabel()) {
    gtk_dialog_add_button(
        GTK_DIALOG(dialog_),
        UTF16ToUTF8(prompt.GetAcceptButtonLabel()).c_str(),
        GTK_RESPONSE_ACCEPT);
  }
#if !GTK_CHECK_VERSION(2, 22, 0)
  gtk_dialog_set_has_separator(GTK_DIALOG(dialog_), FALSE);
#endif

  GtkWidget* scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                 GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);
  GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog_));
  gtk_box_set_spacing(GTK_BOX(content_area), ui::kContentAreaSpacing);

  // Divide the dialog vertically (item data and icon on the top, permissions
  // on the bottom).
  GtkWidget* content_vbox = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(content_vbox),
                                 ui::kContentAreaBorder);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window),
                                        content_vbox);
  GtkWidget* viewport = gtk_bin_get_child(GTK_BIN(scrolled_window));
  gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_NONE);
  gtk_box_pack_start(GTK_BOX(content_area), scrolled_window, TRUE, TRUE, 0);

  // Create a two column layout for the top (item data on the left, icon on
  // the right).
  GtkWidget* top_content_hbox = gtk_hbox_new(FALSE, ui::kContentAreaSpacing);
  gtk_box_pack_start(GTK_BOX(content_vbox), top_content_hbox, TRUE, TRUE, 0);

  // We don't show the image for bundle installs, so let the left column take
  // up that space.
  int left_column_min_width = kLeftColumnMinWidth;
  if (is_bundle_install)
    left_column_min_width += kImageSize;
  if (is_external_install)
    left_column_min_width = kExternalInstallLeftColumnWidth;

  // Create a new vbox for the left column.
  GtkWidget* left_column_area = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(top_content_hbox), left_column_area,
                     TRUE, TRUE, 0);
  gtk_widget_set_size_request(left_column_area, left_column_min_width, -1);

  GtkWidget* heading_vbox = gtk_vbox_new(FALSE, 0);
  // If we are not going to show anything else, vertically center the title.
  bool center_heading = !show_permissions && !show_oauth_issues &&
                        !is_inline_install && !show_retained_files;
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

    if (prompt.GetPermissionCount() > 0) {
      GtkWidget* permissions_header = gtk_util::CreateBoldLabel(
          UTF16ToUTF8(prompt.GetPermissionsHeading()).c_str());
      gtk_util::SetLabelWidth(permissions_header, left_column_min_width);
      gtk_box_pack_start(GTK_BOX(permissions_container), permissions_header,
                         FALSE, FALSE, 0);

      for (size_t i = 0; i < prompt.GetPermissionCount(); ++i) {
        GtkWidget* permission_vbox = gtk_vbox_new(FALSE, 0);
        std::string permission = l10n_util::GetStringFUTF8(
            IDS_EXTENSION_PERMISSION_LINE, prompt.GetPermission(i));
        GtkWidget* permission_label = gtk_label_new(permission.c_str());
        gtk_util::SetLabelWidth(permission_label, left_column_min_width);
        gtk_box_pack_start(GTK_BOX(permission_vbox), permission_label,
                           FALSE, FALSE, 0);
        if (!prompt.GetPermissionsDetails(i).empty()) {
          std::vector<string16> details;
          details.push_back(prompt.GetPermissionsDetails(i));
          gtk_box_pack_start(
              GTK_BOX(permission_vbox),
              CreateDetailsWidget(details, left_column_min_width, false),
              FALSE,
              FALSE,
              0);
        }
        gtk_box_pack_start(GTK_BOX(permissions_container), permission_vbox,
                           FALSE, FALSE, kPermissionsPadding);
      }
    } else {
      GtkWidget* permission_label = gtk_label_new(l10n_util::GetStringUTF8(
          IDS_EXTENSION_NO_SPECIAL_PERMISSIONS).c_str());
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

  if (show_retained_files) {
    GtkWidget* retained_files_container =
        (show_permissions || show_oauth_issues) ? content_vbox
                                                : left_column_area;
    int pixel_width =
        left_column_min_width +
        ((show_permissions || show_oauth_issues) ? kImageSize : 0);

    GtkWidget* retained_files_header = gtk_util::CreateBoldLabel(
        UTF16ToUTF8(prompt.GetRetainedFilesHeading()).c_str());
    gtk_util::SetLabelWidth(retained_files_header, pixel_width);
    gtk_box_pack_start(GTK_BOX(retained_files_container), retained_files_header,
                       FALSE, FALSE, 0);

    std::vector<string16> paths;
    for (size_t i = 0; i < prompt.GetRetainedFileCount(); ++i) {
      paths.push_back(prompt.GetRetainedFile(i));
    }
    gtk_box_pack_start(GTK_BOX(retained_files_container),
                       CreateDetailsWidget(paths, pixel_width, false),
                       FALSE,
                       FALSE,
                       kPermissionsPadding);
  }

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);
  gtk_window_set_resizable(GTK_WINDOW(dialog_), FALSE);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_CLOSE);
  gtk_widget_show_all(dialog_);

  gtk_container_set_border_width(GTK_CONTAINER(content_area), 0);
  gtk_container_set_border_width(
      GTK_CONTAINER(gtk_dialog_get_action_area(GTK_DIALOG(dialog_))),
      ui::kContentAreaBorder);
  gtk_box_set_spacing(GTK_BOX(gtk_bin_get_child(GTK_BIN(dialog_))), 0);
  GtkRequisition requisition;
  gtk_widget_size_request(content_vbox, &requisition);
  gtk_widget_set_size_request(
      scrolled_window, requisition.width, requisition.height);
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
  GtkWidget* box = gtk_vbox_new(FALSE, 0);
  GtkWidget* label = gtk_label_new(l10n_util::GetStringFUTF8(
      IDS_EXTENSION_PERMISSION_LINE, issue_advice.description).c_str());
  gtk_util::SetLabelWidth(label, pixel_width);
  gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);

  if (!issue_advice.details.empty()) {
    gtk_box_pack_start(
        GTK_BOX(box),
        CreateDetailsWidget(issue_advice.details, pixel_width, true),
        TRUE,
        TRUE,
        0);
  }
  return box;
}

}  // namespace chrome

namespace {

void ShowExtensionInstallDialogImpl(
    const ExtensionInstallPrompt::ShowParams& show_params,
    ExtensionInstallPrompt::Delegate* delegate,
    const ExtensionInstallPrompt::Prompt& prompt) {
  new chrome::ExtensionInstallDialog(show_params, delegate, prompt);
}

}  // namespace

// static
ExtensionInstallPrompt::ShowDialogCallback
ExtensionInstallPrompt::GetDefaultShowDialogCallback() {
  return base::Bind(&ShowExtensionInstallDialogImpl);
}
