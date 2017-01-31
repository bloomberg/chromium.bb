// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_model.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
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
using testing::ElementsAre;
using testing::IsEmpty;
using testing::NiceMock;
using testing::Pair;
using testing::Return;
using testing::SizeIs;
using testing::UnorderedElementsAre;

const char kDefaultHomepage[] = "http://myhomepage.com";
const char kDefaultSearchUrl[] = "http://mygoogle.com/s?q={searchTerms}";

#if defined(OS_WIN) || defined(OS_MACOSX)
const char kHomepage1[] = "http://homepage.com/";
const char kHomepage2[] = "http://otherhomepage.com/";
const char kSearchUrl1[] = "http://mysearch.com/s?q={searchTerms}";
const char kSearchUrl2[] = "http://othersearch.com/s?q={searchTerms}";
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

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
    "   }"
    "}";

class SettingsResetPromptModelBrowserTest : public ExtensionBrowserTest {
 protected:
  using ModelPointer = std::unique_ptr<SettingsResetPromptModel>;

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
    return CreateModelForTesting(profile(), std::unordered_set<std::string>());
  }

  // Returns a model with a mock config that will return positive IDs for each
  // URL in |reset_urls|.
  ModelPointer CreateModel(std::unordered_set<std::string> reset_urls) {
    return CreateModelForTesting(profile(), reset_urls);
  }
};

