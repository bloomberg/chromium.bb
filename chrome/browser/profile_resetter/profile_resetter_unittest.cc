// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_resetter.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profile_resetter/profile_resetter_test_base.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"


namespace {

using extensions::Extension;
using extensions::Manifest;

class ProfileResetterTest : public testing::Test,
                            public ProfileResetterTestBase {
 protected:
  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  TemplateURLServiceTestUtil test_util_;
};

void ProfileResetterTest::SetUp() {
  test_util_.SetUp();
  resetter_.reset(new ProfileResetter(test_util_.profile()));
}

void ProfileResetterTest::TearDown() {
  test_util_.TearDown();
}

// ExtensionsResetTest sets up the extension service.
class ExtensionsResetTest : public ExtensionServiceTestBase,
                            public ProfileResetterTestBase {
 protected:
  virtual void SetUp() OVERRIDE;
};

void ExtensionsResetTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();
  resetter_.reset(new ProfileResetter(profile_.get()));
}

scoped_refptr<Extension> CreateExtension(const std::string& name,
                                         const base::FilePath& path,
                                         Manifest::Location location,
                                         bool theme) {
  DictionaryValue manifest;
  manifest.SetString(extension_manifest_keys::kVersion, "1.0.0.0");
  manifest.SetString(extension_manifest_keys::kName, name);
  manifest.SetString("app.launch.web_url", "http://www.google.com");
  if (theme)
    manifest.Set(extension_manifest_keys::kTheme, new DictionaryValue());
  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      path,
      location,
      manifest,
      Extension::NO_FLAGS,
      &error);
  EXPECT_TRUE(extension.get() != NULL) << error;
  return extension;
}

class PinnedTabsResetTest : public BrowserWithTestWindowTest,
                            public ProfileResetterTestBase {
 protected:
  virtual void SetUp() OVERRIDE;

  content::WebContents* CreateWebContents();
};

void PinnedTabsResetTest::SetUp() {
  BrowserWithTestWindowTest::SetUp();
  resetter_.reset(new ProfileResetter(profile()));
}

content::WebContents* PinnedTabsResetTest::CreateWebContents() {
  return content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
}

/********************* Tests *********************/

TEST_F(ProfileResetterTest, ResetDefaultSearchEngine) {
  PrefService* prefs = test_util_.profile()->GetPrefs();
  DCHECK(prefs);
  prefs->SetString(prefs::kLastPromptedGoogleURL, "http://www.foo.com/");

  resetter_->Reset(
      ProfileResetter::DEFAULT_SEARCH_ENGINE,
      base::Bind(&ProfileResetterMockObject::StopLoop,
                 base::Unretained(&mock_object_)));
  mock_object_.RunLoop();

  EXPECT_EQ("", prefs->GetString(prefs::kLastPromptedGoogleURL));
}

TEST_F(ProfileResetterTest, ResetHomepage) {
  PrefService* prefs = test_util_.profile()->GetPrefs();
  DCHECK(prefs);
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
  prefs->SetString(prefs::kHomePage, "http://google.com");
  prefs->SetBoolean(prefs::kShowHomeButton, true);

  resetter_->Reset(
      ProfileResetter::HOMEPAGE,
      base::Bind(&ProfileResetterMockObject::StopLoop,
                 base::Unretained(&mock_object_)));
  mock_object_.RunLoop();

  EXPECT_TRUE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
  EXPECT_EQ(std::string(), prefs->GetString(prefs::kHomePage));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kShowHomeButton));
}

