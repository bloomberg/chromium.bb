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

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, OpenMultipleImagesOnDrive) {
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

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, TraverseSlideImagesOnDrive) {
  set_test_case_name("traverseSlideImagesOnDrive");
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

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, RotateImageOnDownloads) {
  set_test_case_name("rotateImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode, RotateImageOnDownloads) {
  set_test_case_name("rotateImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, RotateImageOnDrive) {
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

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest,
                       RenameImageInThumbnailModeOnDownloads) {
  set_test_case_name("renameImageInThumbnailModeOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, RenameImageInThumbnailModeOnDrive) {
  set_test_case_name("renameImageInThumbnailModeOnDrive");
  StartTest();
}

}  // namespace file_manager
