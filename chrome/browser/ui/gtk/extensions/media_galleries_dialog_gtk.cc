// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/extensions/media_galleries_dialog_gtk.h"

#include "base/auto_reset.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace chrome {

namespace {

// Color used for additional attachment detail text for galleries.
const GdkColor kDeemphasizedTextColor = GDK_COLOR_RGB(0x96, 0x96, 0x96);

// Width and height of the scrollable area in which galleries are shown.
const int kGalleryControlScrollableWidth = 280;
const int kGalleryControlScrollableHeight = 192;

}  // namespace

typedef MediaGalleriesDialogController::GalleryPermissionsVector
    GalleryPermissionsVector;

MediaGalleriesDialogGtk::MediaGalleriesDialogGtk(
    MediaGalleriesDialogController* controller)
      : controller_(controller),
        window_(NULL),
        confirm_(NULL),
        accepted_(false) {
  contents_.reset(gtk_vbox_new(FALSE, ui::kContentAreaSpacing));
  g_object_ref_sink(contents_.get());
  g_signal_connect(contents_.get(),
                   "destroy",
                   G_CALLBACK(OnDestroyThunk),
                   this);
  InitWidgets();

  // May be NULL during tests.
  if (controller->web_contents()) {
    window_ = CreateWebContentsModalDialogGtk(contents_.get(), confirm_);

    WebContentsModalDialogManager* web_contents_modal_dialog_manager =
        WebContentsModalDialogManager::FromWebContents(
            controller->web_contents());
    web_contents_modal_dialog_manager->ShowDialog(window_);
  }
}

MediaGalleriesDialogGtk::~MediaGalleriesDialogGtk() {
}

void MediaGalleriesDialogGtk::InitWidgets() {
  gtk_util::RemoveAllChildren(contents_.get());
  checkbox_map_.clear();
  confirm_ = NULL;

  GtkWidget* header =
      gtk_util::CreateBoldLabel(UTF16ToUTF8(controller_->GetHeader()));
  gtk_box_pack_start(GTK_BOX(contents_.get()), header, FALSE, FALSE, 0);

  GtkWidget* subtext =
      gtk_label_new(UTF16ToUTF8(controller_->GetSubtext()).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(subtext), TRUE);
  gtk_widget_set_size_request(subtext, 500, -1);
  gtk_box_pack_start(GTK_BOX(contents_.get()), subtext, FALSE, FALSE, 0);

  // The checkboxes are added inside a scrollable area.
  GtkWidget* scroll_window =
      gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                      GTK_SHADOW_ETCHED_IN);

  GtkWidget* checkbox_container = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_widget_set_size_request(scroll_window,
                              kGalleryControlScrollableWidth,
                              kGalleryControlScrollableHeight);
  gtk_container_set_border_width(GTK_CONTAINER(checkbox_container),
                                 ui::kGroupIndent);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll_window),
                                        checkbox_container);
  gtk_widget_show(checkbox_container);
  gtk_box_pack_start(GTK_BOX(contents_.get()), scroll_window,
                     FALSE, FALSE, 0);
  gtk_widget_show(scroll_window);

  // Attached galleries checkboxes
  GalleryPermissionsVector permissions = controller_->AttachedPermissions();
  for (GalleryPermissionsVector::const_iterator iter = permissions.begin();
       iter != permissions.end(); ++iter) {
    UpdateGalleryInContainer(iter->pref_info, iter->allowed,
                             checkbox_container);
  }

  // Separator line and unattached volumes header text.
  GtkWidget* separator = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(checkbox_container), separator, FALSE, FALSE, 0);

  GtkWidget* unattached_hbox = gtk_hbox_new(FALSE, ui::kLabelSpacing);
  GtkWidget* unattached_text = gtk_label_new(UTF16ToUTF8(
      controller_->GetUnattachedLocationsHeader()).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(unattached_text), FALSE);
  gtk_box_pack_start(GTK_BOX(unattached_hbox), unattached_text,
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(checkbox_container), unattached_hbox,
                     FALSE, FALSE, 0);

  // Unattached galleries checkboxes
  const GalleryPermissionsVector unattached_permissions =
      controller_->UnattachedPermissions();
  for (GalleryPermissionsVector::const_iterator iter =
           unattached_permissions.begin();
       iter != unattached_permissions.end(); ++iter) {
    UpdateGalleryInContainer(iter->pref_info, iter->allowed,
                             checkbox_container);
  }

  // Holds the "add gallery" and cancel/confirm buttons.
  GtkWidget* add_folder_area = gtk_hbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(contents_.get()), add_folder_area,
                     FALSE, FALSE, 0);

  // Add gallery button.
  GtkWidget* add_folder = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_MEDIA_GALLERIES_DIALOG_ADD_GALLERY).c_str());
  g_signal_connect(add_folder, "clicked", G_CALLBACK(OnAddFolderThunk), this);
  gtk_box_pack_start(GTK_BOX(add_folder_area), add_folder, FALSE, FALSE, 0);

  // Confirm/cancel button.
  GtkWidget* bottom_area = gtk_hbox_new(FALSE, ui::kControlSpacing);
  confirm_ = gtk_button_new_with_label(l10n_util::GetStringUTF8(
      IDS_MEDIA_GALLERIES_DIALOG_CONFIRM).c_str());
  gtk_button_set_image(
      GTK_BUTTON(confirm_),
      gtk_image_new_from_stock(GTK_STOCK_APPLY, GTK_ICON_SIZE_BUTTON));
  g_signal_connect(confirm_, "clicked", G_CALLBACK(OnConfirmThunk), this);
  gtk_box_pack_end(GTK_BOX(bottom_area), confirm_, FALSE, FALSE, 0);

  GtkWidget* cancel = gtk_button_new_with_label(l10n_util::GetStringUTF8(
      IDS_MEDIA_GALLERIES_DIALOG_CANCEL).c_str());
  gtk_button_set_image(
      GTK_BUTTON(cancel),
      gtk_image_new_from_stock(GTK_STOCK_CANCEL, GTK_ICON_SIZE_BUTTON));
  g_signal_connect(cancel, "clicked", G_CALLBACK(OnCancelThunk), this);
  gtk_box_pack_end(GTK_BOX(bottom_area), cancel, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(contents_.get()), bottom_area, FALSE, FALSE, 0);

  // As a safeguard against the user skipping reading over the dialog and just
  // confirming, the button will be unavailable for dialogs without any checks
  // until the user toggles something.
  gtk_widget_set_sensitive(confirm_, controller_->HasPermittedGalleries());

  gtk_widget_show_all(contents_.get());
}

