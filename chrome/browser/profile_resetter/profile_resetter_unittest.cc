// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_resetter.h"

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profile_resetter/brandcode_config_fetcher.h"
#include "chrome/browser/profile_resetter/profile_resetter_test_base.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/test_url_fetcher_factory.h"


namespace {

const char kDistributionConfig[] = "{"
    " \"homepage\" : \"http://www.foo.com\","
    " \"homepage_is_newtabpage\" : false,"
    " \"browser\" : {"
    "   \"show_home_button\" : true"
    "  },"
    " \"session\" : {"
    "   \"restore_on_startup\" : 4,"
    "   \"urls_to_restore_on_startup\" : [\"http://goo.gl\", \"http://foo.de\"]"
    "  },"
    " \"search_provider_overrides\" : ["
    "    {"
    "      \"name\" : \"first\","
    "      \"keyword\" : \"firstkey\","
    "      \"search_url\" : \"http://www.foo.com/s?q={searchTerms}\","
    "      \"favicon_url\" : \"http://www.foo.com/favicon.ico\","
    "      \"suggest_url\" : \"http://www.foo.com/s?q={searchTerms}\","
    "      \"encoding\" : \"UTF-8\","
    "      \"id\" : 1001"
    "    }"
    "  ],"
    " \"extensions\" : {"
    "   \"settings\" : {"
    "     \"placeholder_for_id\": {"
    "      }"
    "    }"
    "  }"
    "}";

const char kXmlConfig[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<response protocol=\"3.0\" server=\"prod\">"
      "<app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\" status=\"ok\">"
        "<data index=\"skipfirstrunui-importsearch-defaultbrowser\" "
          "name=\"install\" status=\"ok\">"
          "placeholder_for_data"
        "</data>"
      "</app>"
    "</response>";

using extensions::Extension;
using extensions::Manifest;

// ProfileResetterTest sets up the extension, WebData and TemplateURL services.
class ProfileResetterTest : public ExtensionServiceTestBase,
                            public ProfileResetterTestBase {
 protected:
  virtual void SetUp() OVERRIDE;

  TestingProfile* profile() { return profile_.get(); }

  static BrowserContextKeyedService* CreateTemplateURLService(
      content::BrowserContext* context);
};

void ProfileResetterTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  profile()->CreateWebDataService();
  TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      &ProfileResetterTest::CreateTemplateURLService);
  resetter_.reset(new ProfileResetter(profile()));
}

// static
BrowserContextKeyedService* ProfileResetterTest::CreateTemplateURLService(
    content::BrowserContext* context) {
  return new TemplateURLService(static_cast<Profile*>(context));
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
    manifest.Set(extension_manifest_keys::kTheme, new DictionaryValue);
  manifest.SetString(extension_manifest_keys::kOmniboxKeyword, name);
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

class ConfigParserTest : public testing::Test {
 protected:
  ConfigParserTest();
  virtual ~ConfigParserTest();

  MOCK_METHOD0(Callback, void(void));

  scoped_ptr<BrandcodeConfigFetcher> WaitForRequest(const GURL& url);

  static scoped_ptr<net::FakeURLFetcher> CreateFakeURLFetcher(
      const GURL& url,
      net::URLFetcherDelegate* d,
      const std::string& response_data,
      bool success);

  base::MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
};

ConfigParserTest::ConfigParserTest()
    : loop_(base::MessageLoop::TYPE_IO),
      ui_thread_(content::BrowserThread::UI, &loop_),
      io_thread_(content::BrowserThread::IO, &loop_) {
}

ConfigParserTest::~ConfigParserTest() {}

scoped_ptr<BrandcodeConfigFetcher> ConfigParserTest::WaitForRequest(
    const GURL& url) {
  EXPECT_CALL(*this, Callback());
  scoped_ptr<BrandcodeConfigFetcher> fetcher(
      new BrandcodeConfigFetcher(base::Bind(&ConfigParserTest::Callback,
                                            base::Unretained(this)),
                                 url));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(fetcher->IsActive());
  return fetcher.Pass();
}

scoped_ptr<net::FakeURLFetcher> ConfigParserTest::CreateFakeURLFetcher(
    const GURL& url,
    net::URLFetcherDelegate* d,
    const std::string& response_data,
    bool success) {
  scoped_ptr<net::FakeURLFetcher> fetcher(
      new net::FakeURLFetcher(url, d, response_data, success));
  scoped_refptr<net::HttpResponseHeaders> download_headers =
      new net::HttpResponseHeaders("");
  download_headers->AddHeader("Content-Type: text/xml");
  fetcher->set_response_headers(download_headers);
  return fetcher.Pass();
}

void ReplaceString(std::string* str,
                   const std::string& placeholder,
                   const std::string& substitution) {
  ASSERT_NE(static_cast<std::string*>(NULL), str);
  size_t placeholder_pos = str->find(placeholder);
  ASSERT_NE(std::string::npos, placeholder_pos);
  str->replace(placeholder_pos, placeholder.size(), substitution);
}


/********************* Tests *********************/

TEST_F(ProfileResetterTest, ResetDefaultSearchEngine) {
  // Search engine's logic is tested by
  // TemplateURLServiceTest.ResetURLs.
  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  prefs->SetString(prefs::kLastPromptedGoogleURL, "http://www.foo.com/");

  scoped_refptr<Extension> extension = CreateExtension(
      "xxx",
      base::FilePath(FILE_PATH_LITERAL("//nonexistent")),
      Manifest::COMPONENT,
      false);
  service_->AddExtension(extension.get());

  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE);

  // TemplateURLService should reset extension search engines.
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(profile());
  TemplateURL* ext_url = model->GetTemplateURLForKeyword(ASCIIToUTF16("xxx"));
  ASSERT_TRUE(ext_url);
  EXPECT_TRUE(ext_url->IsExtensionKeyword());
  EXPECT_EQ(ASCIIToUTF16("xxx"), ext_url->keyword());
  EXPECT_EQ(ASCIIToUTF16("xxx"), ext_url->short_name());
  EXPECT_EQ("", prefs->GetString(prefs::kLastPromptedGoogleURL));
}

