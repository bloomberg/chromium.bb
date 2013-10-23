// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller_mock.h"
#include "chrome/browser/storage_monitor/storage_info.h"
#include "chrome/browser/ui/gtk/extensions/media_galleries_dialog_gtk.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnPointee;
using ::testing::ReturnRef;

namespace {

MediaGalleryPrefInfo MakePrefInfoForTesting(MediaGalleryPrefId id) {
  MediaGalleryPrefInfo gallery;
  gallery.pref_id = id;
  gallery.device_id = StorageInfo::MakeDeviceId(StorageInfo::FIXED_MASS_STORAGE,
                                                base::Int64ToString(id));
  gallery.display_name = ASCIIToUTF16("Display Name");
  return gallery;
}

}  // namespace

class MediaGalleriesDialogTest : public testing::Test {
 public:
  MediaGalleriesDialogTest() {}
  virtual ~MediaGalleriesDialogTest() {}
  virtual void SetUp() OVERRIDE {
    dummy_extension_ = extension_test_util::CreateExtensionWithID("dummy");
  }
  virtual void TearDown() OVERRIDE {
    dummy_extension_ = NULL;
  }

  const extensions::Extension& dummy_extension() const {
    return *dummy_extension_;
  }

 private:
  scoped_refptr<extensions::Extension> dummy_extension_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesDialogTest);
};

// Tests that checkboxes are initialized according to the contents of
// permissions in the registry.
TEST_F(MediaGalleriesDialogTest, InitializeCheckboxes) {
  // TODO(gbillock): Get rid of this mock; make something specialized.
  NiceMock<MediaGalleriesDialogControllerMock> controller(dummy_extension());

  MediaGalleriesDialogController::GalleryPermissionsVector attached_permissions;
  attached_permissions.push_back(
      MediaGalleriesDialogController::GalleryPermission(
          MakePrefInfoForTesting(1), true));
  attached_permissions.push_back(
      MediaGalleriesDialogController::GalleryPermission(
          MakePrefInfoForTesting(2), false));
  EXPECT_CALL(controller, AttachedPermissions()).
      WillRepeatedly(Return(attached_permissions));

  MediaGalleriesDialogController::GalleryPermissionsVector
      unattached_permissions;
  EXPECT_CALL(controller, UnattachedPermissions()).
      WillRepeatedly(Return(unattached_permissions));

  MediaGalleriesDialogGtk dialog(&controller);
  EXPECT_EQ(2U, dialog.checkbox_map_.size());

  GtkWidget* checkbox1 = dialog.checkbox_map_[1];
  ASSERT_TRUE(GTK_IS_TOGGLE_BUTTON(checkbox1));
  EXPECT_TRUE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox1)));

  GtkWidget* checkbox2 = dialog.checkbox_map_[2];
  ASSERT_TRUE(GTK_IS_TOGGLE_BUTTON(checkbox2));
  EXPECT_FALSE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox2)));
}

// Tests that toggling checkboxes updates the controller.
TEST_F(MediaGalleriesDialogTest, ToggleCheckboxes) {
  NiceMock<MediaGalleriesDialogControllerMock> controller(dummy_extension());

  MediaGalleriesDialogController::GalleryPermissionsVector attached_permissions;
  attached_permissions.push_back(
      MediaGalleriesDialogController::GalleryPermission(
          MakePrefInfoForTesting(1), true));
  EXPECT_CALL(controller, AttachedPermissions()).
      WillRepeatedly(Return(attached_permissions));

  MediaGalleriesDialogController::GalleryPermissionsVector
      unattached_permissions;
  EXPECT_CALL(controller, UnattachedPermissions()).
      WillRepeatedly(Return(unattached_permissions));

  MediaGalleriesDialogGtk dialog(&controller);
  EXPECT_EQ(1U, dialog.checkbox_map_.size());
  GtkWidget* checkbox =
      dialog.checkbox_map_[1];
  ASSERT_TRUE(GTK_IS_TOGGLE_BUTTON(checkbox));
  EXPECT_TRUE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox)));

  EXPECT_CALL(controller, DidToggleGalleryId(1, false));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), FALSE);

  EXPECT_CALL(controller, DidToggleGalleryId(1, true));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), TRUE);
}

// Tests that UpdateGallery will add a new checkbox, but only if it refers to
// a gallery that the dialog hasn't seen before.
TEST_F(MediaGalleriesDialogTest, UpdateAdds) {
  NiceMock<MediaGalleriesDialogControllerMock> controller(dummy_extension());

  MediaGalleriesDialogController::GalleryPermissionsVector attached_permissions;
  EXPECT_CALL(controller, AttachedPermissions()).
      WillRepeatedly(ReturnPointee(&attached_permissions));

  MediaGalleriesDialogController::GalleryPermissionsVector
      unattached_permissions;
  EXPECT_CALL(controller, UnattachedPermissions()).
      WillRepeatedly(Return(unattached_permissions));

  MediaGalleriesDialogGtk dialog(&controller);

  EXPECT_TRUE(dialog.checkbox_map_.empty());

  MediaGalleryPrefInfo gallery1 = MakePrefInfoForTesting(1);
  attached_permissions.push_back(
      MediaGalleriesDialogController::GalleryPermission(gallery1, true));
  dialog.UpdateGalleries();
  EXPECT_EQ(1U, dialog.checkbox_map_.size());

  MediaGalleryPrefInfo gallery2 = MakePrefInfoForTesting(2);
  attached_permissions.push_back(
      MediaGalleriesDialogController::GalleryPermission(gallery2, true));
  dialog.UpdateGalleries();
  EXPECT_EQ(2U, dialog.checkbox_map_.size());

  attached_permissions.push_back(
      MediaGalleriesDialogController::GalleryPermission(gallery2, false));
  dialog.UpdateGalleries();
  EXPECT_EQ(2U, dialog.checkbox_map_.size());
}

TEST_F(MediaGalleriesDialogTest, ForgetDeletes) {
  NiceMock<MediaGalleriesDialogControllerMock> controller(dummy_extension());

  MediaGalleriesDialogController::GalleryPermissionsVector attached_permissions;
  EXPECT_CALL(controller, AttachedPermissions()).
      WillRepeatedly(ReturnPointee(&attached_permissions));

  MediaGalleriesDialogController::GalleryPermissionsVector
      unattached_permissions;
  EXPECT_CALL(controller, UnattachedPermissions()).
      WillRepeatedly(Return(unattached_permissions));

  MediaGalleriesDialogGtk dialog(&controller);

  EXPECT_TRUE(dialog.checkbox_map_.empty());

  MediaGalleryPrefInfo gallery1 = MakePrefInfoForTesting(1);
  attached_permissions.push_back(
      MediaGalleriesDialogController::GalleryPermission(gallery1, true));
  dialog.UpdateGalleries();
  EXPECT_EQ(1U, dialog.checkbox_map_.size());

  MediaGalleryPrefInfo gallery2 = MakePrefInfoForTesting(2);
  attached_permissions.push_back(
      MediaGalleriesDialogController::GalleryPermission(gallery2, true));
  dialog.UpdateGalleries();
  EXPECT_EQ(2U, dialog.checkbox_map_.size());

  attached_permissions.pop_back();
  dialog.UpdateGalleries();
  EXPECT_EQ(1U, dialog.checkbox_map_.size());
}
