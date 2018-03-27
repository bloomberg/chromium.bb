// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/values.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/content_verifier/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

enum class BackgroundManifestType {
  kNone,
  kBackgroundScript,
  kBackgroundPage,
};

base::FilePath kBackgroundScriptPath(FILE_PATH_LITERAL("foo/bg.txt"));
base::FilePath kContentScriptPath(FILE_PATH_LITERAL("foo/content.txt"));
base::FilePath kBackgroundPagePath(FILE_PATH_LITERAL("foo/page.txt"));
base::FilePath kScriptFilePath(FILE_PATH_LITERAL("bar/code.js"));
base::FilePath kUnknownTypeFilePath(FILE_PATH_LITERAL("bar/code.txt"));
base::FilePath kHTMLFilePath(FILE_PATH_LITERAL("bar/page.html"));
base::FilePath kHTMFilePath(FILE_PATH_LITERAL("bar/page.htm"));
base::FilePath kIconPath(FILE_PATH_LITERAL("bar/16.png"));

class TestContentVerifierDelegate : public MockContentVerifierDelegate {
 public:
  TestContentVerifierDelegate() = default;
  ~TestContentVerifierDelegate() override = default;

  std::set<base::FilePath> GetBrowserImagePaths(
      const extensions::Extension* extension) override;

  void SetBrowserImagePaths(std::set<base::FilePath> paths);

 private:
  std::set<base::FilePath> browser_images_paths_;

  DISALLOW_COPY_AND_ASSIGN(TestContentVerifierDelegate);
};

std::set<base::FilePath> TestContentVerifierDelegate::GetBrowserImagePaths(
    const extensions::Extension* extension) {
  return std::set<base::FilePath>(browser_images_paths_);
}

void TestContentVerifierDelegate::SetBrowserImagePaths(
    std::set<base::FilePath> paths) {
  browser_images_paths_ = paths;
}

}  // namespace

class ContentVerifierTest
    : public ExtensionsTest,
      public testing::WithParamInterface<BackgroundManifestType> {
 public:
  ContentVerifierTest() {}

  void SetUp() override {
    ExtensionsTest::SetUp();
    background_manifest_type_ = GetParam();

    // Manually register handlers since the |ContentScriptsHandler| is not
    // usually registered in extensions_unittests.
    ManifestHandlerRegistry registry;
    ManifestHandlerRegistry::SetForTesting(&registry);
    (new BackgroundManifestHandler)->Register();
    (new ContentScriptsHandler)->Register();
    ManifestHandler::FinalizeRegistration();

    extension_ = CreateTestExtension();
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension_);

    auto content_verifier_delegate =
        std::make_unique<TestContentVerifierDelegate>();
    content_verifier_delegate_raw_ = content_verifier_delegate.get();

    content_verifier_ = new ContentVerifier(
        browser_context(), std::move(content_verifier_delegate));
    // |ContentVerifier::ShouldVerifyAnyPaths| always returns false if the
    // Content Verifier does not have |ContentVerifierIOData::ExtensionData|
    // for the extension.
    content_verifier_->ResetIODataForTesting(extension_.get());
  }

  void TearDown() override {
    content_verifier_->Shutdown();
    ExtensionsTest::TearDown();
  }

  void UpdateBrowserImagePaths(const std::set<base::FilePath>& paths) {
    content_verifier_delegate_raw_->SetBrowserImagePaths(paths);
    content_verifier_->ResetIODataForTesting(extension_.get());
  }

  bool ShouldVerifySinglePath(const base::FilePath& path) {
    std::set<base::FilePath> paths_to_verify;
    paths_to_verify.insert(path);
    return content_verifier_->ShouldVerifyAnyPaths(
        extension_->id(), extension_->path(), paths_to_verify);
  }

  BackgroundManifestType GetBackgroundManifestType() {
    return background_manifest_type_;
  }

 private:
  // Create a test extension with a content script and possibly a background
  // page or background script.
  scoped_refptr<Extension> CreateTestExtension() {
    base::DictionaryValue manifest;
    manifest.SetString("name", "Dummy Extension");
    manifest.SetString("version", "1");
    manifest.SetInteger("manifest_version", 2);

    if (background_manifest_type_ ==
        BackgroundManifestType::kBackgroundScript) {
      auto background_scripts = std::make_unique<base::ListValue>();
      background_scripts->AppendString("foo/bg.txt");
      manifest.Set(manifest_keys::kBackgroundScripts,
                   std::move(background_scripts));
    } else if (background_manifest_type_ ==
               BackgroundManifestType::kBackgroundPage) {
      manifest.SetString(manifest_keys::kBackgroundPage, "foo/page.txt");
    }

    auto content_scripts = std::make_unique<base::ListValue>();
    auto content_script = std::make_unique<base::DictionaryValue>();
    auto js_files = std::make_unique<base::ListValue>();
    auto matches = std::make_unique<base::ListValue>();
    js_files->AppendString("foo/content.txt");
    content_script->Set("js", std::move(js_files));
    matches->AppendString("http://*/*");
    content_script->Set("matches", std::move(matches));
    content_scripts->Append(std::move(content_script));
    manifest.Set(manifest_keys::kContentScripts, std::move(content_scripts));

    base::FilePath path;
    EXPECT_TRUE(PathService::Get(DIR_TEST_DATA, &path));

    std::string error;
    scoped_refptr<Extension> extension(Extension::Create(
        path, Manifest::INTERNAL, manifest, Extension::NO_FLAGS, &error));
    EXPECT_TRUE(extension.get()) << error;
    return extension;
  }

  BackgroundManifestType background_manifest_type_;
  scoped_refptr<ContentVerifier> content_verifier_;
  scoped_refptr<Extension> extension_;
  TestContentVerifierDelegate* content_verifier_delegate_raw_;

  DISALLOW_COPY_AND_ASSIGN(ContentVerifierTest);
};

