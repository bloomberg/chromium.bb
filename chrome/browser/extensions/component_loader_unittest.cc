// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/extensions/component_loader.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockExtensionService : public TestExtensionService {
 private:
  bool ready_;
  ExtensionList extension_list_;

 public:
  MockExtensionService() : ready_(false) {
  }

  virtual void AddExtension(const Extension* extension) OVERRIDE {
    // ExtensionService must become the owner of the extension object.
    extension_list_.push_back(extension);
  }

  virtual void UnloadExtension(
      const std::string& extension_id,
      extension_misc::UnloadedExtensionReason reason) OVERRIDE {
    // Remove the extension with the matching id.
    for (ExtensionList::iterator it = extension_list_.begin();
         it != extension_list_.end();
         ++it) {
      if ((*it)->id() == extension_id) {
        extension_list_.erase(it);
        return;
      }
    }
  }

  virtual bool is_ready() OVERRIDE {
    return ready_;
  }

  virtual const ExtensionList* extensions() const OVERRIDE {
    return &extension_list_;
  }

  void set_ready(bool ready) {
    ready_ = ready;
  }

  void clear_extension_list() {
    extension_list_.clear();
  }
};

}  // namespace

namespace extensions {

class ComponentLoaderTest : public testing::Test {
 public:
  ComponentLoaderTest() :
      // Note: we pass the same pref service here, to stand in for both
      // user prefs and local state.
      component_loader_(&extension_service_, &prefs_, &prefs_) {
  }

  void SetUp() {
    FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    extension_path_ =
        test_data_dir.AppendASCII("extensions")
                     .AppendASCII("good")
                     .AppendASCII("Extensions")
                     .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                     .AppendASCII("1.0.0.0");

    // Read in the extension manifest.
    ASSERT_TRUE(file_util::ReadFileToString(
        extension_path_.Append(Extension::kManifestFilename),
                               &manifest_contents_));

    // Register the user prefs that ComponentLoader will read.
    prefs_.RegisterStringPref(prefs::kEnterpriseWebStoreURL, std::string());
    prefs_.RegisterStringPref(prefs::kEnterpriseWebStoreName, std::string());

    // Register the local state prefs.
#if defined(OS_CHROMEOS)
    prefs_.RegisterBooleanPref(prefs::kAccessibilityEnabled, false);
#endif
  }

 protected:
  MockExtensionService extension_service_;
  TestingPrefService prefs_;
  ComponentLoader component_loader_;

  // The root directory of the text extension.
  FilePath extension_path_;

