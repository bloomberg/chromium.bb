// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"

#include <algorithm>

#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

using InstallSource = InstallSource;

class ExternallyInstalledWebAppPrefsTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ExternallyInstalledWebAppPrefsTest() = default;
  ~ExternallyInstalledWebAppPrefsTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // TODO(https://crbug.com/891172): Use an extension agnostic test registry.
    extensions::TestExtensionSystem* test_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile()));
    test_system->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                        profile()->GetPath(),
                                        false);  // autoupdate_enabled
  }

  std::string GenerateFakeExtensionId(GURL url) {
    return crx_file::id_util::GenerateId("fake_app_id_for:" + url.spec());
  }

  void SimulatePreviouslyInstalledApp(GURL url, InstallSource install_source) {
    std::string id = GenerateFakeExtensionId(url);
    extensions::ExtensionRegistry::Get(profile())->AddEnabled(
        extensions::ExtensionBuilder("Dummy Name").SetID(id).Build());

    ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
        .Insert(url, id, install_source);
  }

  void SimulateUninstallApp(GURL url) {
    std::string id = GenerateFakeExtensionId(url);
    extensions::ExtensionRegistry::Get(profile())->RemoveEnabled(id);
  }

  std::vector<GURL> GetInstalledAppUrls(InstallSource install_source) {
    std::vector<GURL> vec = ExternallyInstalledWebAppPrefs::GetInstalledAppUrls(
        profile(), install_source);
    std::sort(vec.begin(), vec.end());
    return vec;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternallyInstalledWebAppPrefsTest);
};

TEST_F(ExternallyInstalledWebAppPrefsTest, BasicOps) {
  GURL url_a("https://a.example.com/");
  GURL url_b("https://b.example.com/");
  GURL url_c("https://c.example.com/");
  GURL url_d("https://d.example.com/");

  std::string id_a = GenerateFakeExtensionId(url_a);
  std::string id_b = GenerateFakeExtensionId(url_b);
  std::string id_c = GenerateFakeExtensionId(url_c);
  std::string id_d = GenerateFakeExtensionId(url_d);

  auto* prefs = profile()->GetPrefs();
  ExternallyInstalledWebAppPrefs map(prefs);

  // Start with an empty map.

  EXPECT_EQ("missing", map.LookupAppId(url_a).value_or("missing"));
  EXPECT_EQ("missing", map.LookupAppId(url_b).value_or("missing"));
  EXPECT_EQ("missing", map.LookupAppId(url_c).value_or("missing"));
  EXPECT_EQ("missing", map.LookupAppId(url_d).value_or("missing"));

  EXPECT_FALSE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_a));
  EXPECT_FALSE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_b));
  EXPECT_FALSE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_c));
  EXPECT_FALSE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_d));

  EXPECT_EQ(std::vector<GURL>({}),
            GetInstalledAppUrls(InstallSource::kInternal));
  EXPECT_EQ(std::vector<GURL>({}),
            GetInstalledAppUrls(InstallSource::kExternalDefault));
  EXPECT_EQ(std::vector<GURL>({}),
            GetInstalledAppUrls(InstallSource::kExternalPolicy));

  // Add some entries.

  SimulatePreviouslyInstalledApp(url_a, InstallSource::kExternalDefault);
  SimulatePreviouslyInstalledApp(url_b, InstallSource::kInternal);
  SimulatePreviouslyInstalledApp(url_c, InstallSource::kExternalDefault);

  EXPECT_EQ(id_a, map.LookupAppId(url_a).value_or("missing"));
  EXPECT_EQ(id_b, map.LookupAppId(url_b).value_or("missing"));
  EXPECT_EQ(id_c, map.LookupAppId(url_c).value_or("missing"));
  EXPECT_EQ("missing", map.LookupAppId(url_d).value_or("missing"));

  EXPECT_TRUE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_a));
  EXPECT_TRUE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_b));
  EXPECT_TRUE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_c));
  EXPECT_FALSE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_d));

  EXPECT_EQ(std::vector<GURL>({url_b}),
            GetInstalledAppUrls(InstallSource::kInternal));
  EXPECT_EQ(std::vector<GURL>({url_a, url_c}),
            GetInstalledAppUrls(InstallSource::kExternalDefault));
  EXPECT_EQ(std::vector<GURL>({}),
            GetInstalledAppUrls(InstallSource::kExternalPolicy));

  // Overwrite an entry.

  SimulatePreviouslyInstalledApp(url_c, InstallSource::kInternal);

  EXPECT_EQ(id_a, map.LookupAppId(url_a).value_or("missing"));
  EXPECT_EQ(id_b, map.LookupAppId(url_b).value_or("missing"));
  EXPECT_EQ(id_c, map.LookupAppId(url_c).value_or("missing"));
  EXPECT_EQ("missing", map.LookupAppId(url_d).value_or("missing"));

  EXPECT_TRUE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_a));
  EXPECT_TRUE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_b));
  EXPECT_TRUE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_c));
  EXPECT_FALSE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_d));

  EXPECT_EQ(std::vector<GURL>({url_b, url_c}),
            GetInstalledAppUrls(InstallSource::kInternal));
  EXPECT_EQ(std::vector<GURL>({url_a}),
            GetInstalledAppUrls(InstallSource::kExternalDefault));
  EXPECT_EQ(std::vector<GURL>({}),
            GetInstalledAppUrls(InstallSource::kExternalPolicy));

  // Uninstall an underlying extension. The ExternallyInstalledWebAppPrefs will
  // still return positive for LookupAppId and HasAppId (as they ignore
  // installed-ness), but GetInstalledAppUrls will skip over it.

  SimulateUninstallApp(url_b);

  EXPECT_EQ(id_a, map.LookupAppId(url_a).value_or("missing"));
  EXPECT_EQ(id_b, map.LookupAppId(url_b).value_or("missing"));
  EXPECT_EQ(id_c, map.LookupAppId(url_c).value_or("missing"));
  EXPECT_EQ("missing", map.LookupAppId(url_d).value_or("missing"));

  EXPECT_TRUE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_a));
  EXPECT_TRUE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_b));
  EXPECT_TRUE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_c));
  EXPECT_FALSE(ExternallyInstalledWebAppPrefs::HasAppId(prefs, id_d));

  EXPECT_EQ(std::vector<GURL>({url_c}),
            GetInstalledAppUrls(InstallSource::kInternal));
  EXPECT_EQ(std::vector<GURL>({url_a}),
            GetInstalledAppUrls(InstallSource::kExternalDefault));
  EXPECT_EQ(std::vector<GURL>({}),
            GetInstalledAppUrls(InstallSource::kExternalPolicy));
}

}  // namespace web_app
