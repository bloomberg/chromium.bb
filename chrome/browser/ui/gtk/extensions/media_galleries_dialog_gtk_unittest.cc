// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_gallery/media_galleries_dialog_controller.h"
#include "chrome/browser/ui/gtk/extensions/media_galleries_dialog_gtk.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

class MediaGalleriesDialogTest : public testing::Test,
                                 public MediaGalleriesDialogController {
 public:
  MediaGalleriesDialogTest() : toggles_(0) {}
  virtual ~MediaGalleriesDialogTest() {}

  virtual void DialogFinished(bool accepted) OVERRIDE {}
  virtual void DidToggleGallery(const MediaGalleryPrefInfo* pref_info,
                                bool enabled) OVERRIDE {
    toggles_++;
    MediaGalleriesDialogController::DidToggleGallery(pref_info, enabled);
  }

 protected:
  // Counter that tracks the number of times a checkbox has been toggled.
  size_t toggles_;
};

// Tests that checkboxes are initialized according to the contents of
// |known_galleries|.
TEST_F(MediaGalleriesDialogTest, InitializeCheckboxes) {
  MediaGalleryPrefInfo gallery1;
  gallery1.pref_id = 1;
  known_galleries_[1] = GalleryPermission(gallery1, true);
  MediaGalleryPrefInfo gallery2;
  gallery2.pref_id = 2;
  known_galleries_[2] = GalleryPermission(gallery2, false);

  scoped_ptr<MediaGalleriesDialogGtk>
      dialog_(new MediaGalleriesDialogGtk(this));
  EXPECT_EQ(2U, dialog_->checkbox_map_.size());

  GtkWidget* checkbox1 = dialog_->checkbox_map_[&known_galleries_[1].pref_info];
  ASSERT_TRUE(GTK_IS_TOGGLE_BUTTON(checkbox1));
  EXPECT_TRUE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox1)));

  GtkWidget* checkbox2 = dialog_->checkbox_map_[&known_galleries_[2].pref_info];
  ASSERT_TRUE(GTK_IS_TOGGLE_BUTTON(checkbox2));
  EXPECT_FALSE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox2)));

  // Initializing checkboxes should not cause them to be toggled.
  EXPECT_EQ(0U, toggles_);
}

// Tests that toggling checkboxes updates the controller.
TEST_F(MediaGalleriesDialogTest, ToggleCheckboxes) {
  MediaGalleryPrefInfo gallery1;
  gallery1.pref_id = 1;
  known_galleries_[1] = GalleryPermission(gallery1, true);

  scoped_ptr<MediaGalleriesDialogGtk>
      dialog_(new MediaGalleriesDialogGtk(this));
  EXPECT_EQ(1U, dialog_->checkbox_map_.size());
  GtkWidget* checkbox =
      dialog_->checkbox_map_[&known_galleries_[1].pref_info];
  ASSERT_TRUE(GTK_IS_TOGGLE_BUTTON(checkbox));
  EXPECT_TRUE(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox)));

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), FALSE);
  EXPECT_FALSE(known_galleries_[1].allowed);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), TRUE);
  EXPECT_TRUE(known_galleries_[1].allowed);

  EXPECT_EQ(2U, toggles_);
}

// Tests that UpdateGallery will add a new checkbox, but only if it refers to
// a gallery that the dialog hasn't seen before.
TEST_F(MediaGalleriesDialogTest, UpdateAdds) {
  scoped_ptr<MediaGalleriesDialogGtk>
      dialog_(new MediaGalleriesDialogGtk(this));

  EXPECT_TRUE(dialog_->checkbox_map_.empty());

  MediaGalleryPrefInfo gallery1;
  dialog_->UpdateGallery(&gallery1, true);
  EXPECT_EQ(1U, dialog_->checkbox_map_.size());

  MediaGalleryPrefInfo gallery2;
  dialog_->UpdateGallery(&gallery2, true);
  EXPECT_EQ(2U, dialog_->checkbox_map_.size());

  dialog_->UpdateGallery(&gallery2, false);
  EXPECT_EQ(2U, dialog_->checkbox_map_.size());
}

}  // namespace chrome