  // The contents of the text extension's manifest file.
  std::string manifest_contents_;
};

TEST_F(ComponentLoaderTest, ParseManifest) {
  scoped_ptr<DictionaryValue> manifest;

  // Test invalid JSON.
  manifest.reset(
      component_loader_.ParseManifest("{ 'test': 3 } invalid"));
  ASSERT_EQ((DictionaryValue*)NULL, manifest.get());

  // Test manifests that are valid JSON, but don't have an object literal
  // at the root. ParseManifest() should always return NULL.

  manifest.reset(component_loader_.ParseManifest(""));
  ASSERT_EQ((DictionaryValue*)NULL, manifest.get());

  manifest.reset(component_loader_.ParseManifest("[{ \"foo\": 3 }]"));
  ASSERT_EQ((DictionaryValue*)NULL, manifest.get());

  manifest.reset(component_loader_.ParseManifest("\"Test\""));
  ASSERT_EQ((DictionaryValue*)NULL, manifest.get());

  manifest.reset(component_loader_.ParseManifest("42"));
  ASSERT_EQ((DictionaryValue*)NULL, manifest.get());

  manifest.reset(component_loader_.ParseManifest("true"));
  ASSERT_EQ((DictionaryValue*)NULL, manifest.get());

  manifest.reset(component_loader_.ParseManifest("false"));
  ASSERT_EQ((DictionaryValue*)NULL, manifest.get());

  manifest.reset(component_loader_.ParseManifest("null"));
  ASSERT_EQ((DictionaryValue*)NULL, manifest.get());

  // Test parsing valid JSON.

  int value;
  manifest.reset(component_loader_.ParseManifest(
      "{ \"test\": { \"one\": 1 }, \"two\": 2 }"));
  ASSERT_NE(manifest.get(), (DictionaryValue*)NULL);
  ASSERT_TRUE(manifest->GetInteger("test.one", &value));
  ASSERT_EQ(1, value);
  ASSERT_TRUE(manifest->GetInteger("two", &value));
  ASSERT_EQ(2, value);

  std::string string_value;
  manifest.reset(component_loader_.ParseManifest(manifest_contents_));
  ASSERT_TRUE(manifest->GetString("background_page", &string_value));
  ASSERT_EQ("backgroundpage.html", string_value);
}

// Test that the extension isn't loaded if the extension service isn't ready.
TEST_F(ComponentLoaderTest, AddWhenNotReady) {
  scoped_refptr<const Extension> extension;
  extension_service_.set_ready(false);
  extension = component_loader_.Add(manifest_contents_, extension_path_);
  ASSERT_EQ((Extension*)NULL, extension.get());
  ASSERT_EQ(0U, extension_service_.extensions()->size());
}

// Test that it *is* loaded when the extension service *is* ready.
TEST_F(ComponentLoaderTest, AddWhenReady) {
  scoped_refptr<const Extension> extension;
  extension_service_.set_ready(true);
  extension = component_loader_.Add(manifest_contents_, extension_path_);
  ASSERT_NE((Extension*)NULL, extension.get());
  ASSERT_EQ(1U, extension_service_.extensions()->size());
}

TEST_F(ComponentLoaderTest, Remove) {
  extension_service_.set_ready(false);

  // Removing an extension that was never added should be ok.
  component_loader_.Remove(extension_path_);
  ASSERT_EQ(0U, extension_service_.extensions()->size());

  // Try adding and removing before LoadAll() is called.
  component_loader_.Add(manifest_contents_, extension_path_);
  component_loader_.Remove(extension_path_);
  component_loader_.LoadAll();
  ASSERT_EQ(0U, extension_service_.extensions()->size());

  // Load an extension, and check that it's unloaded when Remove() is called.
  scoped_refptr<const Extension> extension;
  extension_service_.set_ready(true);
  extension = component_loader_.Add(manifest_contents_, extension_path_);
  ASSERT_NE((Extension*)NULL, extension.get());
  component_loader_.Remove(extension_path_);
  ASSERT_EQ(0U, extension_service_.extensions()->size());

  // And after calling LoadAll(), it shouldn't get loaded.
  component_loader_.LoadAll();
  ASSERT_EQ(0U, extension_service_.extensions()->size());
}

TEST_F(ComponentLoaderTest, LoadAll) {
  extension_service_.set_ready(false);

  // No extensions should be loaded if none were added.
  component_loader_.LoadAll();
  ASSERT_EQ(0U, extension_service_.extensions()->size());

  // Use LoadAll() to load the default extensions.
  component_loader_.AddDefaultComponentExtensions();
  component_loader_.LoadAll();
  unsigned int default_count = extension_service_.extensions()->size();

  // Clear the list of loaded extensions, and reload with one more.
  extension_service_.clear_extension_list();
  component_loader_.Add(manifest_contents_, extension_path_);
  component_loader_.LoadAll();

  ASSERT_EQ(default_count + 1, extension_service_.extensions()->size());
}

TEST_F(ComponentLoaderTest, EnterpriseWebStore) {
  component_loader_.AddDefaultComponentExtensions();
  component_loader_.LoadAll();
  unsigned int default_count = extension_service_.extensions()->size();

  // Set the pref, and it should get loaded automatically.
  extension_service_.set_ready(true);
  prefs_.SetUserPref(prefs::kEnterpriseWebStoreURL,
                     Value::CreateStringValue("http://www.google.com"));
  ASSERT_EQ(default_count + 1, extension_service_.extensions()->size());

  // Now that the pref is set, check if it's added by default.
  extension_service_.set_ready(false);
  extension_service_.clear_extension_list();
  component_loader_.ClearAllRegistered();
  component_loader_.AddDefaultComponentExtensions();
  component_loader_.LoadAll();
  ASSERT_EQ(default_count + 1, extension_service_.extensions()->size());

  // Number of loaded extensions should be the same after changing the pref.
  prefs_.SetUserPref(prefs::kEnterpriseWebStoreURL,
                     Value::CreateStringValue("http://www.google.de"));
  ASSERT_EQ(default_count + 1, extension_service_.extensions()->size());
}

}  // namespace extensions
