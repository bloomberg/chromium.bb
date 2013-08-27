// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_resetter.h"

#include "base/json/json_string_value_serializer.h"
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
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/common/manifest_constants.h"
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


// ProfileResetterTest --------------------------------------------------------

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


// PinnedTabsResetTest --------------------------------------------------------

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


// ConfigParserTest -----------------------------------------------------------

// URLFetcher delegate that simply records the upload data.
struct URLFetcherRequestListener : net::URLFetcherDelegate {
  URLFetcherRequestListener();
  virtual ~URLFetcherRequestListener();

  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  std::string upload_data;
  net::URLFetcherDelegate* real_delegate;
};

URLFetcherRequestListener::URLFetcherRequestListener()
    : real_delegate(NULL) {
}

URLFetcherRequestListener::~URLFetcherRequestListener() {
}

void URLFetcherRequestListener::OnURLFetchComplete(
    const net::URLFetcher* source) {
  const net::TestURLFetcher* test_fetcher =
      static_cast<const net::TestURLFetcher*>(source);
  upload_data = test_fetcher->upload_data();
  DCHECK(real_delegate);
  real_delegate->OnURLFetchComplete(source);
}

class ConfigParserTest : public testing::Test {
 protected:
  ConfigParserTest();
  virtual ~ConfigParserTest();

  scoped_ptr<BrandcodeConfigFetcher> WaitForRequest(const GURL& url);

  net::FakeURLFetcherFactory& factory() { return factory_; }

 private:
  scoped_ptr<net::FakeURLFetcher> CreateFakeURLFetcher(
      const GURL& url,
      net::URLFetcherDelegate* fetcher_delegate,
      const std::string& response_data,
      bool success);

  MOCK_METHOD0(Callback, void(void));

  base::MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  URLFetcherRequestListener request_listener_;
  net::FakeURLFetcherFactory factory_;
};

ConfigParserTest::ConfigParserTest()
    : loop_(base::MessageLoop::TYPE_IO),
      ui_thread_(content::BrowserThread::UI, &loop_),
      io_thread_(content::BrowserThread::IO, &loop_),
      factory_(NULL, base::Bind(&ConfigParserTest::CreateFakeURLFetcher,
                                base::Unretained(this))) {
}

ConfigParserTest::~ConfigParserTest() {}

scoped_ptr<BrandcodeConfigFetcher> ConfigParserTest::WaitForRequest(
    const GURL& url) {
  EXPECT_CALL(*this, Callback());
  scoped_ptr<BrandcodeConfigFetcher> fetcher(
      new BrandcodeConfigFetcher(base::Bind(&ConfigParserTest::Callback,
                                            base::Unretained(this)),
                                 url,
                                 "ABCD"));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(fetcher->IsActive());
  // Look for the brand code in the request.
  EXPECT_NE(std::string::npos, request_listener_.upload_data.find("ABCD"));
  return fetcher.Pass();
}

scoped_ptr<net::FakeURLFetcher> ConfigParserTest::CreateFakeURLFetcher(
    const GURL& url,
    net::URLFetcherDelegate* fetcher_delegate,
    const std::string& response_data,
    bool success) {
  request_listener_.real_delegate = fetcher_delegate;
  scoped_ptr<net::FakeURLFetcher> fetcher(
      new net::FakeURLFetcher(url, &request_listener_, response_data, success));
  scoped_refptr<net::HttpResponseHeaders> download_headers =
      new net::HttpResponseHeaders("");
  download_headers->AddHeader("Content-Type: text/xml");
  fetcher->set_response_headers(download_headers);
  return fetcher.Pass();
}


// helper functions -----------------------------------------------------------