TEST_F(ProfileResetterTest, ResetContentSettings) {
  HostContentSettingsMap* host_content_settings_map =
      test_util_.profile()->GetHostContentSettingsMap();
  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(test_util_.profile());
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.org");
  std::map<ContentSettingsType, ContentSetting> default_settings;

  for (int type = 0; type < CONTENT_SETTINGS_NUM_TYPES; ++type) {
    if (type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
      notification_service->SetDefaultContentSetting(CONTENT_SETTING_BLOCK);
      notification_service->GrantPermission(GURL("http://foo.de"));
    } else if (type == CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE ||
               type == CONTENT_SETTINGS_TYPE_MIXEDSCRIPT ||
               type == CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS) {
      // These types are excluded because one can't call
      // GetDefaultContentSetting() for them.
    } else {
      ContentSettingsType content_type = static_cast<ContentSettingsType>(type);
      ContentSetting default_setting =
          host_content_settings_map->GetDefaultContentSetting(content_type,
                                                              NULL);
      default_settings[content_type] = default_setting;
      ContentSetting wildcard_setting =
          default_setting == CONTENT_SETTING_BLOCK ? CONTENT_SETTING_ALLOW
                                                   : CONTENT_SETTING_BLOCK;
      ContentSetting site_setting =
          default_setting == CONTENT_SETTING_ALLOW ? CONTENT_SETTING_ALLOW
                                                   : CONTENT_SETTING_BLOCK;
      if (HostContentSettingsMap::IsSettingAllowedForType(
              test_util_.profile()->GetPrefs(),
              wildcard_setting,
              content_type)) {
        host_content_settings_map->SetDefaultContentSetting(
            content_type,
            wildcard_setting);
      }
      if (!HostContentSettingsMap::ContentTypeHasCompoundValue(content_type) &&
          HostContentSettingsMap::IsValueAllowedForType(
              test_util_.profile()->GetPrefs(),
              Value::CreateIntegerValue(site_setting),
              content_type)) {
        host_content_settings_map->SetContentSetting(
            pattern,
            ContentSettingsPattern::Wildcard(),
            content_type,
            std::string(),
            site_setting);
        ContentSettingsForOneType host_settings;
        host_content_settings_map->GetSettingsForOneType(
            content_type, std::string(), &host_settings);
        EXPECT_EQ(2U, host_settings.size());
      }
    }
  }

  resetter_->Reset(
      ProfileResetter::CONTENT_SETTINGS,
      base::Bind(&ProfileResetterMockObject::StopLoop,
                 base::Unretained(&mock_object_)));
  mock_object_.RunLoop();

  for (int type = 0; type < CONTENT_SETTINGS_NUM_TYPES; ++type) {
    if (type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
      EXPECT_EQ(CONTENT_SETTING_ASK,
                notification_service->GetDefaultContentSetting(NULL));
      EXPECT_EQ(CONTENT_SETTING_ASK,
                notification_service->GetContentSetting(GURL("http://foo.de")));
    } else {
      ContentSettingsType content_type = static_cast<ContentSettingsType>(type);
      if (HostContentSettingsMap::ContentTypeHasCompoundValue(content_type) ||
          type == CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE ||
          content_type == CONTENT_SETTINGS_TYPE_MIXEDSCRIPT ||
          content_type == CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS)
        continue;
      ContentSetting default_setting =
          host_content_settings_map->GetDefaultContentSetting(content_type,
                                                              NULL);
      EXPECT_TRUE(default_settings.count(content_type));
      EXPECT_EQ(default_settings[content_type], default_setting);
      if (!HostContentSettingsMap::ContentTypeHasCompoundValue(content_type)) {
        ContentSetting site_setting =
            host_content_settings_map->GetContentSetting(
                GURL("example.org"),
                GURL(),
                content_type,
                std::string());
        EXPECT_EQ(default_setting, site_setting);
      }

      ContentSettingsForOneType host_settings;
      host_content_settings_map->GetSettingsForOneType(
          content_type, std::string(), &host_settings);
      EXPECT_EQ(1U, host_settings.size());
    }
  }
}

TEST_F(ExtensionsResetTest, ResetExtensionsByDisabling) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<Extension> theme = CreateExtension("example1", temp_dir.path(),
                                                   Manifest::INVALID_LOCATION,
                                                   true);
  service_->FinishInstallationForTest(theme.get());
  // Let ThemeService finish creating the theme pack.
  base::MessageLoop::current()->RunUntilIdle();

  // ThemeService isn't compiled for Android.
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(theme_service->UsingDefaultTheme());

  scoped_refptr<Extension> ext2 = CreateExtension(
      "example2",
      base::FilePath(FILE_PATH_LITERAL("//nonexistent")),
      Manifest::INVALID_LOCATION,
      false);
  service_->AddExtension(ext2.get());
  // Components and external policy extensions shouldn't be deleted.
  scoped_refptr<Extension> ext3 = CreateExtension(
      "example3",
      base::FilePath(FILE_PATH_LITERAL("//nonexistent2")),
      Manifest::COMPONENT,
      false);
  service_->AddExtension(ext3.get());
  scoped_refptr<Extension> ext4 =
      CreateExtension("example4",
                      base::FilePath(FILE_PATH_LITERAL("//nonexistent3")),
                      Manifest::EXTERNAL_POLICY_DOWNLOAD,
                      false);
  service_->AddExtension(ext4.get());
  EXPECT_EQ(4u, service_->extensions()->size());

  resetter_->Reset(
      ProfileResetter::EXTENSIONS,
      base::Bind(&ProfileResetterMockObject::StopLoop,
                 base::Unretained(&mock_object_)));
  mock_object_.RunLoop();

  EXPECT_EQ(2u, service_->extensions()->size());
  EXPECT_TRUE(service_->extensions()->Contains(ext3->id()));
  EXPECT_TRUE(service_->extensions()->Contains(ext4->id()));
  EXPECT_TRUE(theme_service->UsingDefaultTheme());
}

