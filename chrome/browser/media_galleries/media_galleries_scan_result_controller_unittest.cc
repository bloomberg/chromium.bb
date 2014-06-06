// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller_test_util.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/media_galleries/media_galleries_scan_result_controller.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/media_galleries_permission.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

class MediaGalleriesScanResultControllerTest : public testing::Test {
 public:
  MediaGalleriesScanResultControllerTest()
      : dialog_(NULL),
        dialog_update_count_at_destruction_(0),
        controller_(NULL),
        profile_(new TestingProfile()),
        weak_factory_(this) {
  }

  virtual ~MediaGalleriesScanResultControllerTest() {
    EXPECT_FALSE(controller_);
    EXPECT_FALSE(dialog_);
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(storage_monitor::TestStorageMonitor::CreateAndInstall());

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_.get())));
    extension_system->CreateExtensionService(
        CommandLine::ForCurrentProcess(), base::FilePath(), false);

    gallery_prefs_.reset(new MediaGalleriesPreferences(profile_.get()));
    base::RunLoop loop;
    gallery_prefs_->EnsureInitialized(loop.QuitClosure());
    loop.Run();

    std::vector<std::string> read_permissions;
    read_permissions.push_back(
        extensions::MediaGalleriesPermission::kReadPermission);
    extension_ = AddMediaGalleriesApp("read", read_permissions, profile_.get());
  }

  virtual void TearDown() OVERRIDE {
    storage_monitor::TestStorageMonitor::Destroy();
  }

  void StartDialog() {
    ASSERT_FALSE(controller_);
    controller_ = new MediaGalleriesScanResultController(
        *extension_.get(),
        gallery_prefs_.get(),
        base::Bind(
            &MediaGalleriesScanResultControllerTest::CreateMockDialog,
            base::Unretained(this)),
        base::Bind(
            &MediaGalleriesScanResultControllerTest::OnControllerDone,
            base::Unretained(this)));
  }

  size_t GetFirstSectionSize() const {
    return controller()->GetSectionEntries(0).size();
  }

  MediaGalleriesScanResultController* controller() const {
    return controller_;
  }

  MockMediaGalleriesDialog* dialog() {
    return dialog_;
  }

  int dialog_update_count_at_destruction() {
    EXPECT_FALSE(dialog_);
    return dialog_update_count_at_destruction_;
  }

  extensions::Extension* extension() {
    return extension_.get();
  }

  MediaGalleriesPreferences* gallery_prefs() {
    return gallery_prefs_.get();
  }

  MediaGalleryPrefId AddGallery(const std::string& path,
                                MediaGalleryPrefInfo::Type type,
                                int audio_count, int image_count,
                                int video_count) {
    MediaGalleryPrefInfo gallery_info;
    gallery_prefs_->LookUpGalleryByPath(MakeMediaGalleriesTestingPath(path),
                                        &gallery_info);
    return gallery_prefs_->AddGallery(
        gallery_info.device_id,
        gallery_info.path,
        type,
        gallery_info.volume_label,
        gallery_info.vendor_name,
        gallery_info.model_name,
        gallery_info.total_size_in_bytes,
        gallery_info.last_attach_time,
        audio_count, image_count, video_count);
  }

  MediaGalleryPrefId AddScanResult(const std::string& path, int audio_count,
                                   int image_count, int video_count) {
    return AddGallery(path, MediaGalleryPrefInfo::kScanResult, audio_count,
                      image_count, video_count);
  }

 private:
  MediaGalleriesDialog* CreateMockDialog(
      MediaGalleriesDialogController* controller) {
    EXPECT_FALSE(dialog_);
    dialog_update_count_at_destruction_ = 0;
    dialog_ = new MockMediaGalleriesDialog(base::Bind(
        &MediaGalleriesScanResultControllerTest::OnDialogDestroyed,
        weak_factory_.GetWeakPtr()));
    return dialog_;
  }

  void OnDialogDestroyed(int update_count) {
    EXPECT_TRUE(dialog_);
    dialog_update_count_at_destruction_ = update_count;
    dialog_ = NULL;
  }

  void OnControllerDone() {
    controller_ = NULL;
  }

  // Needed for extension service & friends to work.
  content::TestBrowserThreadBundle thread_bundle_;

  // The dialog is owned by the controller, but this pointer should only be
  // valid while the dialog is live within the controller.
  MockMediaGalleriesDialog* dialog_;
  int dialog_update_count_at_destruction_;

  // The controller owns itself.
  MediaGalleriesScanResultController* controller_;

  scoped_refptr<extensions::Extension> extension_;

  EnsureMediaDirectoriesExists mock_gallery_locations_;