TEST_F(ProfileResetterTest, ResetDefaultSearchEngineNonOrganic) {
  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  prefs->SetString(prefs::kLastPromptedGoogleURL, "http://www.foo.com/");

  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE, kDistributionConfig);

  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(profile());
  EXPECT_EQ(1u, model->GetTemplateURLs().size());
  TemplateURL* default_engine = model->GetDefaultSearchProvider();
  ASSERT_NE(static_cast<TemplateURL*>(NULL), default_engine);
  EXPECT_EQ(ASCIIToUTF16("first"), default_engine->short_name());
  EXPECT_EQ(ASCIIToUTF16("firstkey"), default_engine->keyword());
  EXPECT_EQ("http://www.foo.com/s?q={searchTerms}", default_engine->url());

  EXPECT_EQ("", prefs->GetString(prefs::kLastPromptedGoogleURL));
}

TEST_F(ProfileResetterTest, ResetHomepage) {
  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
  prefs->SetString(prefs::kHomePage, "http://google.com");
  prefs->SetBoolean(prefs::kShowHomeButton, true);

  ResetAndWait(ProfileResetter::HOMEPAGE);

  EXPECT_TRUE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
  EXPECT_EQ(std::string(), prefs->GetString(prefs::kHomePage));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kShowHomeButton));
}

TEST_F(ProfileResetterTest, ResetHomepageNonOrganic) {
  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, true);
  prefs->SetString(prefs::kHomePage, "http://google.com");
  prefs->SetBoolean(prefs::kShowHomeButton, false);

  ResetAndWait(ProfileResetter::HOMEPAGE, kDistributionConfig);

  EXPECT_FALSE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
  EXPECT_EQ("http://www.foo.com", prefs->GetString(prefs::kHomePage));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kShowHomeButton));
}

TEST_F(ProfileResetterTest, ResetContentSettings) {
  HostContentSettingsMap* host_content_settings_map =
      profile()->GetHostContentSettingsMap();
  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(profile());
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
              profile()->GetPrefs(),
              wildcard_setting,
              content_type)) {
        host_content_settings_map->SetDefaultContentSetting(
            content_type,
            wildcard_setting);
      }
      if (!HostContentSettingsMap::ContentTypeHasCompoundValue(content_type) &&
          HostContentSettingsMap::IsSettingAllowedForType(
              profile()->GetPrefs(),
              site_setting,
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

  ResetAndWait(ProfileResetter::CONTENT_SETTINGS);

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

TEST_F(ProfileResetterTest, ResetExtensionsByDisabling) {
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
      ThemeServiceFactory::GetForProfile(profile());
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

  ResetAndWait(ProfileResetter::EXTENSIONS);

  EXPECT_EQ(2u, service_->extensions()->size());
  EXPECT_TRUE(service_->extensions()->Contains(ext3->id()));
  EXPECT_TRUE(service_->extensions()->Contains(ext4->id()));
  EXPECT_TRUE(theme_service->UsingDefaultTheme());
}

TEST_F(ProfileResetterTest, ResetExtensionsByDisablingNonOrganic) {
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
      Manifest::INVALID_LOCATION,
      false);
  service_->AddExtension(ext3.get());
  EXPECT_EQ(2u, service_->extensions()->size());

  std::string master_prefs(kDistributionConfig);
  ReplaceString(&master_prefs, "placeholder_for_id", ext3->id());

  ResetAndWait(ProfileResetter::EXTENSIONS, master_prefs);

  EXPECT_EQ(1u, service_->extensions()->size());
  EXPECT_TRUE(service_->extensions()->Contains(ext3->id()));
}

TEST_F(ProfileResetterTest, ResetStartPage) {
  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);

  SessionStartupPref startup_pref(SessionStartupPref::URLS);
  startup_pref.urls.push_back(GURL("http://foo"));
  startup_pref.urls.push_back(GURL("http://bar"));
  SessionStartupPref::SetStartupPref(prefs, startup_pref);

  ResetAndWait(ProfileResetter::STARTUP_PAGES);

  startup_pref = SessionStartupPref::GetStartupPref(prefs);
  EXPECT_EQ(SessionStartupPref::GetDefaultStartupType(), startup_pref.type);
  EXPECT_EQ(std::vector<GURL>(), startup_pref.urls);
}

