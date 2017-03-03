// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_model.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_test_utils.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "extensions/common/extension.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {
namespace {

using extensions::Extension;
using testing::_;
using testing::Bool;
using testing::Combine;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::NiceMock;
using testing::Pair;
using testing::UnorderedElementsAre;

const char kDefaultHomepage[] = "http://myhomepage.com";
const char kDefaultSearchUrl[] = "http://mygoogle.com/s?q={searchTerms}";
const char kDefaultStartupUrl1[] = "http://mystart1.com";
const char kDefaultStartupUrl2[] = "http://mystart2.com";

const char kHomepage1[] = "http://homepage.com/";
const char kHomepage2[] = "http://otherhomepage.com/";
const char kSearchUrl1[] = "http://mysearch.com/s?q={searchTerms}";
const char kSearchUrl2[] = "http://othersearch.com/s?q={searchTerms}";
const char kStartupUrl1[] = "http://super-startup.com";
const char kStartupUrl2[] = "http://awesome-start-page.com";

// Extension manifests to override settings.
const char kManifestNoOverride[] =
    "{"
    "  'name': 'Safe Extension',"
    "  'version': '1',"
    "  'manifest_version': 2"
    "}";

const char kManifestToOverrideHomepage[] =
    "{"
    "  'name': 'Homepage Extension',"
    "  'version': '1',"
    "  'manifest_version': 2,"
    "  'chrome_settings_overrides' : {"
    "    'homepage': '%s'"
    "  }"
    "}";

const char kManifestToOverrideSearch[] =
    "{"
    "  'name': 'Search Extension',"
    "  'version': '0.1',"
    "  'manifest_version': 2,"
    "  'chrome_settings_overrides': {"
    "    'search_provider': {"
    "        'name': 'name',"
    "        'keyword': 'keyword',"
    "        'search_url': '%s',"
    "        'favicon_url': 'http://someplace.com/favicon.ico',"
    "        'encoding': 'UTF-8',"
    "        'is_default': true"
    "    }"
    "  }"
    "}";

const char kManifestToOverrideStartupUrls[] =
    "{"
    "  'name': 'Startup URLs Extension',"
    "  'version': '1',"
    "  'manifest_version': 2,"
    "  'chrome_settings_overrides' : {"
    "    'startup_pages': ['%s']"
    "  }"
    "}";

class SettingsResetPromptModelBrowserTest : public ExtensionBrowserTest {
 public:
  virtual void OnResetDone() { ++reset_callbacks_; }

 protected:
  using ModelPointer = std::unique_ptr<SettingsResetPromptModel>;

  SettingsResetPromptModelBrowserTest()
      : startup_pref_(SessionStartupPref::URLS) {}

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    // Set up an active homepage with visible homepage button.
    PrefService* prefs = profile()->GetPrefs();
    ASSERT_TRUE(prefs);
    prefs->SetBoolean(prefs::kShowHomeButton, true);
    prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
    prefs->SetString(prefs::kHomePage, kDefaultHomepage);

    // Set up a default search with known URL.
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    ASSERT_TRUE(template_url_service);

    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16("default"));
    data.SetKeyword(base::ASCIIToUTF16("default"));
    data.SetURL(kDefaultSearchUrl);

    TemplateURL* template_url =
        template_url_service->Add(base::MakeUnique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

    // Set up a couple of startup URLs.
    startup_pref_.urls.push_back(GURL(kDefaultStartupUrl1));
    startup_pref_.urls.push_back(GURL(kDefaultStartupUrl2));
    SessionStartupPref::SetStartupPref(profile(), startup_pref_);
    ASSERT_EQ(SessionStartupPref::PrefValueToType(
                  GetPrefs()->GetInteger(prefs::kRestoreOnStartup)),
              startup_pref_.type);
  }

  PrefService* GetPrefs() { return profile()->GetPrefs(); }

  void LoadHomepageExtension(const char* homepage,
                             const Extension** out_extension) {
    const std::string manifest =
        base::StringPrintf(kManifestToOverrideHomepage, homepage);
    LoadManifest(manifest, out_extension);
    ASSERT_EQ(std::string(homepage), GetPrefs()->GetString(prefs::kHomePage));
  }

