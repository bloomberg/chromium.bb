// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/extensions/media_galleries_dialog_gtk.h"

#include "base/auto_reset.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace chrome {

typedef MediaGalleriesDialogController::KnownGalleryPermissions
    GalleryPermissions;

MediaGalleriesDialogGtk::MediaGalleriesDialogGtk(
    MediaGalleriesDialogController* controller)
      : controller_(controller),
        window_(NULL),
        confirm_(NULL),
        checkbox_container_(NULL),
        ignore_toggles_(false),
        accepted_(false) {
  InitWidgets();

  // May be NULL during tests.
  if (controller->web_contents())
    window_ = new ConstrainedWindowGtk(controller->web_contents(), this);
}

MediaGalleriesDialogGtk::~MediaGalleriesDialogGtk() {
}

void MediaGalleriesDialogGtk::InitWidgets() {
  contents_.Own(gtk_vbox_new(FALSE, ui::kContentAreaSpacing));

  GtkWidget* header =
      gtk_util::CreateBoldLabel(UTF16ToUTF8(controller_->GetHeader()));
  gtk_box_pack_start(GTK_BOX(contents_.get()), header, FALSE, FALSE, 0);

  GtkWidget* subtext =
      gtk_label_new(UTF16ToUTF8(controller_->GetSubtext()).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(subtext), TRUE);
  gtk_widget_set_size_request(subtext, 500, -1);
  gtk_box_pack_start(GTK_BOX(contents_.get()), subtext, FALSE, FALSE, 0);

  checkbox_container_ = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(contents_.get()), checkbox_container_,
                     FALSE, FALSE, 0);

  const GalleryPermissions& permissions = controller_->permissions();
  for (GalleryPermissions::const_iterator iter = permissions.begin();
       iter != permissions.end(); ++iter) {
    UpdateGallery(&iter->second.pref_info, iter->second.allowed);
  }

  // Holds the "add gallery" and cancel/confirm buttons.
  GtkWidget* bottom_area = gtk_hbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(contents_.get()), bottom_area, FALSE, FALSE, 0);

  // Add gallery button.
  GtkWidget* add_folder = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_MEDIA_GALLERIES_DIALOG_ADD_GALLERY).c_str());
  g_signal_connect(add_folder, "clicked", G_CALLBACK(OnAddFolderThunk), this);
  gtk_box_pack_start(GTK_BOX(bottom_area), add_folder, FALSE, FALSE, 0);

  // Confirm/cancel button.
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

  // As a safeguard against the user skipping reading over the dialog and just
  // confirming, the button will be unavailable for dialogs without any checks
  // until the user toggles something.
  gtk_widget_set_sensitive(confirm_, controller_->HasPermittedGalleries());
}

void MediaGalleriesDialogGtk::UpdateGallery(
    const MediaGalleryPrefInfo* gallery,
    bool permitted) {
  CheckboxMap::iterator iter = checkbox_map_.find(gallery);

  GtkWidget* widget = NULL;
  if (iter != checkbox_map_.end()) {
    widget = iter->second;
  } else {
    widget = gtk_check_button_new();
    g_signal_connect(widget, "toggled", G_CALLBACK(OnToggledThunk), this);
    gtk_box_pack_start(GTK_BOX(checkbox_container_), widget, FALSE, FALSE, 0);
    gtk_widget_show(widget);
    checkbox_map_[gallery] = widget;
  }

  gtk_widget_set_tooltip_text(widget, UTF16ToUTF8(
      MediaGalleriesDialogController::GetGalleryTooltip(*gallery)).c_str());

  base::AutoReset<bool> reset(&ignore_toggles_, true);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), permitted);
  std::string label_text = UTF16ToUTF8(
      MediaGalleriesDialogController::GetGalleryDisplayName(*gallery));
  gtk_button_set_label(GTK_BUTTON(widget), label_text.c_str());
}

void MediaGalleriesDialogGtk::ForgetGallery(
    const MediaGalleryPrefInfo* gallery) {
  CheckboxMap::iterator iter = checkbox_map_.find(gallery);
  if (iter == checkbox_map_.end())
    return;

  base::AutoReset<bool> reset(&ignore_toggles_, true);
  gtk_widget_destroy(iter->second);
  checkbox_map_.erase(iter);
}

GtkWidget* MediaGalleriesDialogGtk::GetWidgetRoot() {
  return contents_.get();
}

GtkWidget* MediaGalleriesDialogGtk::GetFocusWidget() {
  return confirm_;
}

void MediaGalleriesDialogGtk::DeleteDelegate() {
  controller_->DialogFinished(accepted_);
}

void MediaGalleriesDialogGtk::OnToggled(GtkWidget* widget) {
  if (confirm_)
    gtk_widget_set_sensitive(confirm_, TRUE);

  if (ignore_toggles_)
    return;

  for (CheckboxMap::const_iterator iter = checkbox_map_.begin();
       iter != checkbox_map_.end(); ++iter) {
    if (iter->second == widget) {
      controller_->DidToggleGallery(
          iter->first, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
      return;
    }
  }

  NOTREACHED();
  return;
}

void MediaGalleriesDialogGtk::OnAddFolder(GtkWidget* widget) {
  controller_->OnAddFolderClicked();
}

void MediaGalleriesDialogGtk::OnConfirm(GtkWidget* widget) {
  accepted_ = true;
  window_->CloseWebContentsModalDialog();
}

void MediaGalleriesDialogGtk::OnCancel(GtkWidget* widget) {
  window_->CloseWebContentsModalDialog();
}

// MediaGalleriesDialogController ----------------------------------------------

// static
MediaGalleriesDialog* MediaGalleriesDialog::Create(
    MediaGalleriesDialogController* controller) {
  return new MediaGalleriesDialogGtk(controller);
}

}  // namespace chrome
