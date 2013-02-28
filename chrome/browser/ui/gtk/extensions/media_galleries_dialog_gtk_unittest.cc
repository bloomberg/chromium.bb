// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller_mock.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/browser/ui/gtk/extensions/media_galleries_dialog_gtk.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::ReturnRef;

namespace chrome {

namespace {

MediaGalleryPrefInfo MakePrefInfoForTesting(MediaGalleryPrefId id) {
  MediaGalleryPrefInfo gallery;
  gallery.pref_id = id;
  gallery.device_id =
      MediaStorageUtil::MakeDeviceId(MediaStorageUtil::FIXED_MASS_STORAGE,
                                     base::Int64ToString(id));
  return gallery;
}

}  // namespace

class MediaGalleriesDialogTest : public testing::Test {
};

// Tests that checkboxes are initialized according to the contents of
// permissions().
TEST_F(MediaGalleriesDialogTest, InitializeCheckboxes) {
  NiceMock<MediaGalleriesDialogControllerMock> controller;

  MediaGalleriesDialogController::KnownGalleryPermissions permissions;
  MediaGalleryPrefInfo gallery1 = MakePrefInfoForTesting(1);
  permissions[1] =
      MediaGalleriesDialogController::GalleryPermission(gallery1, true);
  MediaGalleryPrefInfo gallery2 = MakePrefInfoForTesting(2);
  permissions[2] =
      MediaGalleriesDialogController::GalleryPermission(gallery2, false);
  EXPECT_CALL(controller, permissions()).
      WillRepeatedly(ReturnRef(permissions));

  // Initializing checkboxes should not cause them to be toggled.
  EXPECT_CALL(controller, DidToggleGallery(_, _)).
      Times(0);

  MediaGalleriesDialogGtk dialog(&controller);
  EXPECT_EQ(2U, dialog.checkbox_map_.size());

  GtkWidget* checkbox1 = dialog.checkbox_map_[&permissions[1].pref_info];
  ASSERT_TRUE(GTK_IS_TOGGLE_BUTTON(checkbox1));
  EXPECT_TRUE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox1)));

  GtkWidget* checkbox2 = dialog.checkbox_map_[&permissions[2].pref_info];
  ASSERT_TRUE(GTK_IS_TOGGLE_BUTTON(checkbox2));
  EXPECT_FALSE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox2)));
}

// Tests that toggling checkboxes updates the controller.
TEST_F(MediaGalleriesDialogTest, ToggleCheckboxes) {
  NiceMock<MediaGalleriesDialogControllerMock> controller;

  MediaGalleriesDialogController::KnownGalleryPermissions permissions;
  MediaGalleryPrefInfo gallery1 = MakePrefInfoForTesting(1);
  permissions[1] =
      MediaGalleriesDialogController::GalleryPermission(gallery1, true);
  EXPECT_CALL(controller, permissions()).
      WillRepeatedly(ReturnRef(permissions));

  MediaGalleriesDialogGtk dialog(&controller);
  EXPECT_EQ(1U, dialog.checkbox_map_.size());
  GtkWidget* checkbox =
      dialog.checkbox_map_[&permissions[1].pref_info];
  ASSERT_TRUE(GTK_IS_TOGGLE_BUTTON(checkbox));
  EXPECT_TRUE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox)));

  EXPECT_CALL(controller, DidToggleGallery(_, false));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), FALSE);

  EXPECT_CALL(controller, DidToggleGallery(_, true));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), TRUE);
}

// Tests that UpdateGallery will add a new checkbox, but only if it refers to
// a gallery that the dialog hasn't seen before.
TEST_F(MediaGalleriesDialogTest, UpdateAdds) {
  NiceMock<MediaGalleriesDialogControllerMock> controller;

  MediaGalleriesDialogController::KnownGalleryPermissions permissions;
  EXPECT_CALL(controller, permissions()).
      WillRepeatedly(ReturnRef(permissions));

  MediaGalleriesDialogGtk dialog(&controller);

  EXPECT_TRUE(dialog.checkbox_map_.empty());

  MediaGalleryPrefInfo gallery1 = MakePrefInfoForTesting(1);
  dialog.UpdateGallery(&gallery1, true);
  EXPECT_EQ(1U, dialog.checkbox_map_.size());

  MediaGalleryPrefInfo gallery2 = MakePrefInfoForTesting(2);
  dialog.UpdateGallery(&gallery2, true);
  EXPECT_EQ(2U, dialog.checkbox_map_.size());

  dialog.UpdateGallery(&gallery2, false);
  EXPECT_EQ(2U, dialog.checkbox_map_.size());
}

TEST_F(MediaGalleriesDialogTest, ForgetDeletes) {
  NiceMock<MediaGalleriesDialogControllerMock> controller;

  MediaGalleriesDialogController::KnownGalleryPermissions permissions;
  EXPECT_CALL(controller, permissions()).
      WillRepeatedly(ReturnRef(permissions));

  MediaGalleriesDialogGtk dialog(&controller);

  EXPECT_TRUE(dialog.checkbox_map_.empty());

  MediaGalleryPrefInfo gallery1 = MakePrefInfoForTesting(1);
  dialog.UpdateGallery(&gallery1, true);
  EXPECT_EQ(1U, dialog.checkbox_map_.size());

  MediaGalleryPrefInfo gallery2 = MakePrefInfoForTesting(2);
  dialog.UpdateGallery(&gallery2, true);
  EXPECT_EQ(2U, dialog.checkbox_map_.size());

  dialog.ForgetGallery(&gallery2);
  EXPECT_EQ(1U, dialog.checkbox_map_.size());
}

}  // namespace chrome