void MediaGalleriesDialogGtk::UpdateGallery(
    const MediaGalleryPrefInfo& gallery,
    bool permitted) {
  InitWidgets();
}

void MediaGalleriesDialogGtk::UpdateGalleryInContainer(
    const MediaGalleryPrefInfo& gallery,
    bool permitted,
    GtkWidget* checkbox_container) {
  GtkWidget* hbox = gtk_hbox_new(FALSE, ui::kLabelSpacing);
  GtkWidget* widget = gtk_check_button_new();
  g_signal_connect(widget, "toggled", G_CALLBACK(OnToggledThunk), this);
  gtk_box_pack_start(GTK_BOX(checkbox_container), hbox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 0);
  std::string details = UTF16ToUTF8(
      MediaGalleriesDialogController::GetGalleryAdditionalDetails(gallery));
  GtkWidget* details_label = gtk_label_new(details.c_str());
  gtk_label_set_line_wrap(GTK_LABEL(details_label), FALSE);
  gtk_util::SetLabelColor(details_label, &kDeemphasizedTextColor);
  gtk_box_pack_start(GTK_BOX(hbox), details_label, FALSE, FALSE, 0);

  gtk_widget_show(hbox);
  checkbox_map_[gallery.pref_id] = widget;

  std::string tooltip_text = UTF16ToUTF8(
      MediaGalleriesDialogController::GetGalleryTooltip(gallery));
  gtk_widget_set_tooltip_text(widget, tooltip_text.c_str());

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), permitted);
  std::string label = UTF16ToUTF8(
      MediaGalleriesDialogController::GetGalleryDisplayNameNoAttachment(
          gallery));
  gtk_button_set_label(GTK_BUTTON(widget), label.c_str());
}

void MediaGalleriesDialogGtk::ForgetGallery(MediaGalleryPrefId gallery) {
  for (CheckboxMap::iterator iter = checkbox_map_.begin();
       iter != checkbox_map_.end(); ++iter) {
    if (iter->first == gallery) {
      GtkWidget* checkbox = iter->second;
      checkbox_map_.erase(iter);
      gtk_widget_destroy(gtk_widget_get_parent(checkbox));
      return;
    }
  }
}

void MediaGalleriesDialogGtk::OnToggled(GtkWidget* widget) {
  if (confirm_)
    gtk_widget_set_sensitive(confirm_, TRUE);

  for (CheckboxMap::const_iterator iter = checkbox_map_.begin();
       iter != checkbox_map_.end(); ++iter) {
    if (iter->second == widget) {
      controller_->DidToggleGalleryId(
          iter->first,
          gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
    }
  }
}

void MediaGalleriesDialogGtk::OnAddFolder(GtkWidget* widget) {
  controller_->OnAddFolderClicked();
}

void MediaGalleriesDialogGtk::OnConfirm(GtkWidget* widget) {
  accepted_ = true;

  if (window_)
    gtk_widget_destroy(window_);
}

void MediaGalleriesDialogGtk::OnCancel(GtkWidget* widget) {
  if (window_)
    gtk_widget_destroy(window_);
}

void MediaGalleriesDialogGtk::OnDestroy(GtkWidget* widget) {
  controller_->DialogFinished(accepted_);
}

// MediaGalleriesDialogController ----------------------------------------------

// static
MediaGalleriesDialog* MediaGalleriesDialog::Create(
    MediaGalleriesDialogController* controller) {
  return new MediaGalleriesDialogGtk(controller);
}

}  // namespace chrome
