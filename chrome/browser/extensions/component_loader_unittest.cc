// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/extensions/component_loader.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/background_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/incognito_handler.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class MockExtensionService : public TestExtensionService {
 private:
  bool ready_;
  size_t unloaded_count_;
  ExtensionSet extension_set_;

 public:
  MockExtensionService() : ready_(false), unloaded_count_(0) {
  }

  virtual void AddComponentExtension(const Extension* extension) OVERRIDE {
    EXPECT_FALSE(extension_set_.Contains(extension->id()));
    // ExtensionService must become the owner of the extension object.
    extension_set_.Insert(extension);
  }

  virtual void UnloadExtension(
      const std::string& extension_id,
      extension_misc::UnloadedExtensionReason reason) OVERRIDE {
    ASSERT_TRUE(extension_set_.Contains(extension_id));
    // Remove the extension with the matching id.
    extension_set_.Remove(extension_id);
    unloaded_count_++;
  }

  virtual bool is_ready() OVERRIDE {
    return ready_;
  }

  virtual const ExtensionSet* extensions() const OVERRIDE {
    return &extension_set_;
  }

  void set_ready(bool ready) {
    ready_ = ready;
  }

  size_t unloaded_count() const {
    return unloaded_count_;
  }

  void clear_extensions() {
    extension_set_.Clear();
  }
};

}  // namespace

class ComponentLoaderTest : public testing::Test {
 public:
  ComponentLoaderTest() :
      // Note: we pass the same pref service here, to stand in for both
      // user prefs and local state.
      component_loader_(&extension_service_, &prefs_, &local_state_) {
  }

  virtual void SetUp() OVERRIDE {
    (new BackgroundManifestHandler)->Register();
    (new IncognitoHandler)->Register();

    extension_path_ =
        GetBasePath().AppendASCII("good")
                     .AppendASCII("Extensions")
                     .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                     .AppendASCII("1.0.0.0");

    // Read in the extension manifest.
    ASSERT_TRUE(file_util::ReadFileToString(
        extension_path_.Append(kManifestFilename),
        &manifest_contents_));

    // Register the user prefs that ComponentLoader will read.
    prefs_.registry()->RegisterStringPref(
        prefs::kEnterpriseWebStoreURL,
        std::string(),
        PrefRegistrySyncable::UNSYNCABLE_PREF);
    prefs_.registry()->RegisterStringPref(
        prefs::kEnterpriseWebStoreName,
        std::string(),
        PrefRegistrySyncable::UNSYNCABLE_PREF);

    // Register the local state prefs.
#if defined(OS_CHROMEOS)
    local_state_.registry()->RegisterBooleanPref(
        prefs::kSpokenFeedbackEnabled, false);
#endif
  }

  virtual void TearDown() OVERRIDE {
    ManifestHandler::ClearRegistryForTesting();
    testing::Test::TearDown();
  }

 protected:
  MockExtensionService extension_service_;
  TestingPrefServiceSyncable prefs_;
  TestingPrefServiceSimple local_state_;
  ComponentLoader component_loader_;

  // The root directory of the text extension.
  base::FilePath extension_path_;

  // The contents of the text extension's manifest file.
  std::string manifest_contents_;

  base::FilePath GetBasePath() {
    base::FilePath test_data_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    return test_data_dir.AppendASCII("extensions");
  }
};

TEST_F(ComponentLoaderTest, ParseManifest) {
  scoped_ptr<DictionaryValue> manifest;

  // Test invalid JSON.
  manifest.reset(
      component_loader_.ParseManifest("{ 'test': 3 } invalid"));
  EXPECT_FALSE(manifest.get());

  // Test manifests that are valid JSON, but don't have an object literal
  // at the root. ParseManifest() should always return NULL.

  manifest.reset(component_loader_.ParseManifest(""));
  EXPECT_FALSE(manifest.get());

  manifest.reset(component_loader_.ParseManifest("[{ \"foo\": 3 }]"));
  EXPECT_FALSE(manifest.get());

  manifest.reset(component_loader_.ParseManifest("\"Test\""));
  EXPECT_FALSE(manifest.get());

  manifest.reset(component_loader_.ParseManifest("42"));
  EXPECT_FALSE(manifest.get());

  manifest.reset(component_loader_.ParseManifest("true"));
  EXPECT_FALSE(manifest.get());

  manifest.reset(component_loader_.ParseManifest("false"));
  EXPECT_FALSE(manifest.get());

  manifest.reset(component_loader_.ParseManifest("null"));
  EXPECT_FALSE(manifest.get());

  // Test parsing valid JSON.

  int value = 0;
  manifest.reset(component_loader_.ParseManifest(
      "{ \"test\": { \"one\": 1 }, \"two\": 2 }"));
  ASSERT_TRUE(manifest.get());
  EXPECT_TRUE(manifest->GetInteger("test.one", &value));
  EXPECT_EQ(1, value);
  ASSERT_TRUE(manifest->GetInteger("two", &value));
  EXPECT_EQ(2, value);

  std::string string_value;
  manifest.reset(component_loader_.ParseManifest(manifest_contents_));
  ASSERT_TRUE(manifest->GetString("background.page", &string_value));
  EXPECT_EQ("backgroundpage.html", string_value);
}

// Test that the extension isn't loaded if the extension service isn't ready.
TEST_F(ComponentLoaderTest, AddWhenNotReady) {
  extension_service_.set_ready(false);
  std::string extension_id =
      component_loader_.Add(manifest_contents_, extension_path_);
  EXPECT_NE("", extension_id);
  EXPECT_EQ(0u, extension_service_.extensions()->size());
}

