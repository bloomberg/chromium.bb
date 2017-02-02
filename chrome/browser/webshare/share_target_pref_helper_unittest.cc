// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/webshare/share_target_pref_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrefRegistrySimple;

namespace {

class ShareTargetPrefHelperUnittest : public testing::Test {
 protected:
  ShareTargetPrefHelperUnittest() {}
  ~ShareTargetPrefHelperUnittest() override {}

  void SetUp() override {
    pref_service_.reset(new TestingPrefServiceSimple());
    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kWebShareVisitedTargets);
  }

  PrefService* pref_service() { return pref_service_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

constexpr char kUrlTemplateKey[] = "url_template";

TEST_F(ShareTargetPrefHelperUnittest, AddMultipleShareTargets) {
  // Add a share target to prefs that wasn't previously stored.
  std::string manifest_url = "https://www.sharetarget.com/manifest.json";
  base::Optional<std::string> url_template =
      base::Optional<std::string>("share/?title={title}");

  UpdateShareTargetInPrefs(manifest_url, url_template, pref_service());

  const base::DictionaryValue* share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(1UL, share_target_dict->size());
  const base::DictionaryValue* share_target_info_dict = nullptr;
  ASSERT_TRUE(share_target_dict->GetDictionaryWithoutPathExpansion(
      manifest_url, &share_target_info_dict));
  EXPECT_EQ(1UL, share_target_info_dict->size());
  std::string url_template_in_dict;
  EXPECT_TRUE(share_target_info_dict->GetString(kUrlTemplateKey,
                                                &url_template_in_dict));
  EXPECT_EQ(url_template_in_dict, url_template);

  // Add second share target to prefs that wasn't previously stored.
  manifest_url = "https://www.sharetarget2.com/manifest.json";
  url_template = base::Optional<std::string>("share/?title={title}");

  UpdateShareTargetInPrefs(manifest_url, url_template, pref_service());

  share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(2UL, share_target_dict->size());
  ASSERT_TRUE(share_target_dict->GetDictionaryWithoutPathExpansion(
      manifest_url, &share_target_info_dict));
  EXPECT_EQ(1UL, share_target_info_dict->size());
  EXPECT_TRUE(share_target_info_dict->GetString(kUrlTemplateKey,
                                                &url_template_in_dict));
  EXPECT_EQ(url_template_in_dict, url_template);
}

TEST_F(ShareTargetPrefHelperUnittest, AddShareTargetTwice) {
  const char kManifestUrl[] = "https://www.sharetarget.com/manifest.json";
  const base::Optional<std::string> kUrlTemplate =
      base::Optional<std::string>("share/?title={title}");

  // Add a share target to prefs that wasn't previously stored.
  UpdateShareTargetInPrefs(kManifestUrl, kUrlTemplate, pref_service());

  const base::DictionaryValue* share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(1UL, share_target_dict->size());
  const base::DictionaryValue* share_target_info_dict = nullptr;
  ASSERT_TRUE(share_target_dict->GetDictionaryWithoutPathExpansion(
      kManifestUrl, &share_target_info_dict));
  EXPECT_EQ(1UL, share_target_info_dict->size());
  std::string url_template_in_dict;
  EXPECT_TRUE(share_target_info_dict->GetString(kUrlTemplateKey,
                                                &url_template_in_dict));
  EXPECT_EQ(url_template_in_dict, kUrlTemplate);

  // Add same share target to prefs that was previously stored; shouldn't
  // duplicate it.
  UpdateShareTargetInPrefs(kManifestUrl, kUrlTemplate, pref_service());

  share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(1UL, share_target_dict->size());
  ASSERT_TRUE(share_target_dict->GetDictionaryWithoutPathExpansion(
      kManifestUrl, &share_target_info_dict));
  EXPECT_EQ(1UL, share_target_info_dict->size());
  EXPECT_TRUE(share_target_info_dict->GetString(kUrlTemplateKey,
                                                &url_template_in_dict));
  EXPECT_EQ(url_template_in_dict, kUrlTemplate);
}

TEST_F(ShareTargetPrefHelperUnittest, UpdateShareTarget) {
  // Add a share target to prefs that wasn't previously stored.
  std::string manifest_url = "https://www.sharetarget.com/manifest.json";
  base::Optional<std::string> url_template =
      base::Optional<std::string>("share/?title={title}");

  UpdateShareTargetInPrefs(manifest_url, url_template, pref_service());

  const base::DictionaryValue* share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(1UL, share_target_dict->size());
  const base::DictionaryValue* share_target_info_dict = nullptr;
  ASSERT_TRUE(share_target_dict->GetDictionaryWithoutPathExpansion(
      manifest_url, &share_target_info_dict));
  EXPECT_EQ(1UL, share_target_info_dict->size());
  std::string url_template_in_dict;
  EXPECT_TRUE(share_target_info_dict->GetString(kUrlTemplateKey,
                                                &url_template_in_dict));
  EXPECT_EQ(url_template_in_dict, url_template);

  // Add same share target to prefs that was previously stored, with new
  // url_template_in_dict; should update the value.
  manifest_url = "https://www.sharetarget.com/manifest.json";
  url_template =
      base::Optional<std::string>("share/?title={title}&text={text}");

  UpdateShareTargetInPrefs(manifest_url, url_template, pref_service());

  share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(1UL, share_target_dict->size());
  ASSERT_TRUE(share_target_dict->GetDictionaryWithoutPathExpansion(
      manifest_url, &share_target_info_dict));
  EXPECT_EQ(1UL, share_target_info_dict->size());
  EXPECT_TRUE(share_target_info_dict->GetString(kUrlTemplateKey,
                                                &url_template_in_dict));
  EXPECT_EQ(url_template_in_dict, url_template);
}

TEST_F(ShareTargetPrefHelperUnittest, DontAddNonShareTarget) {
  const char kManifestUrl[] = "https://www.dudsharetarget.com/manifest.json";
  const base::Optional<std::string> kUrlTemplate;

  // Don't add a site that has a null template.
  UpdateShareTargetInPrefs(kManifestUrl, kUrlTemplate, pref_service());

  const base::DictionaryValue* share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(0UL, share_target_dict->size());
  const base::DictionaryValue* share_target_info_dict = nullptr;
  ASSERT_FALSE(share_target_dict->GetDictionaryWithoutPathExpansion(
      kManifestUrl, &share_target_info_dict));
}

TEST_F(ShareTargetPrefHelperUnittest, RemoveShareTarget) {
  // Add a share target to prefs that wasn't previously stored.
  std::string manifest_url = "https://www.sharetarget.com/manifest.json";
  base::Optional<std::string> url_template =
      base::Optional<std::string>("share/?title={title}");

  UpdateShareTargetInPrefs(manifest_url, url_template, pref_service());

  const base::DictionaryValue* share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(1UL, share_target_dict->size());
  const base::DictionaryValue* share_target_info_dict = nullptr;
  ASSERT_TRUE(share_target_dict->GetDictionaryWithoutPathExpansion(
      manifest_url, &share_target_info_dict));
  EXPECT_EQ(1UL, share_target_info_dict->size());
  std::string url_template_in_dict;
  EXPECT_TRUE(share_target_info_dict->GetString(kUrlTemplateKey,
                                                &url_template_in_dict));
  EXPECT_EQ(url_template_in_dict, url_template);

  // Share target already added now has null template. Remove from prefs.
  manifest_url = "https://www.sharetarget.com/manifest.json";
  url_template = base::nullopt;

  UpdateShareTargetInPrefs(manifest_url, url_template, pref_service());

  share_target_dict =
      pref_service()->GetDictionary(prefs::kWebShareVisitedTargets);
  EXPECT_EQ(0UL, share_target_dict->size());
}

}  // namespace