IN_PROC_BROWSER_TEST_F(SettingsResetPromptModelBrowserTest,
                       ExtensionsToDisable_Homepage) {
  // Homepage does not require reset to start with.
  {
    ModelPointer model = CreateModel();
    EXPECT_EQ(model->homepage_reset_state(),
              SettingsResetPromptModel::DISABLED_DUE_TO_DOMAIN_NOT_MATCHED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Let homepage require reset, no extensions need to be disable.
  {
    ModelPointer model = CreateModel({kDefaultHomepage});
    EXPECT_EQ(model->homepage_reset_state(), SettingsResetPromptModel::ENABLED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Load an extension that does not override settings. Homepage still requires
  // reset, no extensions need to be disable.
  const Extension* safe_extension = nullptr;
  LoadManifest(kManifestNoOverride, &safe_extension);
  {
    ModelPointer model = CreateModel({kDefaultHomepage});
    EXPECT_EQ(model->homepage_reset_state(), SettingsResetPromptModel::ENABLED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

// The next part should only be run on platforms where settings override for
// extension is available. The settings reset prompt is currently designed for
// desktop, and Windows in particular. Getting the tests to run on other
// platforms that do not allow extensions to override settings (e.g., Linux)
// would require more #ifdefs and platform specific considerations than it is
// worth.
#if defined(OS_WIN) || defined(OS_MACOSX)
  // Load extension that overrides homepage. Homepage no longer needs to be
  // reset.
  const Extension* homepage_extension1 = nullptr;
  LoadHomepageExtension(kHomepage1, &homepage_extension1);
  {
    ModelPointer model = CreateModel({kDefaultHomepage});
    EXPECT_EQ(model->homepage_reset_state(),
              SettingsResetPromptModel::DISABLED_DUE_TO_DOMAIN_NOT_MATCHED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Let the domain used by the extension require reset. Homepage now needs to
  // be reset, one extension needs to be disabled.
  {
    ModelPointer model = CreateModel({kDefaultHomepage, kHomepage1});
    EXPECT_EQ(model->homepage_reset_state(), SettingsResetPromptModel::ENABLED);
    EXPECT_THAT(model->extensions_to_disable(),
                ElementsAre(Pair(homepage_extension1->id(), _)));
  }

  // Add a second homepage-overriding extension. Homepage no longer needs to be
  // reset, no extensions need to be disabled.
  const Extension* homepage_extension2 = nullptr;
  LoadHomepageExtension(kHomepage2, &homepage_extension2);
  {
    ModelPointer model = CreateModel({kDefaultHomepage, kHomepage1});
    EXPECT_EQ(model->homepage_reset_state(),
              SettingsResetPromptModel::DISABLED_DUE_TO_DOMAIN_NOT_MATCHED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Let the domain used by the second extension require reset. Homepage needs
  // to be reset again, and both extensions need to be disabled.
  {
    ModelPointer model =
        CreateModel({kDefaultHomepage, kHomepage1, kHomepage2});
    EXPECT_EQ(model->homepage_reset_state(), SettingsResetPromptModel::ENABLED);
    EXPECT_THAT(model->extensions_to_disable(),
                UnorderedElementsAre(Pair(homepage_extension1->id(), _),
                                     Pair(homepage_extension2->id(), _)));
  }

  // Let only the domain for the second extension require reset. Homepage still
  // needs to be reset, and both extensions need to be disabled.
  {
    ModelPointer model = CreateModel({kHomepage2});
    EXPECT_EQ(model->homepage_reset_state(), SettingsResetPromptModel::ENABLED);
    EXPECT_THAT(model->extensions_to_disable(),
                UnorderedElementsAre(Pair(homepage_extension1->id(), _),
                                     Pair(homepage_extension2->id(), _)));
  }
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
}

IN_PROC_BROWSER_TEST_F(SettingsResetPromptModelBrowserTest,
                       ExtensionsToDisable_DefaultSearch) {
  // Search does not need to be reset to start with.
  {
    ModelPointer model = CreateModel();
    EXPECT_EQ(model->default_search_reset_state(),
              SettingsResetPromptModel::DISABLED_DUE_TO_DOMAIN_NOT_MATCHED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Let the default search domain require reset, no extensions need to be
  // disabled.
  {
    ModelPointer model = CreateModel({kDefaultSearchUrl});
    EXPECT_EQ(model->default_search_reset_state(),
              SettingsResetPromptModel::ENABLED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Load an extension that does not override settings. Search still needs to be
  // reset, no extensions need to ge disabled.
  const Extension* safe_extension = nullptr;
  LoadManifest(kManifestNoOverride, &safe_extension);
  {
    ModelPointer model = CreateModel({kDefaultSearchUrl});
    EXPECT_EQ(model->default_search_reset_state(),
              SettingsResetPromptModel::ENABLED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

// The next part should only be run on platforms where settings override for
// extension is available. The settings reset prompt is currently designed for
// desktop, and Windows in particular. Getting the tests to run on other
// platforms that do not allow extensions to override settings (e.g., Linux)
// would require more #ifdefs and platform specific considerations than it is
// worth.
#if defined(OS_WIN) || defined(OS_MACOSX)
  // Load extension that overrides search. Search no longer needs to be reset.
  const Extension* search_extension1 = nullptr;
  LoadSearchExtension(kSearchUrl1, &search_extension1);
  {
    ModelPointer model = CreateModel({kDefaultSearchUrl});
    EXPECT_EQ(model->default_search_reset_state(),
              SettingsResetPromptModel::DISABLED_DUE_TO_DOMAIN_NOT_MATCHED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Let the domain used by the extension require reset. Search now needs to be
  // reset, one extension needs to be disabled.
  {
    ModelPointer model = CreateModel({kDefaultSearchUrl, kSearchUrl1});
    EXPECT_EQ(model->default_search_reset_state(),
              SettingsResetPromptModel::ENABLED);
    EXPECT_THAT(model->extensions_to_disable(),
                ElementsAre(Pair(search_extension1->id(), _)));
  }

  // Add a second search overriding extension. Search no longer needs to be
  // reset, no extensions need to be disabled.
  const Extension* search_extension2 = nullptr;
  LoadSearchExtension(kSearchUrl2, &search_extension2);
  {
    ModelPointer model = CreateModel({kDefaultSearchUrl, kSearchUrl1});
    EXPECT_EQ(model->default_search_reset_state(),
              SettingsResetPromptModel::DISABLED_DUE_TO_DOMAIN_NOT_MATCHED);
    EXPECT_THAT(model->extensions_to_disable(), IsEmpty());
  }

  // Let the domain used by the second extension require reset. Search needs to
  // be reset again, and both extensions need to be disabled.
  {
    ModelPointer model =
        CreateModel({kDefaultSearchUrl, kSearchUrl1, kSearchUrl2});
    EXPECT_EQ(model->default_search_reset_state(),
              SettingsResetPromptModel::ENABLED);
    EXPECT_THAT(model->extensions_to_disable(),
                UnorderedElementsAre(Pair(search_extension1->id(), _),
                                     Pair(search_extension2->id(), _)));
  }

  // let only the domain for the second extension require reset. Search still
  // needs to be reset, and both extensions need to be disabled.
  {
    ModelPointer model = CreateModel({kSearchUrl2});
    EXPECT_EQ(model->default_search_reset_state(),
              SettingsResetPromptModel::ENABLED);
    EXPECT_THAT(model->extensions_to_disable(),
                UnorderedElementsAre(Pair(search_extension1->id(), _),
                                     Pair(search_extension2->id(), _)));
  }
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
}

}  // namespace
}  // namespace safe_browsing