TEST_F(ProfileResetterTest, ResetStartPageNonOrganic) {
  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);

  SessionStartupPref startup_pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(prefs, startup_pref);

  ResetAndWait(ProfileResetter::STARTUP_PAGES, kDistributionConfig);

  startup_pref = SessionStartupPref::GetStartupPref(prefs);
  EXPECT_EQ(SessionStartupPref::URLS, startup_pref.type);
  const GURL urls[] = {GURL("http://goo.gl"), GURL("http://foo.de")};
  EXPECT_EQ(std::vector<GURL>(urls, urls + arraysize(urls)), startup_pref.urls);
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

  ResetAndWait(ProfileResetter::PINNED_TABS);

  EXPECT_EQ(contents1, tab_strip_model->GetWebContentsAt(0));
  EXPECT_EQ(contents2, tab_strip_model->GetWebContentsAt(1));
  EXPECT_EQ(contents3, tab_strip_model->GetWebContentsAt(2));
  EXPECT_EQ(contents4, tab_strip_model->GetWebContentsAt(3));
  EXPECT_EQ(1, tab_strip_model->IndexOfFirstNonMiniTab());
}

TEST_F(ProfileResetterTest, ResetFewFlags) {
  // mock_object_ is a StrictMock, so we verify that it is called only once.
  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE |
               ProfileResetter::HOMEPAGE |
               ProfileResetter::CONTENT_SETTINGS);
}

// Tries to load unavailable config file.
TEST_F(ConfigParserTest, NoConnectivity) {
  net::FakeURLFetcherFactory factory(NULL);
  const std::string url("http://test");
  factory.SetFakeResponse(url, "", false);

  scoped_ptr<BrandcodeConfigFetcher> fetcher = WaitForRequest(GURL(url));
  EXPECT_FALSE(fetcher->GetSettings());
}

// Tries to load available config file.
TEST_F(ConfigParserTest, ParseConfig) {
  net::FakeURLFetcherFactory factory(
      NULL,
      base::Bind(&ConfigParserTest::CreateFakeURLFetcher));
  const std::string url("http://test");
  std::string xml_config(kXmlConfig);
  ReplaceString(&xml_config, "placeholder_for_data", kDistributionConfig);
  ReplaceString(&xml_config,
                "placeholder_for_id",
                "abbaabbaabbaabbaabbaabbaabbaabba");
  factory.SetFakeResponse(url, xml_config, true);

  scoped_ptr<BrandcodeConfigFetcher> fetcher = WaitForRequest(GURL(url));
  scoped_ptr<BrandcodedDefaultSettings> settings = fetcher->GetSettings();
  ASSERT_TRUE(settings);

  std::vector<std::string> extension_ids;
  EXPECT_TRUE(settings->GetExtensions(&extension_ids));
  EXPECT_EQ(1u, extension_ids.size());
  EXPECT_EQ("abbaabbaabbaabbaabbaabbaabbaabba", extension_ids[0]);

  std::string homepage;
  EXPECT_TRUE(settings->GetHomepage(&homepage));
  EXPECT_EQ("http://www.foo.com", homepage);

  scoped_ptr<base::ListValue> startup_list(
      settings->GetUrlsToRestoreOnStartup());
  EXPECT_TRUE(startup_list);
  std::vector<std::string> startup_pages;
  for (base::ListValue::iterator i = startup_list->begin();
       i != startup_list->end(); ++i) {
    std::string url;
    EXPECT_TRUE((*i)->GetAsString(&url));
    startup_pages.push_back(url);
  }
  EXPECT_EQ("http://goo.gl", startup_pages[0]);
  EXPECT_EQ("http://foo.de", startup_pages[1]);
}

}  // namespace
