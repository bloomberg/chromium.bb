// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/srt_chrome_prompt_impl_win.h"

#include <vector>

#include "base/feature_list.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/mock_extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class ExtensionDeletionTest : public extensions::ExtensionServiceTestBase {
 public:
  ~ExtensionDeletionTest() override = default;

  void SetUp() override {
    const std::vector<base::Feature> enabled_features = GetEnabledFeatures();
    const std::vector<base::Feature> disabled_features = GetDisabledFeatures();

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    ExtensionServiceTestBase::SetUp();
  }

 protected:
  // Protected constructor to make this class abstract. Following
  // implementations will be explicit about the feature flag state.
  ExtensionDeletionTest() { InitializeEmptyExtensionService(); }

  // Hooks to set up feature flags.
  virtual const std::vector<base::Feature> GetEnabledFeatures() const {
    return {};
  }

  virtual const std::vector<base::Feature> GetDisabledFeatures() const {
    return {};
  }

  // Creates some extension IDs and registers them in the service.
  std::vector<base::string16> PopulateExtensionIds(
      bool installExtensions = true) {
    std::vector<base::string16> extension_ids{};
    extensions::ExtensionService* extension_service = this->service();
    for (int i = 40; i < 43; i++) {
      scoped_refptr<const extensions::Extension> extension =
          extensions::ExtensionBuilder(base::NumberToString(i))
              .SetManifestKey("version", "1")
              .Build();
      auto id = extension->id();
      extension_ids.push_back(base::UTF8ToUTF16(id));
      if (installExtensions) {
        extension_service->AddExtension(extension.get());
        extension_service->EnableExtension(id);
      }
    }

    return extension_ids;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(ExtensionDeletionTest);
};

class ExtensionDeletionEnabledTest : public ExtensionDeletionTest {
 public:
  ExtensionDeletionEnabledTest() = default;
  ~ExtensionDeletionEnabledTest() override = default;

 protected:
  const std::vector<base::Feature> GetEnabledFeatures() const override {
    return {kChromeCleanupExtensionsFeature};
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionDeletionEnabledTest);
};

class ExtensionDeletionDisabledTest : public ExtensionDeletionTest {
 public:
  ExtensionDeletionDisabledTest() = default;
  ~ExtensionDeletionDisabledTest() override = default;

 protected:
  const std::vector<base::Feature> GetDisabledFeatures() const override {
    return {kChromeCleanupExtensionsFeature};
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionDeletionDisabledTest);
};

TEST_F(ExtensionDeletionEnabledTest, DisableExtensionTest) {
  std::vector<base::string16> extension_ids = PopulateExtensionIds();
  extensions::ExtensionService* extension_service = this->service();
  std::unique_ptr<ChromePromptImpl> chrome_prompt =
      std::make_unique<ChromePromptImpl>(extension_service, nullptr,
                                         base::DoNothing(), base::DoNothing());

  chrome_prompt->PromptUser({}, {}, extension_ids, base::DoNothing());
  std::vector<base::string16> extensions_to_disable{extension_ids[0]};
  chrome_prompt->DisableExtensions(
      extensions_to_disable,
      base::BindOnce([](bool result) { EXPECT_TRUE(result); }));
  EXPECT_EQ(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[0])),
            nullptr);
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[1])),
            nullptr);
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[2])),
            nullptr);

  chrome_prompt = std::make_unique<ChromePromptImpl>(
      extension_service, nullptr, base::DoNothing(), base::DoNothing());
  chrome_prompt->PromptUser({}, {}, extension_ids, base::DoNothing());
  extensions_to_disable = {extension_ids[2], extension_ids[1]};
  chrome_prompt->DisableExtensions(
      extensions_to_disable,
      base::BindOnce([](bool result) { EXPECT_TRUE(result); }));
  EXPECT_EQ(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[0])),
            nullptr);
  EXPECT_EQ(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[1])),
            nullptr);
  EXPECT_EQ(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[2])),
            nullptr);
}

