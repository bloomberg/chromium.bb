// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaGalleriesPreferences unit tests.

#include "chrome/browser/media_gallery/media_galleries_preferences.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/media_gallery/media_file_system_registry.h"
#include "chrome/browser/media_gallery/media_galleries_test_util.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "sync/api/string_ordinal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

namespace {

class MediaStorageUtilTest : public MediaStorageUtil {
 public:
  static void SetTestingMode() {
    SetGetDeviceInfoFromPathFunctionForTesting(
        &GetDeviceInfoFromPathTestFunction);
  }

  static bool GetDeviceInfoFromPathTestFunction(const base::FilePath& path,
                                                std::string* device_id,
                                                string16* device_name,
                                                base::FilePath* relative_path) {
    if (device_id)
      *device_id = MakeDeviceId(FIXED_MASS_STORAGE, path.AsUTF8Unsafe());
    if (device_name)
      *device_name = path.BaseName().LossyDisplayName();
    if (relative_path)
      *relative_path = base::FilePath();
    return true;
  }
};

class MockGalleryChangeObserver
    : public MediaGalleriesPreferences::GalleryChangeObserver {
 public:
  explicit MockGalleryChangeObserver(MediaGalleriesPreferences* pref)
      : pref_(pref),
        notifications_(0) {}
  virtual ~MockGalleryChangeObserver() {}

  int notifications() const { return notifications_;}

 private:
  // MediaGalleriesPreferences::GalleryChangeObserver implementation.
  virtual void OnGalleryChanged(MediaGalleriesPreferences* pref,
                                const std::string& /*extension_id*/) OVERRIDE {
    EXPECT_EQ(pref_, pref);
    ++notifications_;
  }

  MediaGalleriesPreferences* pref_;
  int notifications_;

  DISALLOW_COPY_AND_ASSIGN(MockGalleryChangeObserver);
};

}  // namespace

class MediaGalleriesPreferencesTest : public testing::Test {
 public:
  typedef std::map<std::string /*device id*/, MediaGalleryPrefIdSet>
      DeviceIdPrefIdsMap;

  MediaGalleriesPreferencesTest()
      : ui_thread_(content::BrowserThread::UI, &loop_),
        file_thread_(content::BrowserThread::FILE, &loop_),
        profile_(new TestingProfile()),
        default_galleries_count_(0) {
    MediaStorageUtilTest::SetTestingMode();
  }

  virtual ~MediaGalleriesPreferencesTest() {
    // TestExtensionSystem uses DeleteSoon, so we need to delete the profile
    // and then run the message queue to clean up.
    profile_.reset();
    MessageLoop::current()->RunUntilIdle();
  }

