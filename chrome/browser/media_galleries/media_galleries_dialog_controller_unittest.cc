// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/storage_monitor/storage_info.h"
#include "chrome/browser/storage_monitor/test_storage_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

std::string GalleryName(const MediaGalleryPrefInfo& gallery) {
  string16 name = gallery.GetGalleryDisplayName();
  return UTF16ToASCII(name);
}

TEST(MediaGalleriesDialogControllerTest, TestNameGeneration) {
  ASSERT_TRUE(TestStorageMonitor::CreateAndInstall());
  MediaGalleryPrefInfo gallery;
  gallery.pref_id = 1;
  gallery.device_id = StorageInfo::MakeDeviceId(
      StorageInfo::FIXED_MASS_STORAGE, "/path/to/gallery");
  gallery.type = MediaGalleryPrefInfo::kAutoDetected;
  std::string galleryName("/path/to/gallery");
#if defined(OS_CHROMEOS)
  galleryName = "gallery";
#endif
  EXPECT_EQ(galleryName, GalleryName(gallery));

  gallery.display_name = ASCIIToUTF16("override");
  EXPECT_EQ("override", GalleryName(gallery));

  gallery.display_name = string16();
  gallery.volume_label = ASCIIToUTF16("label");
  EXPECT_EQ(galleryName, GalleryName(gallery));

  gallery.path = base::FilePath(FILE_PATH_LITERAL("sub/gallery2"));
  galleryName = "/path/to/gallery/sub/gallery2";
#if defined(OS_CHROMEOS)
  galleryName = "gallery2";
#endif
#if defined(OS_WIN)
  galleryName = base::FilePath(FILE_PATH_LITERAL("/path/to/gallery"))
                    .Append(gallery.path).MaybeAsASCII();
#endif
  EXPECT_EQ(galleryName, GalleryName(gallery));

  gallery.path = base::FilePath();
  gallery.device_id = StorageInfo::MakeDeviceId(
      StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM,
      "/path/to/dcim");
  gallery.display_name = ASCIIToUTF16("override");
  EXPECT_EQ("override", GalleryName(gallery));

  gallery.volume_label = ASCIIToUTF16("volume");
  gallery.vendor_name = ASCIIToUTF16("vendor");
  gallery.model_name = ASCIIToUTF16("model");
  EXPECT_EQ("override", GalleryName(gallery));

  gallery.display_name = string16();
  EXPECT_EQ("volume", GalleryName(gallery));

  gallery.volume_label = string16();
  EXPECT_EQ("vendor, model", GalleryName(gallery));

  gallery.total_size_in_bytes = 1000000;
  EXPECT_EQ("977 KB vendor, model", GalleryName(gallery));

  gallery.path = base::FilePath(FILE_PATH_LITERAL("sub/path"));
  EXPECT_EQ("path - 977 KB vendor, model", GalleryName(gallery));
  TestStorageMonitor::RemoveSingleton();
}