scoped_refptr<Extension> CreateExtension(const std::string& name,
                                         const base::FilePath& path,
                                         Manifest::Location location,
                                         extensions::Manifest::Type type,
                                         bool installed_by_default) {
  DictionaryValue manifest;
  manifest.SetString(extensions::manifest_keys::kVersion, "1.0.0.0");
  manifest.SetString(extensions::manifest_keys::kName, name);
  switch (type) {
    case extensions::Manifest::TYPE_THEME:
      manifest.Set(extensions::manifest_keys::kTheme, new DictionaryValue);
      break;
    case extensions::Manifest::TYPE_HOSTED_APP:
      manifest.SetString(extensions::manifest_keys::kLaunchWebURL,
                         "http://www.google.com");
      manifest.SetString(extensions::manifest_keys::kUpdateURL,
                         "http://clients2.google.com/service/update2/crx");
      break;
    case extensions::Manifest::TYPE_EXTENSION:
      // do nothing
      break;
    default:
      NOTREACHED();
  }
  manifest.SetString(extensions::manifest_keys::kOmniboxKeyword, name);
  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      path,
      location,
      manifest,
      installed_by_default ? Extension::WAS_INSTALLED_BY_DEFAULT
                           : Extension::NO_FLAGS,
      &error);
  EXPECT_TRUE(extension.get() != NULL) << error;
  return extension;
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
      extensions::Manifest::TYPE_EXTENSION,
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
  service_->Init();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<Extension> theme =
      CreateExtension("example1",
                      temp_dir.path(),
                      Manifest::INVALID_LOCATION,
                      extensions::Manifest::TYPE_THEME,
                      false);
  service_->FinishInstallationForTest(theme.get());
  // Let ThemeService finish creating the theme pack.
  base::MessageLoop::current()->RunUntilIdle();

  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile());
  EXPECT_FALSE(theme_service->UsingDefaultTheme());

  scoped_refptr<Extension> ext2 = CreateExtension(
      "example2",
      base::FilePath(FILE_PATH_LITERAL("//nonexistent")),
      Manifest::INVALID_LOCATION,
      extensions::Manifest::TYPE_EXTENSION,
      false);
  service_->AddExtension(ext2.get());
  // Components and external policy extensions shouldn't be deleted.
  scoped_refptr<Extension> ext3 = CreateExtension(
      "example3",
      base::FilePath(FILE_PATH_LITERAL("//nonexistent2")),
      Manifest::COMPONENT,
      extensions::Manifest::TYPE_EXTENSION,
      false);
  service_->AddExtension(ext3.get());
  scoped_refptr<Extension> ext4 =
      CreateExtension("example4",
                      base::FilePath(FILE_PATH_LITERAL("//nonexistent3")),
                      Manifest::EXTERNAL_POLICY_DOWNLOAD,
                      extensions::Manifest::TYPE_EXTENSION,
                      false);
  service_->AddExtension(ext4.get());
  EXPECT_EQ(4u, service_->extensions()->size());

  ResetAndWait(ProfileResetter::EXTENSIONS);
  EXPECT_EQ(2u, service_->extensions()->size());
  EXPECT_FALSE(service_->extensions()->Contains(theme->id()));
  EXPECT_FALSE(service_->extensions()->Contains(ext2->id()));
  EXPECT_TRUE(service_->extensions()->Contains(ext3->id()));
  EXPECT_TRUE(service_->extensions()->Contains(ext4->id()));
  EXPECT_TRUE(theme_service->UsingDefaultTheme());
}

TEST_F(ProfileResetterTest, ResetExtensionsByDisablingNonOrganic) {
  scoped_refptr<Extension> ext2 = CreateExtension(
      "example2",
      base::FilePath(FILE_PATH_LITERAL("//nonexistent")),
      Manifest::INVALID_LOCATION,
      extensions::Manifest::TYPE_EXTENSION,
      false);
  service_->AddExtension(ext2.get());
  // Components and external policy extensions shouldn't be deleted.
  scoped_refptr<Extension> ext3 = CreateExtension(
      "example3",
      base::FilePath(FILE_PATH_LITERAL("//nonexistent2")),
      Manifest::INVALID_LOCATION,
      extensions::Manifest::TYPE_EXTENSION,
      false);
  service_->AddExtension(ext3.get());
  EXPECT_EQ(2u, service_->extensions()->size());

  std::string master_prefs(kDistributionConfig);
  ReplaceString(&master_prefs, "placeholder_for_id", ext3->id());

  ResetAndWait(ProfileResetter::EXTENSIONS, master_prefs);

  EXPECT_EQ(1u, service_->extensions()->size());
  EXPECT_TRUE(service_->extensions()->Contains(ext3->id()));
}