  virtual void SetUp() OVERRIDE {
    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_.get())));
    extension_system->CreateExtensionService(
        CommandLine::ForCurrentProcess(), base::FilePath(), false);

    gallery_prefs_.reset(new MediaGalleriesPreferences(profile_.get()));

    // Load the default galleries into the expectations.
    const MediaGalleriesPrefInfoMap& known_galleries =
        gallery_prefs_->known_galleries();
    if (known_galleries.size()) {
      ASSERT_EQ(3U, known_galleries.size());
      default_galleries_count_ = 3;
      MediaGalleriesPrefInfoMap::const_iterator it;
      for (it = known_galleries.begin(); it != known_galleries.end(); ++it) {
        expected_galleries_[it->first] = it->second;
        if (it->second.type == MediaGalleryPrefInfo::kAutoDetected)
          expected_galleries_for_all.insert(it->first);
      }
    }

    std::vector<std::string> all_permissions;
    all_permissions.push_back("allAutoDetected");
    all_permissions.push_back("read");
    std::vector<std::string> read_permissions;
    read_permissions.push_back("read");

    all_permission_extension =
        AddMediaGalleriesApp("all", all_permissions, profile_.get());
    regular_permission_extension =
        AddMediaGalleriesApp("regular", read_permissions, profile_.get());
    no_permissions_extension =
        AddMediaGalleriesApp("no", read_permissions, profile_.get());
  }

  virtual void TearDown() OVERRIDE {
    Verify();
  }

  void Verify() {
    const MediaGalleriesPrefInfoMap& known_galleries =
        gallery_prefs_->known_galleries();
    EXPECT_EQ(expected_galleries_.size(), known_galleries.size());
    for (MediaGalleriesPrefInfoMap::const_iterator it = known_galleries.begin();
         it != known_galleries.end();
         ++it) {
      VerifyGalleryInfo(it->second, it->first);
    }

    for (DeviceIdPrefIdsMap::const_iterator it = expected_device_map.begin();
         it != expected_device_map.end();
         ++it) {
      MediaGalleryPrefIdSet actual_id_set =
          gallery_prefs_->LookUpGalleriesByDeviceId(it->first);
      EXPECT_EQ(it->second, actual_id_set);
    }

    std::set<MediaGalleryPrefId> galleries_for_all =
        gallery_prefs_->GalleriesForExtension(*all_permission_extension.get());
    EXPECT_EQ(expected_galleries_for_all, galleries_for_all);

    std::set<MediaGalleryPrefId> galleries_for_regular =
        gallery_prefs_->GalleriesForExtension(
            *regular_permission_extension.get());
    EXPECT_EQ(expected_galleries_for_regular, galleries_for_regular);

    std::set<MediaGalleryPrefId> galleries_for_no =
        gallery_prefs_->GalleriesForExtension(*no_permissions_extension.get());
    EXPECT_EQ(0U, galleries_for_no.size());
  }

  void VerifyGalleryInfo(const MediaGalleryPrefInfo& actual,
                         MediaGalleryPrefId expected_id) const {
    MediaGalleriesPrefInfoMap::const_iterator in_expectation =
      expected_galleries_.find(expected_id);
    ASSERT_FALSE(in_expectation == expected_galleries_.end())  << expected_id;
    EXPECT_EQ(in_expectation->second.pref_id, actual.pref_id);
    EXPECT_EQ(in_expectation->second.display_name, actual.display_name);
    EXPECT_EQ(in_expectation->second.device_id, actual.device_id);
    EXPECT_EQ(in_expectation->second.path.value(), actual.path.value());
    EXPECT_EQ(in_expectation->second.type, actual.type);
  }

  MediaGalleriesPreferences* gallery_prefs() {
    return gallery_prefs_.get();
  }

  uint64 default_galleries_count() {
    return default_galleries_count_;
  }

  void AddGalleryExpectation(MediaGalleryPrefId id, string16 display_name,
                             std::string device_id,
                             base::FilePath relative_path,
                             MediaGalleryPrefInfo::Type type) {
    expected_galleries_[id].pref_id = id;
    expected_galleries_[id].display_name = display_name;
    expected_galleries_[id].device_id = device_id;
    expected_galleries_[id].path = relative_path.NormalizePathSeparators();
    expected_galleries_[id].type = type;

    if (type == MediaGalleryPrefInfo::kAutoDetected)
      expected_galleries_for_all.insert(id);

    expected_device_map[device_id].insert(id);
  }

  MediaGalleryPrefId AddGalleryWithNameV0(const std::string& device_id,
                                        const string16& display_name,
                                        const base::FilePath& relative_path,
                                        bool user_added) {
    return gallery_prefs()->AddGalleryInternal(
        device_id, display_name, relative_path, user_added,
        string16(), string16(), string16(), 0, base::Time(), false, 0);
  }

  scoped_refptr<extensions::Extension> all_permission_extension;
  scoped_refptr<extensions::Extension> regular_permission_extension;
  scoped_refptr<extensions::Extension> no_permissions_extension;

  std::set<MediaGalleryPrefId> expected_galleries_for_all;
  std::set<MediaGalleryPrefId> expected_galleries_for_regular;

  DeviceIdPrefIdsMap expected_device_map;

  MediaGalleriesPrefInfoMap expected_galleries_;

 private:
  // Needed for extension service & friends to work.
  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<MediaGalleriesPreferences> gallery_prefs_;

  uint64 default_galleries_count_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesPreferencesTest);
};

base::FilePath MakePath(std::string dir) {
#if defined(OS_WIN)
  return base::FilePath(FILE_PATH_LITERAL("C:\\")).Append(UTF8ToWide(dir));
#elif defined(OS_POSIX)
  return base::FilePath(FILE_PATH_LITERAL("/")).Append(dir);
#else
  NOTREACHED();
#endif
}