#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif

  storage_monitor::TestStorageMonitor monitor_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<MediaGalleriesPreferences> gallery_prefs_;

  base::WeakPtrFactory<MediaGalleriesScanResultControllerTest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesScanResultControllerTest);
};

TEST_F(MediaGalleriesScanResultControllerTest, EmptyDialog) {
  StartDialog();
  EXPECT_TRUE(controller());
  EXPECT_TRUE(dialog());
  EXPECT_EQ(0U, GetFirstSectionSize());

  controller()->DialogFinished(true);
  EXPECT_FALSE(controller());
  EXPECT_FALSE(dialog());
  EXPECT_EQ(0, dialog_update_count_at_destruction());
}

TEST_F(MediaGalleriesScanResultControllerTest, AddScanResults) {
  // Start with two scan results.
  MediaGalleryPrefId scan_id = AddScanResult("scan_id", 1, 0, 0);
  MediaGalleryPrefId auto_id =
      AddGallery("auto_id", MediaGalleryPrefInfo::kAutoDetected, 2, 0, 0);
  EXPECT_EQ(0U, gallery_prefs()->GalleriesForExtension(*extension()).size());

  // Show the dialog, but cancel it.
  StartDialog();
  EXPECT_EQ(2U, GetFirstSectionSize());
  controller()->DialogFinished(false);
  EXPECT_EQ(0U, gallery_prefs()->GalleriesForExtension(*extension()).size());

  // Show the dialog, unselect both and accept it.
  StartDialog();
  EXPECT_EQ(2U, GetFirstSectionSize());
  controller()->DidToggleEntry(scan_id, false);
  controller()->DidToggleEntry(auto_id, false);
  controller()->DialogFinished(true);
  EXPECT_EQ(0U, gallery_prefs()->GalleriesForExtension(*extension()).size());

  // Show the dialog, leave one selected and accept it.
  StartDialog();
  EXPECT_EQ(2U, GetFirstSectionSize());
  controller()->DidToggleEntry(scan_id, false);
  controller()->DialogFinished(true);
  MediaGalleryPrefIdSet permitted =
      gallery_prefs()->GalleriesForExtension(*extension());
  ASSERT_EQ(1U, permitted.size());
  EXPECT_EQ(auto_id, *permitted.begin());

  // Show the dialog, toggle the remaining entry twice and then accept it.
  StartDialog();
  EXPECT_EQ(1U, GetFirstSectionSize());
  controller()->DidToggleEntry(scan_id, false);
  controller()->DidToggleEntry(scan_id, true);
  controller()->DialogFinished(true);
  EXPECT_EQ(2U, gallery_prefs()->GalleriesForExtension(*extension()).size());
}

TEST_F(MediaGalleriesScanResultControllerTest, Blacklisted) {
  // Start with two scan results.
  MediaGalleryPrefId scan_id = AddScanResult("scan_id", 1, 0, 0);
  MediaGalleryPrefId auto_id =
      AddGallery("auto_id", MediaGalleryPrefInfo::kAutoDetected, 2, 0, 0);
  EXPECT_EQ(0U, gallery_prefs()->GalleriesForExtension(*extension()).size());

  // Show the dialog, but cancel it.
  StartDialog();
  EXPECT_EQ(2U, GetFirstSectionSize());
  controller()->DialogFinished(false);
  EXPECT_EQ(0U, gallery_prefs()->GalleriesForExtension(*extension()).size());

  // Blacklist one and try again.
  gallery_prefs()->ForgetGalleryById(scan_id);
  StartDialog();
  EXPECT_EQ(1U, GetFirstSectionSize());
  controller()->DialogFinished(false);

  // Adding it as a user gallery should change its type.
  AddGallery("scan_id", MediaGalleryPrefInfo::kUserAdded, 1, 0, 0);
  StartDialog();
  EXPECT_EQ(2U, GetFirstSectionSize());

  // Blacklisting the other while the dialog is open should remove it.
  gallery_prefs()->ForgetGalleryById(auto_id);
  EXPECT_EQ(1U, GetFirstSectionSize());
  controller()->DialogFinished(false);
  EXPECT_EQ(0U, gallery_prefs()->GalleriesForExtension(*extension()).size());
  EXPECT_EQ(1, dialog_update_count_at_destruction());
}

