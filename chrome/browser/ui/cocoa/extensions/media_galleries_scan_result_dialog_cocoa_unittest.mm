// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/media_galleries/media_galleries_scan_result_dialog_controller.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_alert.h"
#include "chrome/browser/ui/cocoa/extensions/media_galleries_scan_result_dialog_cocoa.h"
#include "components/storage_monitor/storage_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/test_util.h"
#include "testing/gtest_mac.h"

class MockMediaGalleriesScanResultDialogController
    : public MediaGalleriesScanResultDialogController {
 public:
  explicit MockMediaGalleriesScanResultDialogController(
      const extensions::Extension& extension)
      : MediaGalleriesScanResultDialogController(
          extension, NULL, base::Bind(&MediaGalleriesScanResultDialog::Create),
          base::Bind(&base::DoNothing)) {
  }
  virtual ~MockMediaGalleriesScanResultDialogController() {
  }

  // MediaGalleriesScanResultDialogController overrides.
  virtual OrderedScanResults GetGalleryList() const OVERRIDE {
    MediaGalleryPrefInfo test_pref_info;
    test_pref_info.pref_id = 1;
    test_pref_info.display_name = base::ASCIIToUTF16("Test Gallery");
    test_pref_info.device_id =
        StorageInfo::MakeDeviceId(StorageInfo::FIXED_MASS_STORAGE, "blah");
    test_pref_info.type = MediaGalleryPrefInfo::kScanResult;

    OrderedScanResults result;
    result.push_back(ScanResult(test_pref_info, true));
    return result;
  }
  virtual void DidToggleGalleryId(MediaGalleryPrefId pref_id,
                                  bool selected) OVERRIDE {}
  virtual void DidClickOpenFolderViewer(
      MediaGalleryPrefId pref_id) const OVERRIDE {}
  virtual void DidForgetGallery(MediaGalleryPrefId pref_id) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaGalleriesScanResultDialogController);
};

class MediaGalleriesScanResultDialogCocoaTest : public CocoaTest {
 public:
  MediaGalleriesScanResultDialogCocoaTest() {}
  virtual ~MediaGalleriesScanResultDialogCocoaTest() {}

  MediaGalleriesScanResultDialogCocoa* CreateDialog() {
    dummy_extension_ = extensions::test_util::CreateExtensionWithID("dummy");
    mock_controller_.reset(new MockMediaGalleriesScanResultDialogController(
        *dummy_extension_.get()));
    return static_cast<MediaGalleriesScanResultDialogCocoa*>(
        mock_controller_->dialog_.get());
  }

  ConstrainedWindowAlert* ExtractAlert() {
    MediaGalleriesScanResultDialogCocoa* cocoa_dialog =
        static_cast<MediaGalleriesScanResultDialogCocoa*>(
          mock_controller_->dialog_.get());
    return cocoa_dialog->alert_.get();
  }

  void FinishDialog() {
    // The controller deletes itself on Finish.
    MockMediaGalleriesScanResultDialogController* controller =
        mock_controller_.release();
    controller->DialogFinished(false);
  }

 private:
  scoped_refptr<extensions::Extension> dummy_extension_;
  scoped_ptr<MockMediaGalleriesScanResultDialogController> mock_controller_;
};

TEST_F(MediaGalleriesScanResultDialogCocoaTest, Show) {
  CreateDialog();
  ConstrainedWindowAlert* alert = ExtractAlert();

  [[alert window] makeKeyAndOrderFront:nil];
  FinishDialog();
}