  void LoadSearchExtension(const char* search_url,
                           const Extension** out_extension) {
    const std::string manifest =
        base::StringPrintf(kManifestToOverrideSearch, search_url);
    LoadManifest(manifest, out_extension);

    TemplateURLService* service =
        TemplateURLServiceFactory::GetForProfile(profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(service);

    const TemplateURL* dse = service->GetDefaultSearchProvider();
    EXPECT_EQ(std::string(search_url), dse->url());
  }

  void LoadStartupUrlExtension(const char* startup_url,
                               const Extension** out_extension) {
    const std::string manifest =
        base::StringPrintf(kManifestToOverrideStartupUrls, startup_url);
    LoadManifest(manifest, out_extension);

    // Ensure that the startup url seen in the prefs is same as |startup_url|.
    const base::ListValue* url_list =
        GetPrefs()->GetList(prefs::kURLsToRestoreOnStartup);
    ASSERT_EQ(url_list->GetSize(), 1U);
    std::string url_text;
    ASSERT_TRUE(url_list->GetString(0, &url_text));
    ASSERT_EQ(GURL(url_text), GURL(startup_url));
  }

  void LoadManifest(const std::string& manifest,
                    const Extension** out_extension) {
    extensions::TestExtensionDir extension_dir;
    extension_dir.WriteManifestWithSingleQuotes(manifest);
    *out_extension = LoadExtension(extension_dir.UnpackedPath());
    ASSERT_TRUE(*out_extension);
  }

  // Returns a model with a mock config that will return negative IDs for every
  // URL.
  ModelPointer CreateModel() {
    return CreateModelForTesting(profile(), std::unordered_set<std::string>(),
                                 nullptr);
  }

  // Returns a model with a mock config that will return positive IDs for each
  // URL in |reset_urls|.
  ModelPointer CreateModel(std::unordered_set<std::string> reset_urls) {
    return CreateModelForTesting(profile(), reset_urls, nullptr);
  }

  // Returns a model with a mock config that will return positive IDs for each
  // URL in |reset_urls|.
  ModelPointer CreateModel(std::unordered_set<std::string> reset_urls,
                           std::unique_ptr<ProfileResetter> profile_resetter) {
    return CreateModelForTesting(profile(), reset_urls,
                                 std::move(profile_resetter));
  }

  SessionStartupPref startup_pref_;
  int reset_callbacks_ = 0;
};

IN_PROC_BROWSER_TEST_F(SettingsResetPromptModelBrowserTest,
                       ExtensionsToDisable_Homepage) {
  // Homepage does not require reset to start with.
  {
    ModelPointer model = CreateModel();
    EXPECT_EQ(
        model->homepage_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Let homepage require reset, no extensions need to be disable.
  {
    ModelPointer model = CreateModel({kDefaultHomepage});
    EXPECT_EQ(model->homepage_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Load an extension that does not override settings. Homepage still requires
  // reset, no extensions need to be disable.
  const Extension* safe_extension = nullptr;
  LoadManifest(kManifestNoOverride, &safe_extension);
  {
    ModelPointer model = CreateModel({kDefaultHomepage});
    EXPECT_EQ(model->homepage_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Load extension that overrides homepage. Homepage no longer needs to be
  // reset.
  const Extension* homepage_extension1 = nullptr;
  LoadHomepageExtension(kHomepage1, &homepage_extension1);
  {
    ModelPointer model = CreateModel({kDefaultHomepage});
    EXPECT_EQ(
        model->homepage_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Let the domain used by the extension require reset. Homepage now needs to
  // be reset, one extension needs to be disabled.
  {
    ModelPointer model = CreateModel({kDefaultHomepage, kHomepage1});
    EXPECT_EQ(model->homepage_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
    EXPECT_THAT(model->extensions_to_disable(),
                ElementsAre(Pair(homepage_extension1->id(), _)));
  }

  // Add a second homepage-overriding extension. Homepage no longer needs to be
  // reset, no extensions need to be disabled.
  const Extension* homepage_extension2 = nullptr;
  LoadHomepageExtension(kHomepage2, &homepage_extension2);
  {
    ModelPointer model = CreateModel({kDefaultHomepage, kHomepage1});
    EXPECT_EQ(
        model->homepage_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Let the domain used by the second extension require reset. Homepage needs
  // to be reset again, and both extensions need to be disabled.
  {
    ModelPointer model =
        CreateModel({kDefaultHomepage, kHomepage1, kHomepage2});
    EXPECT_EQ(model->homepage_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
    EXPECT_THAT(model->extensions_to_disable(),
                UnorderedElementsAre(Pair(homepage_extension1->id(), _),
                                     Pair(homepage_extension2->id(), _)));
  }

  // Let only the domain for the second extension require reset. Homepage still
  // needs to be reset, and both extensions need to be disabled.
  {
    ModelPointer model = CreateModel({kHomepage2});
    EXPECT_EQ(model->homepage_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
    EXPECT_THAT(model->extensions_to_disable(),
                UnorderedElementsAre(Pair(homepage_extension1->id(), _),
                                     Pair(homepage_extension2->id(), _)));
  }
}

IN_PROC_BROWSER_TEST_F(SettingsResetPromptModelBrowserTest,
                       ExtensionsToDisable_DefaultSearch) {
  // Search does not need to be reset to start with.
  {
    ModelPointer model = CreateModel();
    EXPECT_EQ(
        model->default_search_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Let the default search domain require reset, no extensions need to be
  // disabled.
  {
    ModelPointer model = CreateModel({kDefaultSearchUrl});
    EXPECT_EQ(model->default_search_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Load an extension that does not override settings. Search still needs to be
  // reset, no extensions need to be disabled.
  const Extension* safe_extension = nullptr;
  LoadManifest(kManifestNoOverride, &safe_extension);
  {
    ModelPointer model = CreateModel({kDefaultSearchUrl});
    EXPECT_EQ(model->default_search_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Load extension that overrides search. Search no longer needs to be reset.
  const Extension* search_extension1 = nullptr;
  LoadSearchExtension(kSearchUrl1, &search_extension1);
  {
    ModelPointer model = CreateModel({kDefaultSearchUrl});
    EXPECT_EQ(
        model->default_search_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Let the domain used by the extension require reset. Search now needs to be
  // reset, one extension needs to be disabled.
  {
    ModelPointer model = CreateModel({kDefaultSearchUrl, kSearchUrl1});
    EXPECT_EQ(model->default_search_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
    EXPECT_THAT(model->extensions_to_disable(),
                ElementsAre(Pair(search_extension1->id(), _)));
  }

  // Add a second search overriding extension. Search no longer needs to be
  // reset, no extensions need to be disabled.
  const Extension* search_extension2 = nullptr;
  LoadSearchExtension(kSearchUrl2, &search_extension2);
  {
    ModelPointer model = CreateModel({kDefaultSearchUrl, kSearchUrl1});
    EXPECT_EQ(
        model->default_search_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Let the domain used by the second extension require reset. Search needs to
  // be reset again, and both extensions need to be disabled.
  {
    ModelPointer model =
        CreateModel({kDefaultSearchUrl, kSearchUrl1, kSearchUrl2});
    EXPECT_EQ(model->default_search_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
    EXPECT_THAT(model->extensions_to_disable(),
                UnorderedElementsAre(Pair(search_extension1->id(), _),
                                     Pair(search_extension2->id(), _)));
  }

  // let only the domain for the second extension require reset. Search still
  // needs to be reset, and both extensions need to be disabled.
  {
    ModelPointer model = CreateModel({kSearchUrl2});
    EXPECT_EQ(model->default_search_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
    EXPECT_THAT(model->extensions_to_disable(),
                UnorderedElementsAre(Pair(search_extension1->id(), _),
                                     Pair(search_extension2->id(), _)));
  }
}

IN_PROC_BROWSER_TEST_F(SettingsResetPromptModelBrowserTest,
                       ExtensionsToDisable_StartupUrls) {
  // Startup urls do not require reset to begin with.
  {
    ModelPointer model = CreateModel();
    EXPECT_EQ(
        model->startup_urls_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
    EXPECT_THAT(model->startup_urls(),
                UnorderedElementsAre(GURL(kDefaultStartupUrl1),
                                     GURL(kDefaultStartupUrl2)));
    EXPECT_THAT(model->startup_urls_to_reset(), IsEmpty());
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Let a default startup URL require reset, no extensions need to be disabled.
  {
    ModelPointer model = CreateModel({kDefaultStartupUrl1});
    EXPECT_EQ(model->startup_urls_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
    EXPECT_THAT(model->startup_urls(),
                UnorderedElementsAre(GURL(kDefaultStartupUrl1),
                                     GURL(kDefaultStartupUrl2)));
    EXPECT_THAT(model->startup_urls_to_reset(),
                ElementsAre(GURL(kDefaultStartupUrl1)));
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Load an extension that does not override settings. Startup URLs still needs
  // to be reset, no extensions need to be disabled.
  const Extension* safe_extension = nullptr;
  LoadManifest(kManifestNoOverride, &safe_extension);
  {
    ModelPointer model = CreateModel({kDefaultStartupUrl1});
    EXPECT_EQ(model->startup_urls_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
    EXPECT_THAT(model->startup_urls(),
                UnorderedElementsAre(GURL(kDefaultStartupUrl1),
                                     GURL(kDefaultStartupUrl2)));
    EXPECT_THAT(model->startup_urls_to_reset(),
                ElementsAre(GURL(kDefaultStartupUrl1)));
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Load two other extensions that each override startup urls.
  const Extension* startup_url_extension1 = nullptr;
  LoadStartupUrlExtension(kStartupUrl1, &startup_url_extension1);
  const Extension* startup_url_extension2 = nullptr;
  LoadStartupUrlExtension(kStartupUrl2, &startup_url_extension2);

  // Startup URLs should not require reset when a default startup URL that is
  // not active (since it is now overridden by an extension) requires reset.
  {
    ModelPointer model = CreateModel({kDefaultStartupUrl1});
    EXPECT_EQ(
        model->startup_urls_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
    EXPECT_THAT(model->startup_urls(), ElementsAre(GURL(kStartupUrl2)));
    EXPECT_THAT(model->startup_urls_to_reset(), IsEmpty());
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Let the first extension's URL require reset. Startup URLs do not need to be
  // reset because the second extension's URL is the active one and does not
  // require reset.
  {
    ModelPointer model = CreateModel({kStartupUrl1});
    EXPECT_EQ(
        model->startup_urls_reset_state(),
        SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
    EXPECT_THAT(model->startup_urls(), ElementsAre(GURL(kStartupUrl2)));
    EXPECT_THAT(model->startup_urls_to_reset(), IsEmpty());
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Let the second extension's URL also require reset. Startup URLs now need to
  // be reset and both extensions need to be disabled.
  {
    ModelPointer model = CreateModel({kStartupUrl1, kStartupUrl2});
    EXPECT_EQ(model->startup_urls_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
    EXPECT_THAT(model->startup_urls(), ElementsAre(GURL(kStartupUrl2)));
    EXPECT_THAT(model->startup_urls_to_reset(),
                ElementsAre(GURL(kStartupUrl2)));
    EXPECT_THAT(model->extensions_to_disable(),
                UnorderedElementsAre(Pair(startup_url_extension1->id(), _),
                                     Pair(startup_url_extension2->id(), _)));
  }

  // Both extensions need to be disabled even when only the second extension's
  // URL requires reset.
  {
    ModelPointer model = CreateModel({kStartupUrl2});
    EXPECT_EQ(model->startup_urls_reset_state(),
              SettingsResetPromptModel::RESET_REQUIRED);
    EXPECT_THAT(model->startup_urls(), ElementsAre(GURL(kStartupUrl2)));
    EXPECT_THAT(model->startup_urls_to_reset(),
                ElementsAre(GURL(kStartupUrl2)));
    EXPECT_THAT(model->extensions_to_disable(),
                UnorderedElementsAre(Pair(startup_url_extension1->id(), _),
                                     Pair(startup_url_extension2->id(), _)));
  }
}

IN_PROC_BROWSER_TEST_F(SettingsResetPromptModelBrowserTest,
                       PerformReset_DefaultSearch) {
  // Load an extension that does not override settings and two extensions that
  // override default search.
  const Extension* safe_extension = nullptr;
  LoadManifest(kManifestNoOverride, &safe_extension);
  const Extension* default_search_extension1 = nullptr;
  LoadSearchExtension(kSearchUrl1, &default_search_extension1);
  const Extension* default_search_extension2 = nullptr;
  LoadSearchExtension(kSearchUrl2, &default_search_extension2);

  ProfileResetter::ResettableFlags expected_reset_flags =
      ProfileResetter::DEFAULT_SEARCH_ENGINE;
  auto mock_resetter =
      base::MakeUnique<NiceMock<MockProfileResetter>>(profile());
  EXPECT_CALL(*mock_resetter.get(), MockReset(expected_reset_flags, _, _))
      .Times(1);
  ModelPointer model = CreateModel({kSearchUrl2}, std::move(mock_resetter));

  EXPECT_TRUE(model->ShouldPromptForReset());
  EXPECT_THAT(model->extensions_to_disable(),
              UnorderedElementsAre(Pair(default_search_extension1->id(), _),
                                   Pair(default_search_extension2->id(), _)));
  EXPECT_EQ(model->default_search_reset_state(),
            SettingsResetPromptModel::RESET_REQUIRED);

  // The |PerformReset()| function uses |ExtensionService| directly to disable
  // extensions so that we can expect the extensions to be disabled after the
  // reset even though we are mocking the |ProfileResetter|.
  model->PerformReset(
      base::Bind(&SettingsResetPromptModelBrowserTest::OnResetDone,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(reset_callbacks_, 1);

  // After the reset, should no longer need any resets and all
  // settings-overriding extensions should be disabled.
  ModelPointer model2 = CreateModel({kSearchUrl2});

  EXPECT_FALSE(model2->ShouldPromptForReset());
  EXPECT_EQ(
      model2->default_search_reset_state(),
      SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
  EXPECT_FALSE(
      extension_service()->IsExtensionEnabled(default_search_extension1->id()));
  EXPECT_FALSE(
      extension_service()->IsExtensionEnabled(default_search_extension2->id()));
  EXPECT_TRUE(extension_service()->IsExtensionEnabled(safe_extension->id()));
}

IN_PROC_BROWSER_TEST_F(SettingsResetPromptModelBrowserTest,
                       PerformReset_StartupUrls) {
  // Load an extension that does not override settings and two extensions that
  // override startup URLs.
  const Extension* safe_extension = nullptr;
  LoadManifest(kManifestNoOverride, &safe_extension);
  const Extension* startup_urls_extension1 = nullptr;
  LoadStartupUrlExtension(kStartupUrl1, &startup_urls_extension1);
  const Extension* startup_urls_extension2 = nullptr;
  LoadStartupUrlExtension(kStartupUrl2, &startup_urls_extension2);

  ProfileResetter::ResettableFlags expected_reset_flags =
      ProfileResetter::STARTUP_PAGES;
  auto mock_resetter =
      base::MakeUnique<NiceMock<MockProfileResetter>>(profile());
  EXPECT_CALL(*mock_resetter.get(), MockReset(expected_reset_flags, _, _))
      .Times(1);
  ModelPointer model = CreateModel({kStartupUrl2}, std::move(mock_resetter));

  EXPECT_TRUE(model->ShouldPromptForReset());
  EXPECT_THAT(model->extensions_to_disable(),
              UnorderedElementsAre(Pair(startup_urls_extension1->id(), _),
                                   Pair(startup_urls_extension2->id(), _)));
  EXPECT_EQ(model->startup_urls_reset_state(),
            SettingsResetPromptModel::RESET_REQUIRED);

  // The |PerformReset()| function uses |ExtensionService| directly to disable
  // extensions so that we can expect the extensions to be disabled after the
  // reset even though we are mocking the |ProfileResetter|.
  model->PerformReset(
      base::Bind(&SettingsResetPromptModelBrowserTest::OnResetDone,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(reset_callbacks_, 1);

  // After the reset, should no longer need any resets and all
  // settings-overriding extensions should be disabled.
  ModelPointer model2 = CreateModel({kStartupUrl2});

  EXPECT_FALSE(model2->ShouldPromptForReset());
  EXPECT_EQ(
      model2->startup_urls_reset_state(),
      SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
  EXPECT_FALSE(
      extension_service()->IsExtensionEnabled(startup_urls_extension1->id()));
  EXPECT_FALSE(
      extension_service()->IsExtensionEnabled(startup_urls_extension2->id()));
  EXPECT_TRUE(extension_service()->IsExtensionEnabled(safe_extension->id()));
}

IN_PROC_BROWSER_TEST_F(SettingsResetPromptModelBrowserTest,
                       PerformReset_Homepage) {
  // Load an extension that does not override settings and two extensions that
  // override startup URLs.
  const Extension* safe_extension = nullptr;
  LoadManifest(kManifestNoOverride, &safe_extension);
  const Extension* homepage_extension1 = nullptr;
  LoadHomepageExtension(kHomepage1, &homepage_extension1);
  const Extension* homepage_extension2 = nullptr;
  LoadHomepageExtension(kHomepage2, &homepage_extension2);

  ProfileResetter::ResettableFlags expected_reset_flags =
      ProfileResetter::HOMEPAGE;
  auto mock_resetter =
      base::MakeUnique<NiceMock<MockProfileResetter>>(profile());
  EXPECT_CALL(*mock_resetter.get(), MockReset(expected_reset_flags, _, _))
      .Times(1);
  ModelPointer model = CreateModel({kHomepage2}, std::move(mock_resetter));

  EXPECT_TRUE(model->ShouldPromptForReset());
  EXPECT_THAT(model->extensions_to_disable(),
              UnorderedElementsAre(Pair(homepage_extension1->id(), _),
                                   Pair(homepage_extension2->id(), _)));
  EXPECT_EQ(model->homepage_reset_state(),
            SettingsResetPromptModel::RESET_REQUIRED);

  // The |PerformReset()| function uses |ExtensionService| directly to disable
  // extensions so that we can expect the extensions to be disabled after the
  // reset even though we are mocking the |ProfileResetter|.
  model->PerformReset(
      base::Bind(&SettingsResetPromptModelBrowserTest::OnResetDone,
                 base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(reset_callbacks_, 1);

  // After the reset, should no longer need any resets and all
  // settings-overriding extensions should be disabled.
  ModelPointer model2 = CreateModel({kHomepage2});

  EXPECT_FALSE(model2->ShouldPromptForReset());
  EXPECT_EQ(
      model2->homepage_reset_state(),
      SettingsResetPromptModel::NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED);
  EXPECT_FALSE(
      extension_service()->IsExtensionEnabled(homepage_extension1->id()));
  EXPECT_FALSE(
      extension_service()->IsExtensionEnabled(homepage_extension2->id()));
  EXPECT_TRUE(extension_service()->IsExtensionEnabled(safe_extension->id()));
}

const char kManifestToOverrideAll[] =
    "{"
    "  'name': 'Override All Extension',"
    "  'version': '0.1',"
    "  'manifest_version': 2,"
    "  'chrome_settings_overrides': {"
    "    'homepage': '%s',"
    "    'search_provider': {"
    "        'name': 'name',"
    "        'keyword': 'keyword',"
    "        'search_url': '%s',"
    "        'favicon_url': 'http://someplace.com/favicon.ico',"
    "        'encoding': 'UTF-8',"
    "        'is_default': true"
    "    },"
    "    'startup_pages': ['%s']"
    "  }"
    "}";

class ExtensionSettingsOverrideTest
    : public SettingsResetPromptModelBrowserTest,
      public testing::WithParamInterface<testing::tuple<bool, bool, bool>> {
 protected:
  void SetUpOnMainThread() override {
    SettingsResetPromptModelBrowserTest::SetUpOnMainThread();
    homepage_matches_config_ = testing::get<0>(GetParam());
    default_search_matches_config_ = testing::get<1>(GetParam());
    startup_urls_matches_config_ = testing::get<2>(GetParam());

    // Always set up these extensions, but only require reset for their URLs
    // based on the parameters of the test:
    //
    // - 1 extension that overrides all three settings.
    // - 2 Homepage-overriding extensions;
    // - 2 Search-overriding extensions; and
    // - 2 Startup URLs-overriding extensions.
    //
    // For each setting, the value set by the last installed extension that
    // overrides that setting will be the value set in preferences.
    std::string override_all_manifest =
        base::StringPrintf(kManifestToOverrideAll, "http://all-homepage.com",
                           "http://all-search.com", "http://all-startup.com");
    LoadManifest(override_all_manifest, &overrides_all_);
    LoadHomepageExtension(kHomepage1, &homepage1_);
    LoadHomepageExtension(kHomepage2, &homepage2_);
    LoadSearchExtension(kSearchUrl1, &default_search1_);
    LoadSearchExtension(kSearchUrl2, &default_search2_);
    LoadStartupUrlExtension(kStartupUrl1, &startup_urls1_);
    LoadStartupUrlExtension(kStartupUrl2, &startup_urls2_);
  }

  bool homepage_matches_config_;
  bool default_search_matches_config_;
  bool startup_urls_matches_config_;
  const Extension* overrides_all_;
  const Extension* homepage1_;
  const Extension* homepage2_;
  const Extension* default_search1_;
  const Extension* default_search2_;
  const Extension* startup_urls1_;
  const Extension* startup_urls2_;
};

IN_PROC_BROWSER_TEST_P(ExtensionSettingsOverrideTest, ExtensionsToDisable) {
  // Prepare the reset URLs based on the test parameters.
  std::unordered_set<std::string> reset_urls;
  if (homepage_matches_config_)
    reset_urls.insert(kHomepage2);
  if (default_search_matches_config_)
    reset_urls.insert(kSearchUrl2);
  if (startup_urls_matches_config_)
    reset_urls.insert(kStartupUrl2);

  {
    ModelPointer model = CreateModel(reset_urls);
    EXPECT_EQ(model->homepage(), GURL(kHomepage2));
    EXPECT_EQ(model->default_search(), GURL(kSearchUrl2));
    EXPECT_THAT(model->startup_urls(), ElementsAre(GURL(kStartupUrl2)));
    if (startup_urls_matches_config_) {
      EXPECT_THAT(model->startup_urls_to_reset(),
                  ElementsAre(GURL(kStartupUrl2)));
    } else {
      EXPECT_THAT(model->startup_urls_to_reset(), IsEmpty());
    }

    // We can assume that the model correctly determines which setting requires
    // reset, since that is tested in the model's unit tests.
    std::unordered_set<extensions::ExtensionId> expected_extension_ids;
    if (model->homepage_reset_state() ==
        SettingsResetPromptModel::RESET_REQUIRED) {
      expected_extension_ids.insert(homepage1_->id());
      expected_extension_ids.insert(homepage2_->id());
      expected_extension_ids.insert(overrides_all_->id());
    }
    if (model->default_search_reset_state() ==
        SettingsResetPromptModel::RESET_REQUIRED) {
      expected_extension_ids.insert(default_search1_->id());
      expected_extension_ids.insert(default_search2_->id());
      expected_extension_ids.insert(overrides_all_->id());
    }
    if (model->startup_urls_reset_state() ==
        SettingsResetPromptModel::RESET_REQUIRED) {
      expected_extension_ids.insert(startup_urls1_->id());
      expected_extension_ids.insert(startup_urls2_->id());
      expected_extension_ids.insert(overrides_all_->id());
    }

    std::unordered_set<extensions::ExtensionId> actual_extension_ids;
    for (const auto& pair : model->extensions_to_disable())
      actual_extension_ids.insert(pair.first);

    EXPECT_EQ(actual_extension_ids, expected_extension_ids);
  }
}

INSTANTIATE_TEST_CASE_P(SettingsResetPromptModel,
                        ExtensionSettingsOverrideTest,
                        Combine(Bool(), Bool(), Bool()));

}  // namespace
}  // namespace safe_browsing