TEST_F(MediaGalleriesScanResultControllerTest, PrefUpdates) {
  MediaGalleryPrefId selected = AddScanResult("selected", 1, 0, 0);
  MediaGalleryPrefId unselected = AddScanResult("unselected", 1, 0, 0);
  MediaGalleryPrefId selected_add_permission =
      AddScanResult("selected_add_permission", 1, 0, 0);
  MediaGalleryPrefId unselected_add_permission =
      AddScanResult("unselected_add_permission", 1, 0, 0);
  MediaGalleryPrefId selected_removed =
      AddScanResult("selected_removed", 1, 0, 0);
  MediaGalleryPrefId unselected_removed =
      AddScanResult("unselected_removed", 1, 0, 0);
  MediaGalleryPrefId selected_update =
      AddScanResult("selected_update", 1, 0, 0);
  MediaGalleryPrefId unselected_update =
      AddScanResult("unselected_update", 1, 0, 0);

  gallery_prefs()->AddGalleryByPath(MakeMediaGalleriesTestingPath("user"),
                                    MediaGalleryPrefInfo::kUserAdded);
  gallery_prefs()->AddGalleryByPath(
      MakeMediaGalleriesTestingPath("auto_detected"),
      MediaGalleryPrefInfo::kAutoDetected);
  MediaGalleryPrefId blacklisted = gallery_prefs()->AddGalleryByPath(
      MakeMediaGalleriesTestingPath("blacklisted"),
      MediaGalleryPrefInfo::kAutoDetected);
  gallery_prefs()->ForgetGalleryById(blacklisted);
  EXPECT_EQ(0U, gallery_prefs()->GalleriesForExtension(*extension()).size());

  StartDialog();
  EXPECT_EQ(8U, GetFirstSectionSize());
  controller()->DidToggleEntry(unselected, false);
  controller()->DidToggleEntry(unselected_add_permission, false);
  controller()->DidToggleEntry(unselected_removed, false);
  controller()->DidToggleEntry(unselected_update, false);
  EXPECT_EQ(0, dialog()->update_count());
  EXPECT_EQ(8U, GetFirstSectionSize());

  // Add permission.
  gallery_prefs()->SetGalleryPermissionForExtension(*extension(),
                                                    unselected_add_permission,
                                                    true);
  EXPECT_EQ(1, dialog()->update_count());
  EXPECT_EQ(7U, GetFirstSectionSize());
  gallery_prefs()->SetGalleryPermissionForExtension(*extension(),
                                                    selected_add_permission,
                                                    true);
  EXPECT_EQ(2, dialog()->update_count());
  EXPECT_EQ(6U, GetFirstSectionSize());

  // Blacklist scan results.
  gallery_prefs()->ForgetGalleryById(unselected_removed);
  EXPECT_EQ(3, dialog()->update_count());
  EXPECT_EQ(5U, GetFirstSectionSize());
  gallery_prefs()->ForgetGalleryById(selected_removed);
  EXPECT_EQ(4, dialog()->update_count());
  EXPECT_EQ(4U, GetFirstSectionSize());

  // Update names.
  const MediaGalleryPrefInfo& unselected_update_info =
      gallery_prefs()->known_galleries().find(unselected_update)->second;
  gallery_prefs()->AddGallery(
      unselected_update_info.device_id, base::FilePath(),
      MediaGalleryPrefInfo::kScanResult,
      base::ASCIIToUTF16("Updated & Unselected"),
      base::string16(), base::string16(), 0, base::Time(), 1, 0, 0);
  EXPECT_EQ(5, dialog()->update_count());
  EXPECT_EQ(4U, GetFirstSectionSize());
  const MediaGalleryPrefInfo& selected_update_info =
      gallery_prefs()->known_galleries().find(selected_update)->second;
  gallery_prefs()->AddGallery(
      selected_update_info.device_id, base::FilePath(),
      MediaGalleryPrefInfo::kScanResult,
      base::ASCIIToUTF16("Updated & Selected"),
      base::string16(), base::string16(), 0, base::Time(), 1, 0, 0);
  EXPECT_EQ(6, dialog()->update_count());
  EXPECT_EQ(4U, GetFirstSectionSize());

  MediaGalleriesDialogController::Entries results =
      controller()->GetSectionEntries(0);
  EXPECT_EQ(selected, results[0].pref_info.pref_id);
  EXPECT_TRUE(results[0].selected);
  EXPECT_EQ(selected_update, results[1].pref_info.pref_id);
  EXPECT_TRUE(results[1].selected);
  EXPECT_EQ(base::ASCIIToUTF16("Updated & Selected"),
            results[1].pref_info.volume_label);
  EXPECT_EQ(unselected, results[2].pref_info.pref_id);
  EXPECT_FALSE(results[2].selected);
  EXPECT_EQ(unselected_update, results[3].pref_info.pref_id);
  EXPECT_FALSE(results[3].selected);
  EXPECT_EQ(base::ASCIIToUTF16("Updated & Unselected"),
            results[3].pref_info.volume_label);

  controller()->DialogFinished(true);
  EXPECT_EQ(4U, gallery_prefs()->GalleriesForExtension(*extension()).size());
  StartDialog();
  EXPECT_EQ(2U, GetFirstSectionSize());
  controller()->DialogFinished(false);
}