TEST_F(ProfileResetterTest, ResetStartPage) {
  PrefService* prefs = test_util_.profile()->GetPrefs();
  DCHECK(prefs);

  SessionStartupPref startup_pref(SessionStartupPref::URLS);
  startup_pref.urls.push_back(GURL("http://foo"));
  startup_pref.urls.push_back(GURL("http://bar"));
  SessionStartupPref::SetStartupPref(prefs, startup_pref);

  resetter_->Reset(
      ProfileResetter::STARTUP_PAGES,
      base::Bind(&ProfileResetterMockObject::StopLoop,
                 base::Unretained(&mock_object_)));
  mock_object_.RunLoop();

  startup_pref = SessionStartupPref::GetStartupPref(prefs);
  EXPECT_EQ(SessionStartupPref::GetDefaultStartupType(), startup_pref.type);
  EXPECT_EQ(std::vector<GURL>(), startup_pref.urls);
}

TEST_F(PinnedTabsResetTest, ResetPinnedTabs) {
  scoped_refptr<Extension> extension_app = CreateExtension(
      "hello!",
      base::FilePath(FILE_PATH_LITERAL("//nonexistent")),
      Manifest::INVALID_LOCATION,
      false);
  scoped_ptr<content::WebContents> contents1(CreateWebContents());
  extensions::TabHelper::CreateForWebContents(contents1.get());
  extensions::TabHelper::FromWebContents(contents1.get())->
      SetExtensionApp(extension_app.get());
  scoped_ptr<content::WebContents> contents2(CreateWebContents());
  scoped_ptr<content::WebContents> contents3(CreateWebContents());
  scoped_ptr<content::WebContents> contents4(CreateWebContents());
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  tab_strip_model->AppendWebContents(contents4.get(), true);
  tab_strip_model->AppendWebContents(contents3.get(), true);
  tab_strip_model->AppendWebContents(contents2.get(), true);
  tab_strip_model->SetTabPinned(2, true);
  tab_strip_model->AppendWebContents(contents1.get(), true);
  tab_strip_model->SetTabPinned(3, true);

  EXPECT_EQ(contents2, tab_strip_model->GetWebContentsAt(0));
  EXPECT_EQ(contents1, tab_strip_model->GetWebContentsAt(1));
  EXPECT_EQ(contents3, tab_strip_model->GetWebContentsAt(2));
  EXPECT_EQ(contents4, tab_strip_model->GetWebContentsAt(3));
  EXPECT_EQ(3, tab_strip_model->IndexOfFirstNonMiniTab());

  resetter_->Reset(
      ProfileResetter::PINNED_TABS,
      base::Bind(&ProfileResetterMockObject::StopLoop,
                 base::Unretained(&mock_object_)));
  mock_object_.RunLoop();

  EXPECT_EQ(contents1, tab_strip_model->GetWebContentsAt(0));
  EXPECT_EQ(contents2, tab_strip_model->GetWebContentsAt(1));
  EXPECT_EQ(contents3, tab_strip_model->GetWebContentsAt(2));
  EXPECT_EQ(contents4, tab_strip_model->GetWebContentsAt(3));
  EXPECT_EQ(1, tab_strip_model->IndexOfFirstNonMiniTab());
}

TEST_F(ProfileResetterTest, ResetFewFlags) {
  // mock_object_ is a StrictMock, so we verify that it is called only once.
  resetter_->Reset(
      ProfileResetter::DEFAULT_SEARCH_ENGINE | ProfileResetter::HOMEPAGE,
      base::Bind(&ProfileResetterMockObject::StopLoop,
                 base::Unretained(&mock_object_)));
  mock_object_.RunLoop();
}

}  // namespace
