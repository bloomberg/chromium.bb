// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/chooser_context_base.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char* kRequiredKey1 = "key-1";
const char* kRequiredKey2 = "key-2";

class TestChooserContext : public ChooserContextBase {
 public:
  // This class uses the USB content settings type for testing purposes only.
  explicit TestChooserContext(Profile* profile)
      : ChooserContextBase(profile, CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA) {}
  ~TestChooserContext() override {}

  bool IsValidObject(const base::DictionaryValue& object) override {
    return object.size() == 2 && object.HasKey(kRequiredKey1) &&
           object.HasKey(kRequiredKey2);
  }
};

}  // namespace

class ChooserContextBaseTest : public testing::Test {
 public:
  ChooserContextBaseTest()
      : context_(&profile_),
        origin1_("https://google.com"),
        origin2_("https://chromium.org") {
    object1_.SetString(kRequiredKey1, "value1");
    object1_.SetString(kRequiredKey2, "value2");
    object2_.SetString(kRequiredKey1, "value3");
    object2_.SetString(kRequiredKey2, "value4");
  }

  ~ChooserContextBaseTest() override {}

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;

 protected:
  TestChooserContext context_;
  GURL origin1_;
  GURL origin2_;
  base::DictionaryValue object1_;
  base::DictionaryValue object2_;
};

TEST_F(ChooserContextBaseTest, GrantAndRevokeObjectPermissions) {
  context_.GrantObjectPermission(origin1_, origin1_, object1_.CreateDeepCopy());
  context_.GrantObjectPermission(origin1_, origin1_, object2_.CreateDeepCopy());

  std::vector<std::unique_ptr<base::DictionaryValue>> objects =
      context_.GetGrantedObjects(origin1_, origin1_);
  EXPECT_EQ(2u, objects.size());
  EXPECT_TRUE(object1_.Equals(objects[0].get()));
  EXPECT_TRUE(object2_.Equals(objects[1].get()));

  // Granting permission to one origin should not grant them to another.
  objects = context_.GetGrantedObjects(origin2_, origin2_);
  EXPECT_EQ(0u, objects.size());

  // Nor when the original origin is embedded in another.
  objects = context_.GetGrantedObjects(origin1_, origin2_);
  EXPECT_EQ(0u, objects.size());
}

TEST_F(ChooserContextBaseTest, GrantObjectPermissionTwice) {
  context_.GrantObjectPermission(origin1_, origin1_, object1_.CreateDeepCopy());
  context_.GrantObjectPermission(origin1_, origin1_, object1_.CreateDeepCopy());

  std::vector<std::unique_ptr<base::DictionaryValue>> objects =
      context_.GetGrantedObjects(origin1_, origin1_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_TRUE(object1_.Equals(objects[0].get()));

  context_.RevokeObjectPermission(origin1_, origin1_, object1_);
  objects = context_.GetGrantedObjects(origin1_, origin1_);
  EXPECT_EQ(0u, objects.size());
}

TEST_F(ChooserContextBaseTest, GrantObjectPermissionEmbedded) {
  context_.GrantObjectPermission(origin1_, origin2_, object1_.CreateDeepCopy());

  std::vector<std::unique_ptr<base::DictionaryValue>> objects =
      context_.GetGrantedObjects(origin1_, origin2_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_TRUE(object1_.Equals(objects[0].get()));

  // The embedding origin still does not have permission.
  objects = context_.GetGrantedObjects(origin2_, origin2_);
  EXPECT_EQ(0u, objects.size());

  // The requesting origin also doesn't have permission when not embedded.
  objects = context_.GetGrantedObjects(origin1_, origin1_);
  EXPECT_EQ(0u, objects.size());
}

TEST_F(ChooserContextBaseTest, GetAllGrantedObjects) {
  context_.GrantObjectPermission(origin1_, origin1_, object1_.CreateDeepCopy());
  context_.GrantObjectPermission(origin2_, origin2_, object2_.CreateDeepCopy());

  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      context_.GetAllGrantedObjects();
  EXPECT_EQ(2u, objects.size());
  bool found_one = false;
  bool found_two = false;
  for (const auto& object : objects) {
    if (object->requesting_origin == origin1_) {
      EXPECT_FALSE(found_one);
      EXPECT_EQ(origin1_, object->embedding_origin);
      EXPECT_TRUE(object->object.Equals(&object1_));
      found_one = true;
    } else if (object->requesting_origin == origin2_) {
      EXPECT_FALSE(found_two);
      EXPECT_EQ(origin2_, object->embedding_origin);
      EXPECT_TRUE(object->object.Equals(&object2_));
      found_two = true;
    } else {
      ADD_FAILURE() << "Unexpected object.";
    }
  }
  EXPECT_TRUE(found_one);
  EXPECT_TRUE(found_two);
}
