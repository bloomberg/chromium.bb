// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller_mock.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "chrome/browser/ui/cocoa/extensions/media_galleries_dialog_cocoa.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

namespace chrome {

MediaGalleryPrefInfo MakePrefInfoForTesting(MediaGalleryPrefId pref_id) {
  MediaGalleryPrefInfo gallery;
  gallery.pref_id = pref_id;
  gallery.device_id =
      MediaStorageUtil::MakeDeviceId(MediaStorageUtil::FIXED_MASS_STORAGE,
                                     base::Int64ToString(pref_id));
  return gallery;
}

class MediaGalleriesDialogTest : public testing::Test {
};

// Tests that checkboxes are initialized according to the contents of
// permissions().
TEST_F(MediaGalleriesDialogTest, InitializeCheckboxes) {
  NiceMock<MediaGalleriesDialogControllerMock> controller;

  MediaGalleriesDialogController::KnownGalleryPermissions permissions;
  MediaGalleryPrefInfo gallery1 = MakePrefInfoForTesting(1);
  permissions[1] = MediaGalleriesDialogController::GalleryPermission(
      gallery1, true);
  MediaGalleryPrefInfo gallery2 = MakePrefInfoForTesting(2);
  permissions[2] = MediaGalleriesDialogController::GalleryPermission(
      gallery2, false);
  EXPECT_CALL(controller, permissions()).
      WillRepeatedly(ReturnRef(permissions));

  // Initializing checkboxes should not cause them to be toggled.
  EXPECT_CALL(controller, DidToggleGallery(_, _)).
      Times(0);

  scoped_ptr<MediaGalleriesDialogCocoa> dialog(
      static_cast<MediaGalleriesDialogCocoa*>(
          MediaGalleriesDialog::Create(&controller)));
  EXPECT_EQ(2U, [dialog->checkboxes_ count]);

  // Note that checkboxes_ is sorted from bottom up.
  NSButton* checkbox1 = [dialog->checkboxes_ objectAtIndex:1];
  EXPECT_EQ([checkbox1 state], NSOnState);

  NSButton* checkbox2 = [dialog->checkboxes_ objectAtIndex:0];
  EXPECT_EQ([checkbox2 state], NSOffState);
}

// Tests that toggling checkboxes updates the controller.
TEST_F(MediaGalleriesDialogTest, ToggleCheckboxes) {
  NiceMock<MediaGalleriesDialogControllerMock> controller;

  MediaGalleriesDialogController::KnownGalleryPermissions permissions;
  MediaGalleryPrefInfo gallery = MakePrefInfoForTesting(1);
  permissions[1] = MediaGalleriesDialogController::GalleryPermission(
      gallery, true);
  EXPECT_CALL(controller, permissions()).
      WillRepeatedly(ReturnRef(permissions));

  scoped_ptr<MediaGalleriesDialogCocoa> dialog(
      static_cast<MediaGalleriesDialogCocoa*>(
          MediaGalleriesDialog::Create(&controller)));
  EXPECT_EQ(1U, [dialog->checkboxes_ count]);

  NSButton* checkbox = [dialog->checkboxes_ objectAtIndex:0];
  EXPECT_EQ([checkbox state], NSOnState);

  EXPECT_CALL(controller, DidToggleGallery(_, false));
  [checkbox performClick:nil];
  EXPECT_EQ([checkbox state], NSOffState);

  EXPECT_CALL(controller, DidToggleGallery(_, true));
  [checkbox performClick:nil];
  EXPECT_EQ([checkbox state], NSOnState);
}

// Tests that UpdateGallery will add a new checkbox, but only if it refers to
// a gallery that the dialog hasn't seen before.
TEST_F(MediaGalleriesDialogTest, UpdateAdds) {
  NiceMock<MediaGalleriesDialogControllerMock> controller;

  MediaGalleriesDialogController::KnownGalleryPermissions permissions;
  EXPECT_CALL(controller, permissions()).
      WillRepeatedly(ReturnRef(permissions));

  scoped_ptr<MediaGalleriesDialogCocoa> dialog(
      static_cast<MediaGalleriesDialogCocoa*>(
          MediaGalleriesDialog::Create(&controller)));

  EXPECT_EQ(0U, [dialog->checkboxes_ count]);
  CGFloat old_container_height = NSHeight([dialog->checkbox_container_ frame]);

  MediaGalleryPrefInfo gallery1 = MakePrefInfoForTesting(1);
  dialog->UpdateGallery(&gallery1, true);
  EXPECT_EQ(1U, [dialog->checkboxes_ count]);

  // The checkbox container should be taller.
  CGFloat new_container_height = NSHeight([dialog->checkbox_container_ frame]);
  EXPECT_GT(new_container_height, old_container_height);
  old_container_height = new_container_height;

  MediaGalleryPrefInfo gallery2 = MakePrefInfoForTesting(2);
  dialog->UpdateGallery(&gallery2, true);
  EXPECT_EQ(2U, [dialog->checkboxes_ count]);

  // The checkbox container should be taller.
  new_container_height = NSHeight([dialog->checkbox_container_ frame]);
  EXPECT_GT(new_container_height, old_container_height);
  old_container_height = new_container_height;

  dialog->UpdateGallery(&gallery2, false);
  EXPECT_EQ(2U, [dialog->checkboxes_ count]);

  // The checkbox container height should not have changed.
  new_container_height = NSHeight([dialog->checkbox_container_ frame]);
  EXPECT_EQ(new_container_height, old_container_height);
}

TEST_F(MediaGalleriesDialogTest, ForgetDeletes) {
  NiceMock<MediaGalleriesDialogControllerMock> controller;

  MediaGalleriesDialogController::KnownGalleryPermissions permissions;
  EXPECT_CALL(controller, permissions()).
      WillRepeatedly(ReturnRef(permissions));

  scoped_ptr<MediaGalleriesDialogCocoa> dialog(
      static_cast<MediaGalleriesDialogCocoa*>(
          MediaGalleriesDialog::Create(&controller)));

  // Add a couple of galleries.
  MediaGalleryPrefInfo gallery1 = MakePrefInfoForTesting(1);
  dialog->UpdateGallery(&gallery1, true);
  MediaGalleryPrefInfo gallery2 = MakePrefInfoForTesting(2);
  dialog->UpdateGallery(&gallery2, true);
  EXPECT_EQ(2U, [dialog->checkboxes_ count]);
  CGFloat old_container_height = NSHeight([dialog->checkbox_container_ frame]);

  // Remove a gallery.
  dialog->ForgetGallery(&gallery1);
  EXPECT_EQ(1U, [dialog->checkboxes_ count]);

  // The checkbox container should be shorter.
  CGFloat new_container_height = NSHeight([dialog->checkbox_container_ frame]);
  EXPECT_LT(new_container_height, old_container_height);
}

}  // namespace chrome
