// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/api/api_resource.h"
#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace utils = extension_function_test_utils;

using content::BrowserThread;

namespace extensions {

class ApiResourceManagerUnitTest : public BrowserWithTestWindowTest {
 public:
  virtual void SetUp() {
    BrowserWithTestWindowTest::SetUp();
  }
};

class FakeApiResource : public ApiResource {
 public:
  FakeApiResource(const std::string& owner_extension_id) :
      ApiResource(owner_extension_id) {}
  virtual ~FakeApiResource() {}
};

TEST_F(ApiResourceManagerUnitTest, TwoAppsCannotShareResources) {
  scoped_ptr<ApiResourceManager<FakeApiResource> > manager(
      new ApiResourceManager<FakeApiResource>(BrowserThread::UI));
  scoped_refptr<extensions::Extension> extension_one(
      utils::CreateEmptyExtension("one"));
  scoped_refptr<extensions::Extension> extension_two(
      utils::CreateEmptyExtension("two"));

  const std::string extension_one_id(extension_one->id());
  const std::string extension_two_id(extension_two->id());

  int resource_one_id = manager->Add(new FakeApiResource(extension_one_id));
  int resource_two_id = manager->Add(new FakeApiResource(extension_two_id));
  CHECK(resource_one_id);
  CHECK(resource_two_id);

  // Confirm each extension can get its own resource.
  ASSERT_TRUE(manager->Get(extension_one_id, resource_one_id) != NULL);
  ASSERT_TRUE(manager->Get(extension_two_id, resource_two_id) != NULL);

  // Confirm neither extension can get the other's resource.
  ASSERT_TRUE(manager->Get(extension_one_id, resource_two_id) == NULL);
  ASSERT_TRUE(manager->Get(extension_two_id, resource_one_id) == NULL);

  // And make sure we're not susceptible to any Jedi mind tricks.
  ASSERT_TRUE(manager->Get(std::string(), resource_one_id) == NULL);
}

}  // namespace extensions
