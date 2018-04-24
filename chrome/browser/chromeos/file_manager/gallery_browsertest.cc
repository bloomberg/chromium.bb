// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_browsertest_base.h"

namespace file_manager {

template <GuestMode MODE>
class GalleryBrowserTestBase : public FileManagerBrowserTestBase {
 public:
  GuestMode GetGuestModeParam() const override { return MODE; }

  const char* GetTestCaseNameParam() const override {
    return test_case_name_.c_str();
  }

  const char* GetTestManifestName() const override {
    return "gallery_test_manifest.json";
  }

 protected:
  void set_test_case_name(const std::string& name) { test_case_name_ = name; }

 private:
  std::string test_case_name_;
};

typedef GalleryBrowserTestBase<NOT_IN_GUEST_MODE> GalleryBrowserTest;
typedef GalleryBrowserTestBase<IN_GUEST_MODE> GalleryBrowserTestInGuestMode;

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, OpenSingleImageOnDownloads) {
  set_test_case_name("openSingleImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       OpenSingleImageOnDownloads) {
  set_test_case_name("openSingleImageOnDownloads");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_OpenSingleImageOnDrive DISABLED_OpenSingleImageOnDrive
#else
#define MAYBE_OpenSingleImageOnDrive OpenSingleImageOnDrive
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_OpenSingleImageOnDrive) {
  set_test_case_name("openSingleImageOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, OpenMultipleImagesOnDownloads) {
  set_test_case_name("openMultipleImagesOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       OpenMultipleImagesOnDownloads) {
  set_test_case_name("openMultipleImagesOnDownloads");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_OpenMultipleImagesAndSwitchToSlideModeOnDownloads \
  DISABLED_OpenMultipleImagesAndSwitchToSlideModeOnDownloads
#else
#define MAYBE_OpenMultipleImagesAndSwitchToSlideModeOnDownloads \
  OpenMultipleImagesAndSwitchToSlideModeOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(
    GalleryBrowserTest,
    MAYBE_OpenMultipleImagesAndSwitchToSlideModeOnDownloads) {
  set_test_case_name("openMultipleImagesAndChangeToSlideModeOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, OpenMultipleImagesOnDrive) {
  set_test_case_name("openMultipleImagesOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, TraverseSlideImagesOnDownloads) {
  set_test_case_name("traverseSlideImagesOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       TraverseSlideImagesOnDownloads) {
  set_test_case_name("traverseSlideImagesOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, TraverseSlideImagesOnDrive) {
  set_test_case_name("traverseSlideImagesOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       TraverseSlideThumbnailsOnDownloads) {
  set_test_case_name("traverseSlideThumbnailsOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, TraverseSlideThumbnailsOnDownloads) {
  set_test_case_name("traverseSlideThumbnailsOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, TraverseSlideThumbnailsOnDrive) {
  set_test_case_name("traverseSlideThumbnailsOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, RenameImageOnDownloads) {
  set_test_case_name("renameImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode, RenameImageOnDownloads) {
  set_test_case_name("renameImageOnDownloads");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_RenameImageOnDrive DISABLED_RenameImageOnDrive
#else
#define MAYBE_RenameImageOnDrive RenameImageOnDrive
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_RenameImageOnDrive) {
  set_test_case_name("renameImageOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, DeleteImageOnDownloads) {
  set_test_case_name("deleteImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode, DeleteImageOnDownloads) {
  set_test_case_name("deleteImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, DeleteImageOnDrive) {
  set_test_case_name("deleteImageOnDrive");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_CheckAvailabilityOfShareButtonOnDownloads \
  DISABLED_CheckAvailabilityOfShareButtonOnDownloads
#else
#define MAYBE_CheckAvailabilityOfShareButtonOnDownloads \
  CheckAvailabilityOfShareButtonOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       MAYBE_CheckAvailabilityOfShareButtonOnDownloads) {
  set_test_case_name("checkAvailabilityOfShareButtonOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       CheckAvailabilityOfShareButtonOnDownloads) {
  set_test_case_name("checkAvailabilityOfShareButtonOnDownloads");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_CheckAvailabilityOfShareButtonOnDrive \
  DISABLED_CheckAvailabilityOfShareButtonOnDrive
#else
#define MAYBE_CheckAvailabilityOfShareButtonOnDrive \
  CheckAvailabilityOfShareButtonOnDrive
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       MAYBE_CheckAvailabilityOfShareButtonOnDrive) {
  set_test_case_name("checkAvailabilityOfShareButtonOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, RotateImageOnDownloads) {
  set_test_case_name("rotateImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode, RotateImageOnDownloads) {
  set_test_case_name("rotateImageOnDownloads");
  StartTest();
}

// http://crbug.com/690983 (Chrome OS debug build)
#if (defined(OS_CHROMEOS) && !defined(NDEBUG))
#define MAYBE_RotateImageOnDrive DISABLED_RotateImageOnDrive
#else
#define MAYBE_RotateImageOnDrive RotateImageOnDrive
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_RotateImageOnDrive) {
  set_test_case_name("rotateImageOnDrive");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_CropImageOnDownloads DISABLED_CropImageOnDownloads
#else
#define MAYBE_CropImageOnDownloads CropImageOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_CropImageOnDownloads) {
  set_test_case_name("cropImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode, CropImageOnDownloads) {
  set_test_case_name("cropImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, CropImageOnDrive) {
  set_test_case_name("cropImageOnDrive");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_ExposureImageOnDownloads DISABLED_ExposureImageOnDownloads
#else
#define MAYBE_ExposureImageOnDownloads ExposureImageOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_ExposureImageOnDownloads) {
  set_test_case_name("exposureImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       ExposureImageOnDownloads) {
  set_test_case_name("exposureImageOnDownloads");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_ExposureImageOnDrive DISABLED_ExposureImageOnDrive
#else
#define MAYBE_ExposureImageOnDrive ExposureImageOnDrive
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_ExposureImageOnDrive) {
  set_test_case_name("exposureImageOnDrive");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_ResizeImageOnDownloads DISABLED_ResizeImageOnDownloads
#else
#define MAYBE_ResizeImageOnDownloads ResizeImageOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_ResizeImageOnDownloads) {
  set_test_case_name("resizeImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode, ResizeImageOnDownloads) {
  set_test_case_name("resizeImageOnDownloads");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_ResizeImageOnDrive DISABLED_ResizeImageOnDrive
#else
#define MAYBE_ResizeImageOnDrive ResizeImageOnDrive
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_ResizeImageOnDrive) {
  set_test_case_name("resizeImageOnDrive");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_EnableDisableOverwriteOriginalCheckboxOnDownloads \
  DISABLED_EnableDisableOverwriteOriginalCheckboxOnDownloads
#else
#define MAYBE_EnableDisableOverwriteOriginalCheckboxOnDownloads \
  EnableDisableOverwriteOriginalCheckboxOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(
    GalleryBrowserTest,
    MAYBE_EnableDisableOverwriteOriginalCheckboxOnDownloads) {
  set_test_case_name("enableDisableOverwriteOriginalCheckboxOnDownloads");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_EnableDisableOverwriteOriginalCheckboxOnDrive \
  DISABLED_EnableDisableOverwriteOriginalCheckboxOnDrive
#else
#define MAYBE_EnableDisableOverwriteOriginalCheckboxOnDrive \
  EnableDisableOverwriteOriginalCheckboxOnDrive
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       MAYBE_EnableDisableOverwriteOriginalCheckboxOnDrive) {
  set_test_case_name("enableDisableOverwriteOriginalCheckboxOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       RenameImageInThumbnailModeOnDownloads) {
  set_test_case_name("renameImageInThumbnailModeOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, RenameImageInThumbnailModeOnDrive) {
  set_test_case_name("renameImageInThumbnailModeOnDrive");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_DeleteAllImagesInThumbnailModeOnDownloads \
  DISABLED_DeleteAllImagesInThumbnailModeOnDownloads
#else
#define MAYBE_DeleteAllImagesInThumbnailModeOnDownloads \
  DeleteAllImagesInThumbnailModeOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       MAYBE_DeleteAllImagesInThumbnailModeOnDownloads) {
  set_test_case_name("deleteAllImagesInThumbnailModeOnDownloads");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_DeleteAllImagesInThumbnailModeOnDrive \
  DISABLED_DeleteAllImagesInThumbnailModeOnDrive
#else
#define MAYBE_DeleteAllImagesInThumbnailModeOnDrive \
  DeleteAllImagesInThumbnailModeOnDrive
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       MAYBE_DeleteAllImagesInThumbnailModeOnDrive) {
  set_test_case_name("deleteAllImagesInThumbnailModeOnDrive");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_DeleteAllImagesInThumbnailModeWithEnterKey \
  DISABLED_DeleteAllImagesInThumbnailModeWithEnterKey
#else
#define MAYBE_DeleteAllImagesInThumbnailModeWithEnterKey \
  DeleteAllImagesInThumbnailModeWithEnterKey
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       MAYBE_DeleteAllImagesInThumbnailModeWithEnterKey) {
  set_test_case_name("deleteAllImagesInThumbnailModeWithEnterKey");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_DeleteAllImagesInThumbnailModeWithDeleteKey \
  DISABLED_DeleteAllImagesInThumbnailModeWithDeleteKey
#else
#define MAYBE_DeleteAllImagesInThumbnailModeWithDeleteKey \
  DeleteAllImagesInThumbnailModeWithDeleteKey
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       MAYBE_DeleteAllImagesInThumbnailModeWithDeleteKey) {
  set_test_case_name("deleteAllImagesInThumbnailModeWithDeleteKey");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_EmptySpaceClickUnselectsInThumbnailModeOnDownloads \
  DISABLED_EmptySpaceClickUnselectsInThumbnailModeOnDownloads
#else
#define MAYBE_EmptySpaceClickUnselectsInThumbnailModeOnDownloads \
  EmptySpaceClickUnselectsInThumbnailModeOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(
    GalleryBrowserTest,
    MAYBE_EmptySpaceClickUnselectsInThumbnailModeOnDownloads) {
  set_test_case_name("emptySpaceClickUnselectsInThumbnailModeOnDownloads");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_EmptySpaceClickUnselectsInThumbnailModeOnDrive \
  DISABLED_EmptySpaceClickUnselectsInThumbnailModeOnDrive
#else
#define MAYBE_EmptySpaceClickUnselectsInThumbnailModeOnDrive \
  EmptySpaceClickUnselectsInThumbnailModeOnDrive
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       MAYBE_EmptySpaceClickUnselectsInThumbnailModeOnDrive) {
  set_test_case_name("emptySpaceClickUnselectsInThumbnailModeOnDrive");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_SelectMultipleImagesWithShiftKeyOnDownloads \
  DISABLED_SelectMultipleImagesWithShiftKeyOnDownloads
#else
#define MAYBE_SelectMultipleImagesWithShiftKeyOnDownloads \
  SelectMultipleImagesWithShiftKeyOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       MAYBE_SelectMultipleImagesWithShiftKeyOnDownloads) {
  set_test_case_name("selectMultipleImagesWithShiftKeyOnDownloads");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_SelectAllImagesAfterImageDeletionOnDownloads \
  DISABLED_SelectAllImagesAfterImageDeletionOnDownloads
#else
#define MAYBE_SelectAllImagesAfterImageDeletionOnDownloads \
  SelectAllImagesAfterImageDeletionOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       MAYBE_SelectAllImagesAfterImageDeletionOnDownloads) {
  set_test_case_name("selectAllImagesAfterImageDeletionOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       SlideshowTraversalOnDownloads) {
  set_test_case_name("slideshowTraversalOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, SlideshowTraversalOnDownloads) {
  set_test_case_name("slideshowTraversalOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, SlideshowTraversalOnDrive) {
  set_test_case_name("slideshowTraversalOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       StopStartSlideshowOnDownloads) {
  set_test_case_name("stopStartSlideshowOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, StopStartSlideshowOnDownloads) {
  set_test_case_name("stopStartSlideshowOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, StopStartSlideshowOnDrive) {
  set_test_case_name("stopStartSlideshowOnDrive");
  StartTest();
}

}  // namespace file_manager
