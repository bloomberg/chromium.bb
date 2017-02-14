// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_browsertest_base.h"

namespace file_manager {

template <GuestMode M>
class GalleryBrowserTestBase : public FileManagerBrowserTestBase {
 public:
  GuestMode GetGuestModeParam() const override { return M; }
  const char* GetTestCaseNameParam() const override {
    return test_case_name_.c_str();
  }

 protected:
  const char* GetTestManifestName() const override {
    return "gallery_test_manifest.json";
  }

  void set_test_case_name(const std::string& name) { test_case_name_ = name; }

 private:
  base::ListValue scripts_;
  std::string test_case_name_;
};

typedef GalleryBrowserTestBase<NOT_IN_GUEST_MODE> GalleryBrowserTest;
typedef GalleryBrowserTestBase<IN_GUEST_MODE> GalleryBrowserTestInGuestMode;

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_OpenSingleImageOnDownloads DISABLED_OpenSingleImageOnDownloads
#else
#define MAYBE_OpenSingleImageOnDownloads OpenSingleImageOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_OpenSingleImageOnDownloads) {
  set_test_case_name("openSingleImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       MAYBE_OpenSingleImageOnDownloads) {
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

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_OpenMultipleImagesOnDrive DISABLED_OpenMultipleImagesOnDrive
#else
#define MAYBE_OpenMultipleImagesOnDrive OpenMultipleImagesOnDrive
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_OpenMultipleImagesOnDrive) {
  set_test_case_name("openMultipleImagesOnDrive");
  StartTest();
}

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_TraverseSlideImagesOnDownloads \
  DISABLED_TraverseSlideImagesOnDownloads
#else
#define MAYBE_TraverseSlideImagesOnDownloads TraverseSlideImagesOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       MAYBE_TraverseSlideImagesOnDownloads) {
  set_test_case_name("traverseSlideImagesOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       TraverseSlideImagesOnDownloads) {
  set_test_case_name("traverseSlideImagesOnDownloads");
  StartTest();
}

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_TraverseSlideImagesOnDrive DISABLED_TraverseSlideImagesOnDrive
#else
#define MAYBE_TraverseSlideImagesOnDrive TraverseSlideImagesOnDrive
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_TraverseSlideImagesOnDrive) {
  set_test_case_name("traverseSlideImagesOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       TraverseSlideThumbnailsOnDownloads) {
  set_test_case_name("traverseSlideThumbnailsOnDownloads");
  StartTest();
}

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_TraverseSlideThumbnailsOnDownloads \
  DISABLED_TraverseSlideThumbnailsOnDownloads
#else
#define MAYBE_TraverseSlideThumbnailsOnDownloads \
  TraverseSlideThumbnailsOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       MAYBE_TraverseSlideThumbnailsOnDownloads) {
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

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_RenameImageOnDownloads DISABLED_RenameImageOnDownloads
#else
#define MAYBE_RenameImageOnDownloads RenameImageOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       MAYBE_RenameImageOnDownloads) {
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

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_DeleteImageOnDownloads DISABLED_DeleteImageOnDownloads
#else
#define MAYBE_DeleteImageOnDownloads DeleteImageOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_DeleteImageOnDownloads) {
  set_test_case_name("deleteImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode, DeleteImageOnDownloads) {
  set_test_case_name("deleteImageOnDownloads");
  StartTest();
}

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_DeleteImageOnDrive DISABLED_DeleteImageOnDrive
#else
#define MAYBE_DeleteImageOnDrive DeleteImageOnDrive
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_DeleteImageOnDrive) {
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
                       MAYBE_CheckAvailabilityOfShareButtonOnDownloads) {
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

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_RotateImageOnDownloads DISABLED_RotateImageOnDownloads
#else
#define MAYBE_RotateImageOnDownloads RotateImageOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_RotateImageOnDownloads) {
  set_test_case_name("rotateImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode, RotateImageOnDownloads) {
  set_test_case_name("rotateImageOnDownloads");
  StartTest();
}

// http://crbug.com/508949
// http://crbug.com/690983 (Chrome OS debug build)
#if defined(MEMORY_SANITIZER) || (defined(OS_CHROMEOS) && !defined(NDEBUG))
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

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_CropImageOnDrive DISABLED_CropImageOnDrive
#else
#define MAYBE_CropImageOnDrive CropImageOnDrive
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_CropImageOnDrive) {
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

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       MAYBE_ResizeImageOnDownloads) {
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

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_RenameImageInThumbnailModeOnDownloads \
  DISABLED_RenameImageInThumbnailModeOnDownloads
#else
#define MAYBE_RenameImageInThumbnailModeOnDownloads \
  RenameImageInThumbnailModeOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       MAYBE_RenameImageInThumbnailModeOnDownloads) {
  set_test_case_name("renameImageInThumbnailModeOnDownloads");
  StartTest();
}

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_RenameImageInThumbnailModeOnDrive \
  DISABLED_RenameImageInThumbnailModeOnDrive
#else
#define MAYBE_RenameImageInThumbnailModeOnDrive \
  RenameImageInThumbnailModeOnDrive
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       MAYBE_RenameImageInThumbnailModeOnDrive) {
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
                       SelectAllImagesAfterImageDeletionOnDownloads) {
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

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_StopStartSlideshowOnDownloads \
  DISABLED_StopStartSlideshowOnDownloads
#else
#define MAYBE_StopStartSlideshowOnDownloads StopStartSlideshowOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       MAYBE_StopStartSlideshowOnDownloads) {
  set_test_case_name("stopStartSlideshowOnDownloads");
  StartTest();
}

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_StopStartSlideshowOnDrive DISABLED_StopStartSlideshowOnDrive
#else
#define MAYBE_StopStartSlideshowOnDrive StopStartSlideshowOnDrive
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_StopStartSlideshowOnDrive) {
  set_test_case_name("stopStartSlideshowOnDrive");
  StartTest();
}

}  // namespace file_manager