TEST_F(MediaGalleriesPreferencesTest, GalleryManagement) {
  MediaGalleryPrefId auto_id, user_added_id, id;
  base::FilePath path;
  std::string device_id;
  string16 device_name;
  base::FilePath relative_path;
  Verify();

  // Add a new auto detected gallery.
  path = MakePath("new_auto");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &device_id, NULL,
                                          &relative_path);
  device_name = ASCIIToUTF16("NewAutoGallery");
  id = gallery_prefs()->AddGalleryWithName(device_id, device_name,
                                           relative_path, false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  auto_id = id;
  AddGalleryExpectation(id, device_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  // Add it again (as user), nothing should happen.
  id = gallery_prefs()->AddGalleryWithName(device_id, device_name,
                                           relative_path, true /*auto*/);
  EXPECT_EQ(auto_id, id);
  Verify();

  // Add a new user added gallery.
  path = MakePath("new_user");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &device_id, NULL,
                                          &relative_path);
  device_name = ASCIIToUTF16("NewUserGallery");
  id = gallery_prefs()->AddGalleryWithName(device_id, device_name,
                                           relative_path, true /*user*/);
  EXPECT_EQ(default_galleries_count() + 2UL, id);
  user_added_id = id;
  AddGalleryExpectation(id, device_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  // Lookup some galleries.
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(MakePath("new_auto"), NULL));
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(MakePath("new_user"), NULL));
  EXPECT_FALSE(gallery_prefs()->LookUpGalleryByPath(MakePath("other"), NULL));

  // Check that we always get the gallery info.
  MediaGalleryPrefInfo gallery_info;
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(MakePath("new_auto"),
                                                   &gallery_info));
  VerifyGalleryInfo(gallery_info, auto_id);
  EXPECT_FALSE(gallery_info.volume_metadata_valid);
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(MakePath("new_user"),
                                                   &gallery_info));
  VerifyGalleryInfo(gallery_info, user_added_id);
  EXPECT_FALSE(gallery_info.volume_metadata_valid);

  path = MakePath("other");
  EXPECT_FALSE(gallery_prefs()->LookUpGalleryByPath(path, &gallery_info));
  EXPECT_EQ(kInvalidMediaGalleryPrefId, gallery_info.pref_id);
  EXPECT_EQ(path.BaseName().LossyDisplayName(), gallery_info.display_name);
  std::string other_device_id;
  MediaStorageUtil::GetDeviceInfoFromPath(path, &other_device_id, NULL,
                                          &relative_path);
  EXPECT_EQ(other_device_id, gallery_info.device_id);
  EXPECT_EQ(relative_path.value(), gallery_info.path.value());

  // Remove an auto added gallery (i.e. make it blacklisted).
  gallery_prefs()->ForgetGalleryById(auto_id);
  expected_galleries_[auto_id].type = MediaGalleryPrefInfo::kBlackListed;
  expected_galleries_for_all.erase(auto_id);
  Verify();

  // Remove a user added gallery and it should go away.
  gallery_prefs()->ForgetGalleryById(user_added_id);
  expected_galleries_.erase(user_added_id);
  expected_device_map[device_id].erase(user_added_id);
  Verify();
}

TEST_F(MediaGalleriesPreferencesTest, AddGalleryWithVolumeMetadata) {
  MediaGalleryPrefId id;
  base::FilePath path;
  std::string device_id;
  base::FilePath relative_path;
  base::Time now = base::Time::Now();
  Verify();

  // Add a new auto detected gallery.
  path = MakePath("new_auto");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &device_id, NULL,
                                          &relative_path);
  id = gallery_prefs()->AddGallery(device_id, relative_path,
                                   false /*auto*/,
                                   ASCIIToUTF16("volume label"),
                                   ASCIIToUTF16("vendor name"),
                                   ASCIIToUTF16("model name"),
                                   1000000ULL, now);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  AddGalleryExpectation(id, string16(), device_id, relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  MediaGalleryPrefInfo gallery_info;
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(MakePath("new_auto"),
                                                   &gallery_info));
  EXPECT_TRUE(gallery_info.volume_metadata_valid);
  EXPECT_EQ(ASCIIToUTF16("volume label"), gallery_info.volume_label);
  EXPECT_EQ(ASCIIToUTF16("vendor name"), gallery_info.vendor_name);
  EXPECT_EQ(ASCIIToUTF16("model name"), gallery_info.model_name);
  EXPECT_EQ(1000000ULL, gallery_info.total_size_in_bytes);
  // Note: we put the microseconds time into a double, so there'll
  // be some possible rounding errors. If it's less than 100, we don't
  // care.
  EXPECT_LE(abs(now.ToInternalValue() -
                gallery_info.last_attach_time.ToInternalValue()), 100);
}

