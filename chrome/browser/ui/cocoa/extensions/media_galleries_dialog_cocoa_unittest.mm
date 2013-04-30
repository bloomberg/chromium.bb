// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller_mock.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/browser/ui/cocoa/extensions/media_galleries_dialog_cocoa.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnPointee;
using ::testing::ReturnRef;

namespace chrome {

MediaGalleryPrefInfo MakePrefInfoForTesting(MediaGalleryPrefId pref_id) {
  MediaGalleryPrefInfo gallery;
  gallery.pref_id = pref_id;
  gallery.device_id =
      MediaStorageUtil::MakeDeviceId(MediaStorageUtil::FIXED_MASS_STORAGE,
                                     base::Int64ToString(pref_id));
  gallery.display_name = ASCIIToUTF16("name");
  return gallery;
}

class MediaGalleriesDialogTest : public testing::Test {
};

// Tests that checkboxes are initialized according to the contents of
// permissions().
TEST_F(MediaGalleriesDialogTest, InitializeCheckboxes) {
  NiceMock<MediaGalleriesDialogControllerMock> controller;

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

  // Initializing checkboxes should not cause them to be toggled.
  EXPECT_CALL(controller, DidToggleGalleryId(_, _)).
      Times(0);

  scoped_ptr<MediaGalleriesDialogCocoa> dialog(
      static_cast<MediaGalleriesDialogCocoa*>(
          MediaGalleriesDialog::Create(&controller)));
  EXPECT_EQ(2U, [dialog->checkboxes_ count]);

  NSButton* checkbox1 = [dialog->checkboxes_ objectAtIndex:0];
  EXPECT_EQ([checkbox1 state], NSOnState);

  NSButton* checkbox2 = [dialog->checkboxes_ objectAtIndex:1];
  EXPECT_EQ([checkbox2 state], NSOffState);
}

// Tests that toggling checkboxes updates the controller.
TEST_F(MediaGalleriesDialogTest, ToggleCheckboxes) {
  NiceMock<MediaGalleriesDialogControllerMock> controller;

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

  scoped_ptr<MediaGalleriesDialogCocoa> dialog(
      static_cast<MediaGalleriesDialogCocoa*>(
          MediaGalleriesDialog::Create(&controller)));
  EXPECT_EQ(1U, [dialog->checkboxes_ count]);

  NSButton* checkbox = [dialog->checkboxes_ objectAtIndex:0];
  EXPECT_EQ([checkbox state], NSOnState);

  EXPECT_CALL(controller, DidToggleGalleryId(1, false));
  [checkbox performClick:nil];
  EXPECT_EQ([checkbox state], NSOffState);

  EXPECT_CALL(controller, DidToggleGalleryId(1, true));
  [checkbox performClick:nil];
  EXPECT_EQ([checkbox state], NSOnState);
}

// Tests that UpdateGallery will add a new checkbox, but only if it refers to
// a gallery that the dialog hasn't seen before.
TEST_F(MediaGalleriesDialogTest, UpdateAdds) {
  NiceMock<MediaGalleriesDialogControllerMock> controller;

  MediaGalleriesDialogController::GalleryPermissionsVector attached_permissions;
  EXPECT_CALL(controller, AttachedPermissions()).
      WillRepeatedly(ReturnPointee(&attached_permissions));

  MediaGalleriesDialogController::GalleryPermissionsVector
      unattached_permissions;
  EXPECT_CALL(controller, UnattachedPermissions()).
      WillRepeatedly(Return(unattached_permissions));

  scoped_ptr<MediaGalleriesDialogCocoa> dialog(
      static_cast<MediaGalleriesDialogCocoa*>(
          MediaGalleriesDialog::Create(&controller)));

  EXPECT_EQ(0U, [dialog->checkboxes_ count]);
  CGFloat old_container_height = NSHeight([dialog->checkbox_container_ frame]);

  attached_permissions.push_back(
      MediaGalleriesDialogController::GalleryPermission(
          MakePrefInfoForTesting(1), true));
  dialog->UpdateGallery(MakePrefInfoForTesting(1), true);
  EXPECT_EQ(1U, [dialog->checkboxes_ count]);

  // The checkbox container should be taller.
  CGFloat new_container_height = NSHeight([dialog->checkbox_container_ frame]);
  EXPECT_GT(new_container_height, old_container_height);
  old_container_height = new_container_height;

  attached_permissions.push_back(
      MediaGalleriesDialogController::GalleryPermission(
          MakePrefInfoForTesting(2), true));
  dialog->UpdateGallery(MakePrefInfoForTesting(2), true);
  EXPECT_EQ(2U, [dialog->checkboxes_ count]);

  // The checkbox container should be taller.
  new_container_height = NSHeight([dialog->checkbox_container_ frame]);
  EXPECT_GT(new_container_height, old_container_height);
  old_container_height = new_container_height;

  attached_permissions[1].allowed = false;
  dialog->UpdateGallery(MakePrefInfoForTesting(2), false);
  EXPECT_EQ(2U, [dialog->checkboxes_ count]);

  // The checkbox container height should not have changed.
  new_container_height = NSHeight([dialog->checkbox_container_ frame]);
  EXPECT_EQ(new_container_height, old_container_height);
}

TEST_F(MediaGalleriesDialogTest, ForgetDeletes) {
  NiceMock<MediaGalleriesDialogControllerMock> controller;

  MediaGalleriesDialogController::GalleryPermissionsVector attached_permissions;
  EXPECT_CALL(controller, AttachedPermissions()).
      WillRepeatedly(ReturnPointee(&attached_permissions));

  MediaGalleriesDialogController::GalleryPermissionsVector
      unattached_permissions;
  EXPECT_CALL(controller, UnattachedPermissions()).
      WillRepeatedly(Return(unattached_permissions));

  scoped_ptr<MediaGalleriesDialogCocoa> dialog(
      static_cast<MediaGalleriesDialogCocoa*>(
          MediaGalleriesDialog::Create(&controller)));

  // Add a couple of galleries.
  attached_permissions.push_back(
      MediaGalleriesDialogController::GalleryPermission(
          MakePrefInfoForTesting(1), true));
  dialog->UpdateGallery(MakePrefInfoForTesting(1), true);
  attached_permissions.push_back(
      MediaGalleriesDialogController::GalleryPermission(
          MakePrefInfoForTesting(2), true));
  dialog->UpdateGallery(MakePrefInfoForTesting(2), true);
  EXPECT_EQ(2U, [dialog->checkboxes_ count]);
  CGFloat old_container_height = NSHeight([dialog->checkbox_container_ frame]);

  // Remove a gallery.
  attached_permissions.erase(attached_permissions.begin());
  dialog->ForgetGallery(1);
  EXPECT_EQ(1U, [dialog->checkboxes_ count]);

  // The checkbox container should be shorter.
  CGFloat new_container_height = NSHeight([dialog->checkbox_container_ frame]);
  EXPECT_LT(new_container_height, old_container_height);
}

}  // namespace chrome