TEST_F(ProfileResetterTest, ResetExtensionsAndDefaultApps) {
  service_->Init();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<Extension> ext1 =
      CreateExtension("example1",
                      temp_dir.path(),
                      Manifest::INVALID_LOCATION,
                      extensions::Manifest::TYPE_THEME,
                      false);
  service_->FinishInstallationForTest(ext1.get());
  // Let ThemeService finish creating the theme pack.
  base::MessageLoop::current()->RunUntilIdle();

  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile());
  EXPECT_FALSE(theme_service->UsingDefaultTheme());

  scoped_refptr<Extension> ext2 =
      CreateExtension("example2",
                      base::FilePath(FILE_PATH_LITERAL("//nonexistent2")),
                      Manifest::INVALID_LOCATION,
                      extensions::Manifest::TYPE_EXTENSION,
                      false);
  service_->AddExtension(ext2.get());

  scoped_refptr<Extension> ext3 =
      CreateExtension("example2",
                      base::FilePath(FILE_PATH_LITERAL("//nonexistent3")),
                      Manifest::INVALID_LOCATION,
                      extensions::Manifest::TYPE_HOSTED_APP,
                      true);
  service_->AddExtension(ext3.get());
  EXPECT_EQ(3u, service_->extensions()->size());

  ResetAndWait(ProfileResetter::EXTENSIONS);

  EXPECT_EQ(1u, service_->extensions()->size());
  EXPECT_FALSE(service_->extensions()->Contains(ext1->id()));
  EXPECT_FALSE(service_->extensions()->Contains(ext2->id()));
  EXPECT_TRUE(service_->extensions()->Contains(ext3->id()));
  EXPECT_TRUE(theme_service->UsingDefaultTheme());
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
      extensions::Manifest::TYPE_HOSTED_APP,
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
  const std::string url("http://test");
  factory().SetFakeResponse(url, "", false);

  scoped_ptr<BrandcodeConfigFetcher> fetcher = WaitForRequest(GURL(url));
  EXPECT_FALSE(fetcher->GetSettings());
}

// Tries to load available config file.
TEST_F(ConfigParserTest, ParseConfig) {
  const std::string url("http://test");
  std::string xml_config(kXmlConfig);
  ReplaceString(&xml_config, "placeholder_for_data", kDistributionConfig);
  ReplaceString(&xml_config,
                "placeholder_for_id",
                "abbaabbaabbaabbaabbaabbaabbaabba");
  factory().SetFakeResponse(url, xml_config, true);

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
  ASSERT_EQ(2u, startup_pages.size());
  EXPECT_EQ("http://goo.gl", startup_pages[0]);
  EXPECT_EQ("http://foo.de", startup_pages[1]);
}