TEST_F(MediaGalleriesPreferencesTest, ReplaceGalleryWithVolumeMetadata) {
  MediaGalleryPrefId id, metadata_id;
  base::FilePath path;
  std::string device_id;
  string16 device_name;
  base::FilePath relative_path;
  base::Time now = base::Time::Now();
  Verify();

  // Add an auto detected gallery in the prefs version 0 format.
  path = MakePath("new_auto");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &device_id, NULL,
                                          &relative_path);
  device_name = ASCIIToUTF16("NewAutoGallery");
  id = AddGalleryWithNameV0(device_id, device_name,
                            relative_path, false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  AddGalleryExpectation(id, device_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  metadata_id = gallery_prefs()->AddGallery(device_id,
                                            relative_path,
                                            false /*auto*/,
                                            ASCIIToUTF16("volume label"),
                                            ASCIIToUTF16("vendor name"),
                                            ASCIIToUTF16("model name"),
                                            1000000ULL, now);
  EXPECT_EQ(id, metadata_id);
  AddGalleryExpectation(id, string16(), device_id, relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);

  // Make sure the display_name is set to empty now, as the metadata
  // upgrade should set the manual override name empty.
  Verify();
}


// Whenever a gallery is added, its type is either set to "AutoDetected" or
// "UserAdded". When the gallery is removed, user added galleries are actually
// deleted and the auto detected galleries are moved to black listed state.
// When the gallery is added again, the black listed state is updated back to
// "AutoDetected" type.
TEST_F(MediaGalleriesPreferencesTest, UpdateGalleryType) {
  MediaGalleryPrefId auto_id, id;
  base::FilePath path;
  std::string device_id;
  string16 device_name;
  base::FilePath relative_path;
  Verify();

  // Add a new auto detect gallery to test with.
  path = MakePath("new_auto");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &device_id, NULL,
                                          &relative_path);
  device_name = ASCIIToUTF16("NewAutoGallery");
  id = gallery_prefs()->AddGalleryWithName(device_id, device_name,
                                           relative_path, false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  auto_id = id;
  AddGalleryExpectation(id, device_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  // Remove an auto added gallery (i.e. make it blacklisted).
  gallery_prefs()->ForgetGalleryById(auto_id);
  expected_galleries_[auto_id].type = MediaGalleryPrefInfo::kBlackListed;
  expected_galleries_for_all.erase(auto_id);
  Verify();

  // Add the gallery again as a user action.
  id = gallery_prefs()->AddGalleryByPath(path);
  EXPECT_EQ(auto_id, id);
  AddGalleryExpectation(id, device_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  // Remove an auto added gallery (i.e. make it blacklisted).
  gallery_prefs()->ForgetGalleryById(auto_id);
  expected_galleries_[auto_id].type = MediaGalleryPrefInfo::kBlackListed;
  expected_galleries_for_all.erase(auto_id);
  Verify();

  // Try adding the gallery again automatically and it should be a no-op.
  id = gallery_prefs()->AddGalleryWithName(device_id, device_name,
                                           relative_path, false /*auto*/);
  EXPECT_EQ(auto_id, id);
  Verify();
}

TEST_F(MediaGalleriesPreferencesTest, GalleryPermissions) {
  MediaGalleryPrefId auto_id, user_added_id, to_blacklist_id, id;
  base::FilePath path;
  std::string device_id;
  string16 device_name;
  base::FilePath relative_path;
  Verify();

  // Add some galleries to test with.
  path = MakePath("new_user");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &device_id, NULL,
                                          &relative_path);
  device_name = ASCIIToUTF16("NewUserGallery");
  id = gallery_prefs()->AddGalleryWithName(device_id, device_name,
                                           relative_path, true /*user*/);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  user_added_id = id;
  AddGalleryExpectation(id, device_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  path = MakePath("new_auto");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &device_id, NULL,
                                          &relative_path);
  device_name = ASCIIToUTF16("NewAutoGallery");
  id = gallery_prefs()->AddGalleryWithName(device_id, device_name,
                                           relative_path, false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 2UL, id);
  auto_id = id;
  AddGalleryExpectation(id, device_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  path = MakePath("to_blacklist");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &device_id, NULL,
                                          &relative_path);
  device_name = ASCIIToUTF16("ToBlacklistGallery");
  id = gallery_prefs()->AddGalleryWithName(device_id, device_name,
                                           relative_path, false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 3UL, id);
  to_blacklist_id = id;
  AddGalleryExpectation(id, device_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  // Remove permission for all galleries from the all-permission extension.
  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), auto_id, false);
  expected_galleries_for_all.erase(auto_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), user_added_id, false);
  expected_galleries_for_all.erase(user_added_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), to_blacklist_id, false);
  expected_galleries_for_all.erase(to_blacklist_id);
  Verify();

  // Add permission back for all galleries to the all-permission extension.
  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), auto_id, true);
  expected_galleries_for_all.insert(auto_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), user_added_id, true);
  expected_galleries_for_all.insert(user_added_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), to_blacklist_id, true);
  expected_galleries_for_all.insert(to_blacklist_id);
  Verify();

  // Add permission for all galleries to the regular permission extension.
  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), auto_id, true);
  expected_galleries_for_regular.insert(auto_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), user_added_id, true);
  expected_galleries_for_regular.insert(user_added_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), to_blacklist_id, true);
  expected_galleries_for_regular.insert(to_blacklist_id);
  Verify();

  // Blacklist the to be black listed gallery
  gallery_prefs()->ForgetGalleryById(to_blacklist_id);
  expected_galleries_[to_blacklist_id].type =
      MediaGalleryPrefInfo::kBlackListed;
  expected_galleries_for_all.erase(to_blacklist_id);
  expected_galleries_for_regular.erase(to_blacklist_id);
  Verify();

  // Remove permission for all galleries to the regular permission extension.
  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), auto_id, false);
  expected_galleries_for_regular.erase(auto_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), user_added_id, false);
  expected_galleries_for_regular.erase(user_added_id);
  Verify();

  // Add permission for an invalid gallery id.
  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), 9999L, true);
  Verify();
}