TEST_F(MediaGalleriesScanResultControllerTest, ForgetGallery) {
  // Start with two scan results.
  MediaGalleryPrefId scan1 = AddScanResult("scan1", 1, 0, 0);
  MediaGalleryPrefId scan2 = AddScanResult("scan2", 2, 0, 0);
  EXPECT_EQ(0U, gallery_prefs()->GalleriesForExtension(*extension()).size());

  // Remove one and then cancel.
  StartDialog();
  EXPECT_EQ(2U, GetFirstSectionSize());
  controller()->DidForgetEntry(scan1);
  controller()->DialogFinished(false);
  EXPECT_EQ(0U, gallery_prefs()->GalleriesForExtension(*extension()).size());

  // Remove one and then have it blacklisted from prefs.
  StartDialog();
  EXPECT_EQ(2U, GetFirstSectionSize());
  controller()->DidForgetEntry(scan1);
  EXPECT_EQ(1, dialog()->update_count());
  controller()->DidToggleEntry(scan2, false);  // Uncheck the second.
  gallery_prefs()->ForgetGalleryById(scan1);
  controller()->DialogFinished(true);
  EXPECT_EQ(0U, gallery_prefs()->GalleriesForExtension(*extension()).size());
  EXPECT_EQ(2, dialog_update_count_at_destruction());

  // Remove the other.
  StartDialog();
  EXPECT_EQ(1U, GetFirstSectionSize());
  controller()->DidForgetEntry(scan2);
  controller()->DialogFinished(true);
  EXPECT_EQ(0U, gallery_prefs()->GalleriesForExtension(*extension()).size());

  // Check that nothing shows up.
  StartDialog();
  EXPECT_EQ(0U, GetFirstSectionSize());
  controller()->DialogFinished(false);
}

TEST_F(MediaGalleriesScanResultControllerTest, SortOrder) {
  // Intentionally out of order numerically and alphabetically.
  MediaGalleryPrefId third = AddScanResult("third", 2, 2, 2);
  MediaGalleryPrefId second =
      AddGallery("second", MediaGalleryPrefInfo::kAutoDetected, 9, 0, 0);
  MediaGalleryPrefId first = AddScanResult("first", 8, 2, 3);
  MediaGalleryPrefId fifth = AddScanResult("abb", 3, 0, 0);
  MediaGalleryPrefId fourth = AddScanResult("aaa", 3, 0, 0);

  StartDialog();
  MediaGalleriesDialogController::Entries results =
      controller()->GetSectionEntries(0);
  ASSERT_EQ(5U, results.size());
  EXPECT_EQ(first, results[0].pref_info.pref_id);
  EXPECT_EQ(second, results[1].pref_info.pref_id);
  EXPECT_EQ(third, results[2].pref_info.pref_id);
  EXPECT_EQ(fourth, results[3].pref_info.pref_id);
  EXPECT_EQ(fifth, results[4].pref_info.pref_id);
  controller()->DialogFinished(false);
}