TEST_F(ExtensionDeletionEnabledTest, CantDeleteNonPromptedExtensions) {
  std::vector<base::string16> extension_ids = PopulateExtensionIds();
  extensions::ExtensionService* extension_service = this->service();
  std::unique_ptr<ChromePromptImpl> chrome_prompt =
      std::make_unique<ChromePromptImpl>(extension_service, nullptr,
                                         base::DoNothing(), base::DoNothing());

  std::vector<base::string16> extensions_to_disable{extension_ids[0]};
  chrome_prompt->DisableExtensions(
      extensions_to_disable,
      base::BindOnce([](bool result) { EXPECT_FALSE(result); }));
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[0])),
            nullptr);
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[1])),
            nullptr);
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[2])),
            nullptr);

  chrome_prompt = std::make_unique<ChromePromptImpl>(
      extension_service, nullptr, base::DoNothing(), base::DoNothing());
  chrome_prompt->DisableExtensions(
      extension_ids, base::BindOnce([](bool result) { EXPECT_FALSE(result); }));
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[0])),
            nullptr);
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[1])),
            nullptr);
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[2])),
            nullptr);

  chrome_prompt->PromptUser({}, {}, {{extension_ids[2]}}, base::DoNothing());
  chrome_prompt->DisableExtensions(
      {extension_ids[0], extension_ids[1]},
      base::BindOnce([](bool result) { EXPECT_FALSE(result); }));
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[0])),
            nullptr);
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[1])),
            nullptr);
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[2])),
            nullptr);
}

TEST_F(ExtensionDeletionEnabledTest, EmptyDeletionTest) {
  std::vector<base::string16> extension_ids = PopulateExtensionIds();
  extensions::ExtensionService* extension_service = this->service();
  std::unique_ptr<ChromePromptImpl> chrome_prompt =
      std::make_unique<ChromePromptImpl>(extension_service, nullptr,
                                         base::DoNothing(), base::DoNothing());

  chrome_prompt->PromptUser({}, {}, extension_ids, base::DoNothing());
  chrome_prompt->DisableExtensions(
      {}, base::BindOnce([](bool result) { EXPECT_TRUE(result); }));
  EXPECT_TRUE(extension_service->IsExtensionEnabled(
      base::UTF16ToUTF8(extension_ids[0])));
  EXPECT_TRUE(extension_service->IsExtensionEnabled(
      base::UTF16ToUTF8(extension_ids[1])));
  EXPECT_TRUE(extension_service->IsExtensionEnabled(
      base::UTF16ToUTF8(extension_ids[2])));
}

TEST_F(ExtensionDeletionEnabledTest, BadlyFormattedDeletionTest) {
  std::vector<base::string16> extension_ids = PopulateExtensionIds();
  extensions::ExtensionService* extension_service = this->service();
  std::unique_ptr<ChromePromptImpl> chrome_prompt =
      std::make_unique<ChromePromptImpl>(extension_service, nullptr,
                                         base::DoNothing(), base::DoNothing());

  chrome_prompt->DisableExtensions(
      {L"bad-extension-id"},
      base::BindOnce([](bool result) { EXPECT_FALSE(result); }));
  chrome_prompt->DisableExtensions(
      {L""}, base::BindOnce([](bool result) { EXPECT_FALSE(result); }));
  chrome_prompt->DisableExtensions(
      {L"🤷☝¯\\_(ツ)_/¯✌🤷"},
      base::BindOnce([](bool result) { EXPECT_FALSE(result); }));
}

TEST_F(ExtensionDeletionEnabledTest, NotInstalledExtensionTest) {
  // Don't actually install the extension
  std::vector<base::string16> extension_ids = PopulateExtensionIds(false);
  extensions::ExtensionService* extension_service = this->service();
  std::unique_ptr<ChromePromptImpl> chrome_prompt =
      std::make_unique<ChromePromptImpl>(extension_service, nullptr,
                                         base::DoNothing(), base::DoNothing());

  chrome_prompt->DisableExtensions(
      extension_ids, base::BindOnce([](bool result) { EXPECT_FALSE(result); }));
}

TEST_F(ExtensionDeletionDisabledTest, CannotDisableExtensionTest) {
  std::vector<base::string16> extension_ids = PopulateExtensionIds();
  extensions::ExtensionService* extension_service = this->service();

  std::unique_ptr<ChromePromptImpl> chrome_prompt =
      std::make_unique<ChromePromptImpl>(extension_service, nullptr,
                                         base::DoNothing(), base::DoNothing());
  chrome_prompt->PromptUser({}, {}, extension_ids, base::DoNothing());
  std::vector<base::string16> extensions_to_disable{extension_ids[0]};
  chrome_prompt->DisableExtensions(
      extensions_to_disable,
      base::BindOnce([](bool result) { EXPECT_FALSE(result); }));

  // Even if we called disable, the extension doesn't get disabled.
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[0])),
            nullptr);
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[1])),
            nullptr);
  EXPECT_NE(extension_service->GetInstalledExtension(
                base::UTF16ToUTF8(extension_ids[2])),
            nullptr);
}

}  // namespace safe_browsing