// Whenever a gallery is added, check to see if there is any gallery info exists
// for the added gallery. If so, verify the existing gallery information with
// the new details. If there is a mismatch, update the gallery information
// accordingly.
TEST_F(MediaGalleriesPreferencesTest, UpdateGalleryDetails) {
  MediaGalleryPrefId auto_id, id;
  base::FilePath path;
  std::string device_id;
  string16 device_name;
  base::FilePath relative_path;
  Verify();

  // Add a new auto detect gallery to test with.
  path = MakePath("new_auto");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &device_id, NULL,
                                          &relative_path);
  device_name = ASCIIToUTF16("NewAutoGallery");
  id = gallery_prefs()->AddGalleryWithName(device_id, device_name,
                                           relative_path, false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  auto_id = id;
  AddGalleryExpectation(id, device_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  // Update the device name and add the gallery again.
  device_name = ASCIIToUTF16("AutoGallery2");
  id = gallery_prefs()->AddGalleryWithName(device_id, device_name,
                                           relative_path, false /*auto*/);
  EXPECT_EQ(auto_id, id);
  AddGalleryExpectation(id, device_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();
}

TEST_F(MediaGalleriesPreferencesTest, MultipleGalleriesPerDevices) {
  base::FilePath path;
  std::string device_id;
  string16 device_name;
  base::FilePath relative_path;
  Verify();

  // Add a regular gallery
  path = MakePath("new_user");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &device_id, NULL,
                                          &relative_path);
  device_name = ASCIIToUTF16("NewUserGallery");
  MediaGalleryPrefId user_added_id =
      gallery_prefs()->AddGalleryWithName(device_id, device_name, relative_path,
                                          true /*user*/);
  EXPECT_EQ(default_galleries_count() + 1UL, user_added_id);
  AddGalleryExpectation(user_added_id, device_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  // Find it by device id and fail to find something related.
  MediaGalleryPrefIdSet pref_id_set;
  pref_id_set = gallery_prefs()->LookUpGalleriesByDeviceId(device_id);
  EXPECT_EQ(1U, pref_id_set.size());
  EXPECT_TRUE(pref_id_set.find(user_added_id) != pref_id_set.end());

  MediaStorageUtil::GetDeviceInfoFromPath(MakePath("new_user/foo"), &device_id,
                                          NULL, NULL);
  pref_id_set = gallery_prefs()->LookUpGalleriesByDeviceId(device_id);
  EXPECT_EQ(0U, pref_id_set.size());

  // Add some galleries on the same device.
  relative_path = base::FilePath(FILE_PATH_LITERAL("path1/on/device1"));
  device_name = ASCIIToUTF16("Device1Path1");
  device_id = "device1";
  MediaGalleryPrefId dev1_path1_id = gallery_prefs()->AddGalleryWithName(
      device_id, device_name, relative_path, true /*user*/);
  EXPECT_EQ(default_galleries_count() + 2UL, dev1_path1_id);
  AddGalleryExpectation(dev1_path1_id, device_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  relative_path = base::FilePath(FILE_PATH_LITERAL("path2/on/device1"));
  device_name = ASCIIToUTF16("Device1Path2");
  MediaGalleryPrefId dev1_path2_id = gallery_prefs()->AddGalleryWithName(
      device_id, device_name, relative_path, true /*user*/);
  EXPECT_EQ(default_galleries_count() + 3UL, dev1_path2_id);
  AddGalleryExpectation(dev1_path2_id, device_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  relative_path = base::FilePath(FILE_PATH_LITERAL("path1/on/device2"));
  device_name = ASCIIToUTF16("Device2Path1");
  device_id = "device2";
  MediaGalleryPrefId dev2_path1_id = gallery_prefs()->AddGalleryWithName(
      device_id, device_name, relative_path, true /*user*/);
  EXPECT_EQ(default_galleries_count() + 4UL, dev2_path1_id);
  AddGalleryExpectation(dev2_path1_id, device_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  relative_path = base::FilePath(FILE_PATH_LITERAL("path2/on/device2"));
  device_name = ASCIIToUTF16("Device2Path2");
  MediaGalleryPrefId dev2_path2_id = gallery_prefs()->AddGalleryWithName(
      device_id, device_name, relative_path, true /*user*/);
  EXPECT_EQ(default_galleries_count() + 5UL, dev2_path2_id);
  AddGalleryExpectation(dev2_path2_id, device_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  // Check that adding one of them again works as expected.
  MediaGalleryPrefId id = gallery_prefs()->AddGalleryWithName(
      device_id, device_name, relative_path, true /*user*/);
  EXPECT_EQ(dev2_path2_id, id);
  Verify();
}

TEST_F(MediaGalleriesPreferencesTest, GalleryChangeObserver) {
  // Start with one observer.
  MockGalleryChangeObserver observer1(gallery_prefs());
  gallery_prefs()->AddGalleryChangeObserver(&observer1);

  // Add a new auto detected gallery.
  string16 device_name = ASCIIToUTF16("NewAutoGallery");
  base::FilePath path = MakePath("new_auto");
  std::string device_id;
  base::FilePath relative_path;
  MediaStorageUtil::GetDeviceInfoFromPath(path, &device_id, NULL,
                                          &relative_path);
  MediaGalleryPrefId auto_id =
      gallery_prefs()->AddGalleryWithName(device_id, device_name, relative_path,
                                          false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 1UL, auto_id);
  AddGalleryExpectation(auto_id, device_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  EXPECT_EQ(1, observer1.notifications());

  // Add a second observer.
  MockGalleryChangeObserver observer2(gallery_prefs());
  gallery_prefs()->AddGalleryChangeObserver(&observer2);

  // Add a new user added gallery.
  path = MakePath("new_user");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &device_id, NULL,
                                          &relative_path);
  device_name = ASCIIToUTF16("NewUserGallery");
  MediaGalleryPrefId user_added_id =
      gallery_prefs()->AddGalleryWithName(device_id, device_name, relative_path,
                                          true /*user*/);
  AddGalleryExpectation(user_added_id, device_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(default_galleries_count() + 2UL, user_added_id);
  EXPECT_EQ(2, observer1.notifications());
  EXPECT_EQ(1, observer2.notifications());

  // Remove the first observer.
  gallery_prefs()->RemoveGalleryChangeObserver(&observer1);

  // Remove an auto added gallery (i.e. make it blacklisted).
  gallery_prefs()->ForgetGalleryById(auto_id);
  expected_galleries_[auto_id].type = MediaGalleryPrefInfo::kBlackListed;
  expected_galleries_for_all.erase(auto_id);

  EXPECT_EQ(2, observer1.notifications());
  EXPECT_EQ(2, observer2.notifications());

  // Remove a user added gallery and it should go away.
  gallery_prefs()->ForgetGalleryById(user_added_id);
  expected_galleries_.erase(user_added_id);
  expected_device_map[device_id].erase(user_added_id);

  EXPECT_EQ(2, observer1.notifications());
  EXPECT_EQ(3, observer2.notifications());
}

}  // namespace chrome