TEST_F(ProfileResetterTest, CheckSnapshots) {
  ResettableSettingsSnapshot empty_snap(profile());
  EXPECT_EQ(0, empty_snap.FindDifferentFields(empty_snap));

  scoped_refptr<Extension> ext = CreateExtension(
      "example",
      base::FilePath(FILE_PATH_LITERAL("//nonexistent")),
      Manifest::INVALID_LOCATION,
      extensions::Manifest::TYPE_EXTENSION,
      false);
  ASSERT_TRUE(ext);
  service_->AddExtension(ext.get());

  std::string master_prefs(kDistributionConfig);
  std::string ext_id = ext->id();
  ReplaceString(&master_prefs, "placeholder_for_id", ext_id);

  // Reset to non organic defaults.
  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE |
               ProfileResetter::HOMEPAGE |
               ProfileResetter::STARTUP_PAGES, master_prefs);

  ResettableSettingsSnapshot nonorganic_snap(profile());
  EXPECT_EQ(ResettableSettingsSnapshot::ALL_FIELDS,
            empty_snap.FindDifferentFields(nonorganic_snap));
  empty_snap.Subtract(nonorganic_snap);
  EXPECT_TRUE(empty_snap.startup_urls().empty());
  EXPECT_EQ(SessionStartupPref::GetDefaultStartupType(),
            empty_snap.startup_type());
  EXPECT_TRUE(empty_snap.homepage().empty());
  EXPECT_TRUE(empty_snap.homepage_is_ntp());
  EXPECT_NE(std::string::npos, empty_snap.dse_url().find("{google:baseURL}"));
  EXPECT_EQ(std::vector<std::string>(), empty_snap.enabled_extensions());

  // Reset to organic defaults.
  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE |
               ProfileResetter::HOMEPAGE |
               ProfileResetter::STARTUP_PAGES |
               ProfileResetter::EXTENSIONS);

  ResettableSettingsSnapshot organic_snap(profile());
  EXPECT_EQ(ResettableSettingsSnapshot::ALL_FIELDS,
            nonorganic_snap.FindDifferentFields(organic_snap));
  nonorganic_snap.Subtract(organic_snap);
  const GURL urls[] = {GURL("http://foo.de"), GURL("http://goo.gl")};
  EXPECT_EQ(std::vector<GURL>(urls, urls + arraysize(urls)),
            nonorganic_snap.startup_urls());
  EXPECT_EQ(SessionStartupPref::URLS, nonorganic_snap.startup_type());
  EXPECT_EQ("http://www.foo.com", nonorganic_snap.homepage());
  EXPECT_FALSE(nonorganic_snap.homepage_is_ntp());
  EXPECT_EQ("http://www.foo.com/s?q={searchTerms}", nonorganic_snap.dse_url());
  EXPECT_EQ(std::vector<std::string>(1, ext_id),
            nonorganic_snap.enabled_extensions());
}

TEST_F(ProfileResetterTest, FeedbackSerializtionTest) {
  // Reset to non organic defaults.
  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE |
               ProfileResetter::HOMEPAGE |
               ProfileResetter::STARTUP_PAGES, kDistributionConfig);

  scoped_refptr<Extension> ext = CreateExtension(
      "example",
      base::FilePath(FILE_PATH_LITERAL("//nonexistent")),
      Manifest::INVALID_LOCATION,
      extensions::Manifest::TYPE_EXTENSION,
      false);
  ASSERT_TRUE(ext);
  service_->AddExtension(ext.get());

  const ResettableSettingsSnapshot nonorganic_snap(profile());

  COMPILE_ASSERT(ResettableSettingsSnapshot::ALL_FIELDS == 63,
                 expand_this_test);
  for (int field_mask = 0; field_mask <= ResettableSettingsSnapshot::ALL_FIELDS;
       ++field_mask) {
    std::string report = SerializeSettingsReport(nonorganic_snap, field_mask);
    JSONStringValueSerializer json(report);
    std::string error;
    scoped_ptr<base::Value> root(json.Deserialize(NULL, &error));
    ASSERT_TRUE(root) << error;
    ASSERT_TRUE(root->IsType(base::Value::TYPE_DICTIONARY)) << error;

    base::DictionaryValue* dict =
        static_cast<base::DictionaryValue*>(root.get());

    ListValue* startup_urls = NULL;
    int startup_type = 0;
    std::string homepage;
    bool homepage_is_ntp = true;
    std::string default_search_engine;
    ListValue* extensions;

    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::STARTUP_URLS),
              dict->GetList("startup_urls", &startup_urls));
    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::STARTUP_TYPE),
              dict->GetInteger("startup_type", &startup_type));
    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::HOMEPAGE),
              dict->GetString("homepage", &homepage));
    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::HOMEPAGE_IS_NTP),
              dict->GetBoolean("homepage_is_ntp", &homepage_is_ntp));
    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::DSE_URL),
              dict->GetString("default_search_engine", &default_search_engine));
    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::EXTENSIONS),
              dict->GetList("enabled_extensions", &extensions));
  }
}

}  // namespace
