// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_api.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_prefs_unittest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

void AddGalleryPermission(chrome::MediaGalleryPrefId gallery,
                          bool has_access,
                          std::vector<chrome::MediaGalleryPermission>* vector) {
  chrome::MediaGalleryPermission permission;
  permission.pref_id = gallery;
  permission.has_permission = has_access;
  vector->push_back(permission);
}

}  // namespace

// Test the MediaGalleries permissions functions.
class MediaGalleriesPermissionsUnittest : public ExtensionPrefsTest {
 protected:
  virtual void Initialize() OVERRIDE {
    extension1_id_ = prefs_.AddExtensionAndReturnId("test1");
    extension2_id_ = prefs_.AddExtensionAndReturnId("test2");
    extension3_id_ = prefs_.AddExtensionAndReturnId("test3");
    // Id4 isn't used to ensure that an empty permission list is ok.
    extension4_id_ = prefs_.AddExtensionAndReturnId("test4");
    Verify();

    MediaGalleriesPrivateAPI::SetMediaGalleryPermission(
        prefs(), extension1_id_, 1, false);
    AddGalleryPermission(1, false, &extension1_expectation_);
    Verify();

    MediaGalleriesPrivateAPI::SetMediaGalleryPermission(
        prefs(), extension1_id_, 2, true);
    AddGalleryPermission(2, true, &extension1_expectation_);
    Verify();

    MediaGalleriesPrivateAPI::SetMediaGalleryPermission(
        prefs(), extension1_id_, 2, false);
    extension1_expectation_[1].has_permission = false;
    Verify();

    MediaGalleriesPrivateAPI::SetMediaGalleryPermission(
        prefs(), extension2_id_, 1, true);
    MediaGalleriesPrivateAPI::SetMediaGalleryPermission(
        prefs(), extension2_id_, 3, true);
    MediaGalleriesPrivateAPI::SetMediaGalleryPermission(
        prefs(), extension2_id_, 4, true);
    AddGalleryPermission(1, true, &extension2_expectation_);
    AddGalleryPermission(3, true, &extension2_expectation_);
    AddGalleryPermission(4, true, &extension2_expectation_);
    Verify();

    MediaGalleriesPrivateAPI::SetMediaGalleryPermission(
        prefs(), extension3_id_, 3, true);
    AddGalleryPermission(3, true, &extension3_expectation_);
    Verify();

    MediaGalleriesPrivateAPI::RemoveMediaGalleryPermissions(prefs(), 3);
    extension2_expectation_.erase(extension2_expectation_.begin() + 1);
    extension3_expectation_.erase(extension3_expectation_.begin());
    Verify();

    MediaGalleriesPrivateAPI::UnsetMediaGalleryPermission(
        prefs(), extension1_id_, 1);
    extension1_expectation_.erase(extension1_expectation_.begin());
    Verify();
  }

  virtual void Verify() OVERRIDE {
    struct TestData {
      std::string* id;
      std::vector<chrome::MediaGalleryPermission>* expectation;
    };

    const TestData test_data[] = {{&extension1_id_, &extension1_expectation_},
                                  {&extension2_id_, &extension2_expectation_},
                                  {&extension3_id_, &extension3_expectation_},
                                  {&extension4_id_, &extension4_expectation_}};
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data); i++) {
      std::vector<chrome::MediaGalleryPermission> actual =
          MediaGalleriesPrivateAPI::GetMediaGalleryPermissions(
              prefs(), *test_data[i].id);
      EXPECT_EQ(test_data[i].expectation->size(), actual.size());
      for (size_t permission_entry = 0;
           permission_entry < test_data[i].expectation->size() &&
               permission_entry < actual.size();
           permission_entry++) {
        EXPECT_EQ(test_data[i].expectation->at(permission_entry).pref_id,
                  actual[permission_entry].pref_id);
        EXPECT_EQ(test_data[i].expectation->at(permission_entry).has_permission,
                  actual[permission_entry].has_permission);
      }
    }
  }

  std::string extension1_id_;
  std::string extension2_id_;
  std::string extension3_id_;
  std::string extension4_id_;

  std::vector<chrome::MediaGalleryPermission> extension1_expectation_;
  std::vector<chrome::MediaGalleryPermission> extension2_expectation_;
  std::vector<chrome::MediaGalleryPermission> extension3_expectation_;
  std::vector<chrome::MediaGalleryPermission> extension4_expectation_;
};
TEST_F(MediaGalleriesPermissionsUnittest, MediaGalleries) {}

}  // namespace extensions