// Verifies that |ContentVerifier::ShouldVerifyAnyPaths| returns true for
// some file paths even if those paths are specified as browser images.
TEST_P(ContentVerifierTest, BrowserImagesShouldBeVerified) {
  std::set<base::FilePath> files_to_be_verified = {
      kContentScriptPath, kScriptFilePath, kHTMLFilePath, kHTMFilePath};
  std::set<base::FilePath> files_not_to_be_verified{kIconPath,
                                                    kUnknownTypeFilePath};

  if (GetBackgroundManifestType() ==
      BackgroundManifestType::kBackgroundScript) {
    files_to_be_verified.insert(kBackgroundScriptPath);
    files_not_to_be_verified.insert(kBackgroundPagePath);
  } else if (GetBackgroundManifestType() ==
             BackgroundManifestType::kBackgroundPage) {
    files_to_be_verified.insert(kBackgroundPagePath);
    files_not_to_be_verified.insert(kBackgroundScriptPath);
  } else {
    files_not_to_be_verified.insert(kBackgroundScriptPath);
    files_not_to_be_verified.insert(kBackgroundPagePath);
  }

  for (const base::FilePath& path : files_to_be_verified) {
    EXPECT_TRUE(ShouldVerifySinglePath(path)) << "for path " << path;
    UpdateBrowserImagePaths(std::set<base::FilePath>{path});
    EXPECT_TRUE(ShouldVerifySinglePath(path)) << "for path " << path;
  }

  for (const base::FilePath& path : files_not_to_be_verified) {
    EXPECT_TRUE(ShouldVerifySinglePath(path)) << "for path " << path;
    UpdateBrowserImagePaths(std::set<base::FilePath>{path});
    EXPECT_FALSE(ShouldVerifySinglePath(path)) << "for path " << path;
  }
}

INSTANTIATE_TEST_CASE_P(
    All,
    ContentVerifierTest,
    testing::Values(BackgroundManifestType::kNone,
                    BackgroundManifestType::kBackgroundScript,
                    BackgroundManifestType::kBackgroundPage));

}  // namespace extensions