// Test that it *is* loaded when the extension service *is* ready.
TEST_F(ComponentLoaderTest, AddWhenReady) {
  extension_service_.set_ready(true);
  std::string extension_id =
      component_loader_.Add(manifest_contents_, extension_path_);
  EXPECT_NE("", extension_id);
  EXPECT_EQ(1u, extension_service_.extensions()->size());
  EXPECT_TRUE(extension_service_.extensions()->GetByID(extension_id));
}

TEST_F(ComponentLoaderTest, Remove) {
  extension_service_.set_ready(false);

  // Removing an extension that was never added should be ok.
  component_loader_.Remove(extension_path_);
  EXPECT_EQ(0u, extension_service_.extensions()->size());

  // Try adding and removing before LoadAll() is called.
  component_loader_.Add(manifest_contents_, extension_path_);
  component_loader_.Remove(extension_path_);
  component_loader_.LoadAll();
  EXPECT_EQ(0u, extension_service_.extensions()->size());

  // Load an extension, and check that it's unloaded when Remove() is called.
  extension_service_.set_ready(true);
  std::string extension_id =
      component_loader_.Add(manifest_contents_, extension_path_);
  EXPECT_EQ(1u, extension_service_.extensions()->size());
  component_loader_.Remove(extension_path_);
  EXPECT_EQ(0u, extension_service_.extensions()->size());

  // And after calling LoadAll(), it shouldn't get loaded.
  component_loader_.LoadAll();
  EXPECT_EQ(0u, extension_service_.extensions()->size());
}

TEST_F(ComponentLoaderTest, LoadAll) {
  extension_service_.set_ready(false);

  // No extensions should be loaded if none were added.
  component_loader_.LoadAll();
  EXPECT_EQ(0u, extension_service_.extensions()->size());

  // Use LoadAll() to load the default extensions.
  component_loader_.AddDefaultComponentExtensions(false);
  component_loader_.LoadAll();
  unsigned int default_count = extension_service_.extensions()->size();

  // Clear the list of loaded extensions, and reload with one more.
  extension_service_.clear_extensions();
  component_loader_.Add(manifest_contents_, extension_path_);
  component_loader_.LoadAll();

  EXPECT_EQ(default_count + 1, extension_service_.extensions()->size());
}

TEST_F(ComponentLoaderTest, RemoveAll) {
  extension_service_.set_ready(true);
  EXPECT_EQ(0u, extension_service_.extensions()->size());
  // Add all the default extensions. Since the extension service is ready, they
  // will be loaded immediately.
  component_loader_.AddDefaultComponentExtensions(false);
  unsigned int default_count = extension_service_.extensions()->size();

  // And add one more just to make sure there is anything in there in case
  // there are no defaults for this platform.
  component_loader_.Add(manifest_contents_, extension_path_);
  EXPECT_EQ(default_count + 1, extension_service_.extensions()->size());

  // Remove all default extensions.
  component_loader_.RemoveAll();
  EXPECT_EQ(0u, extension_service_.extensions()->size());
}

TEST_F(ComponentLoaderTest, EnterpriseWebStore) {
  component_loader_.AddDefaultComponentExtensions(false);
  component_loader_.LoadAll();
  unsigned int default_count = extension_service_.extensions()->size();

  // Set the pref, and it should get loaded automatically.
  extension_service_.set_ready(true);
  prefs_.SetUserPref(prefs::kEnterpriseWebStoreURL,
                     Value::CreateStringValue("http://www.google.com"));
  EXPECT_EQ(default_count + 1, extension_service_.extensions()->size());

  // Now that the pref is set, check if it's added by default.
  extension_service_.set_ready(false);
  extension_service_.clear_extensions();
  component_loader_.ClearAllRegistered();
  component_loader_.AddDefaultComponentExtensions(false);
  component_loader_.LoadAll();
  EXPECT_EQ(default_count + 1, extension_service_.extensions()->size());

  // Number of loaded extensions should be the same after changing the pref.
  prefs_.SetUserPref(prefs::kEnterpriseWebStoreURL,
                     Value::CreateStringValue("http://www.google.de"));
  EXPECT_EQ(default_count + 1, extension_service_.extensions()->size());
}

TEST_F(ComponentLoaderTest, AddOrReplace) {
  EXPECT_EQ(0u, component_loader_.registered_extensions_count());
  component_loader_.AddDefaultComponentExtensions(false);
  size_t const default_count = component_loader_.registered_extensions_count();
  base::FilePath known_extension = GetBasePath()
      .AppendASCII("override_component_extension");
  base::FilePath unknow_extension = extension_path_;

  // Replace a default component extension.
  component_loader_.AddOrReplace(known_extension);
  EXPECT_EQ(default_count,
            component_loader_.registered_extensions_count());

  // Add a new component extension.
  component_loader_.AddOrReplace(unknow_extension);
  EXPECT_EQ(default_count + 1,
            component_loader_.registered_extensions_count());

  extension_service_.set_ready(true);
  component_loader_.LoadAll();

  EXPECT_EQ(default_count + 1, extension_service_.extensions()->size());
  EXPECT_EQ(0u, extension_service_.unloaded_count());

  // replace loaded component extension.
  component_loader_.AddOrReplace(known_extension);
  EXPECT_EQ(default_count + 1, extension_service_.extensions()->size());
  EXPECT_EQ(1u, extension_service_.unloaded_count());
}

}  // namespace extensions
