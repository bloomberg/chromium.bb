// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_resetter.h"

#include "base/json/json_string_value_serializer.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profile_resetter/brandcode_config_fetcher.h"
#include "chrome/browser/profile_resetter/profile_resetter_test_base.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/google/core/browser/google_pref_names.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/shortcut.h"
#endif


namespace {

const char kDistributionConfig[] = "{"
    " \"homepage\" : \"http://www.foo.com\","
    " \"homepage_is_newtabpage\" : false,"
    " \"browser\" : {"
    "   \"show_home_button\" : true"
    "  },"
    " \"session\" : {"
    "   \"restore_on_startup\" : 4,"
    "   \"startup_urls\" : [\"http://goo.gl\", \"http://foo.de\"]"
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
class ProfileResetterTest : public extensions::ExtensionServiceTestBase,
                            public ProfileResetterTestBase {
 public:
  ProfileResetterTest();
  virtual ~ProfileResetterTest();

 protected:
  virtual void SetUp() OVERRIDE;

  TestingProfile* profile() { return profile_.get(); }

  static KeyedService* CreateTemplateURLService(
      content::BrowserContext* context);

 private:
#if defined(OS_WIN)
  base::ScopedPathOverride user_desktop_override_;
  base::ScopedPathOverride app_dir_override_;
  base::ScopedPathOverride start_menu_override_;
  base::ScopedPathOverride taskbar_pins_override_;
  base::win::ScopedCOMInitializer com_init_;
#endif
};

ProfileResetterTest::ProfileResetterTest()
#if defined(OS_WIN)
    : user_desktop_override_(base::DIR_USER_DESKTOP),
      app_dir_override_(base::DIR_APP_DATA),
      start_menu_override_(base::DIR_START_MENU),
      taskbar_pins_override_(base::DIR_TASKBAR_PINS)
#endif
{}

ProfileResetterTest::~ProfileResetterTest() {
}

void ProfileResetterTest::SetUp() {
  extensions::ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  profile()->CreateWebDataService();
  TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      &ProfileResetterTest::CreateTemplateURLService);
  resetter_.reset(new ProfileResetter(profile()));
}

// static
KeyedService* ProfileResetterTest::CreateTemplateURLService(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return new TemplateURLService(
      profile->GetPrefs(),
      scoped_ptr<SearchTermsData>(new UIThreadSearchTermsData(profile)),
      WebDataServiceFactory::GetKeywordWebDataForProfile(
          profile, Profile::EXPLICIT_ACCESS),
      scoped_ptr<TemplateURLServiceClient>(), NULL, NULL, base::Closure());
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
      net::HttpStatusCode response_code,
      net::URLRequestStatus::Status status);

  MOCK_METHOD0(Callback, void(void));

  base::MessageLoopForIO loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  URLFetcherRequestListener request_listener_;
  net::FakeURLFetcherFactory factory_;
};

ConfigParserTest::ConfigParserTest()
    : ui_thread_(content::BrowserThread::UI, &loop_),
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
    net::HttpStatusCode response_code,
    net::URLRequestStatus::Status status) {
  request_listener_.real_delegate = fetcher_delegate;
  scoped_ptr<net::FakeURLFetcher> fetcher(
      new net::FakeURLFetcher(
          url, &request_listener_, response_data, response_code, status));
  scoped_refptr<net::HttpResponseHeaders> download_headers =
      new net::HttpResponseHeaders("");
  download_headers->AddHeader("Content-Type: text/xml");
  fetcher->set_response_headers(download_headers);
  return fetcher.Pass();
}

// A helper class to create/delete/check a Chrome desktop shortcut on Windows.
class ShortcutHandler {
 public:
  ShortcutHandler();
  ~ShortcutHandler();

  static bool IsSupported();
  ShortcutCommand CreateWithArguments(const base::string16& name,
                                      const base::string16& args);
  void CheckShortcutHasArguments(const base::string16& desired_args) const;
  void Delete();

 private:
#if defined(OS_WIN)
  base::FilePath shortcut_path_;
#endif
  DISALLOW_COPY_AND_ASSIGN(ShortcutHandler);
};

#if defined(OS_WIN)
ShortcutHandler::ShortcutHandler() {
}

ShortcutHandler::~ShortcutHandler() {
  if (!shortcut_path_.empty())
    Delete();
}

// static
bool ShortcutHandler::IsSupported() {
  return true;
}

ShortcutCommand ShortcutHandler::CreateWithArguments(
    const base::string16& name,
    const base::string16& args) {
  EXPECT_TRUE(shortcut_path_.empty());
  base::FilePath path_to_create;
  EXPECT_TRUE(PathService::Get(base::DIR_USER_DESKTOP, &path_to_create));
  path_to_create = path_to_create.Append(name);
  EXPECT_FALSE(base::PathExists(path_to_create)) << path_to_create.value();

  base::FilePath path_exe;
  EXPECT_TRUE(PathService::Get(base::FILE_EXE, &path_exe));
  base::win::ShortcutProperties shortcut_properties;
  shortcut_properties.set_target(path_exe);
  shortcut_properties.set_arguments(args);
  EXPECT_TRUE(base::win::CreateOrUpdateShortcutLink(
      path_to_create, shortcut_properties,
      base::win::SHORTCUT_CREATE_ALWAYS)) << path_to_create.value();
  shortcut_path_ = path_to_create;
  return ShortcutCommand(shortcut_path_, args);
}

void ShortcutHandler::CheckShortcutHasArguments(
    const base::string16& desired_args) const {
  EXPECT_FALSE(shortcut_path_.empty());
  base::string16 args;
  EXPECT_TRUE(base::win::ResolveShortcut(shortcut_path_, NULL, &args));
  EXPECT_EQ(desired_args, args);
}

void ShortcutHandler::Delete() {
  EXPECT_FALSE(shortcut_path_.empty());
  EXPECT_TRUE(base::DeleteFile(shortcut_path_, false));
  shortcut_path_.clear();
}
#else
ShortcutHandler::ShortcutHandler() {}

ShortcutHandler::~ShortcutHandler() {}

// static
bool ShortcutHandler::IsSupported() {
  return false;
}

ShortcutCommand ShortcutHandler::CreateWithArguments(
    const base::string16& name,
    const base::string16& args) {
  return ShortcutCommand();
}

void ShortcutHandler::CheckShortcutHasArguments(
    const base::string16& desired_args) const {
}

void ShortcutHandler::Delete() {
}
#endif  // defined(OS_WIN)


// helper functions -----------------------------------------------------------

scoped_refptr<Extension> CreateExtension(const base::string16& name,
                                         const base::FilePath& path,
                                         Manifest::Location location,
                                         extensions::Manifest::Type type,
                                         bool installed_by_default) {
  base::DictionaryValue manifest;
  manifest.SetString(extensions::manifest_keys::kVersion, "1.0.0.0");
  manifest.SetString(extensions::manifest_keys::kName, name);
  switch (type) {
    case extensions::Manifest::TYPE_THEME:
      manifest.Set(extensions::manifest_keys::kTheme,
                   new base::DictionaryValue);
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

TEST_F(ProfileResetterTest, ResetNothing) {
  // The callback should be called even if there is nothing to reset.
  ResetAndWait(0);
}

TEST_F(ProfileResetterTest, ResetDefaultSearchEngineNonOrganic) {
  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  prefs->SetString(prefs::kLastPromptedGoogleURL, "http://www.foo.com/");

  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE, kDistributionConfig);

  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(profile());
  TemplateURL* default_engine = model->GetDefaultSearchProvider();
  ASSERT_NE(static_cast<TemplateURL*>(NULL), default_engine);
  EXPECT_EQ(base::ASCIIToUTF16("first"), default_engine->short_name());
  EXPECT_EQ(base::ASCIIToUTF16("firstkey"), default_engine->keyword());
  EXPECT_EQ("http://www.foo.com/s?q={searchTerms}", default_engine->url());

  EXPECT_EQ("", prefs->GetString(prefs::kLastPromptedGoogleURL));
}

TEST_F(ProfileResetterTest, ResetDefaultSearchEnginePartially) {
  // Search engine's logic is tested by
  // TemplateURLServiceTest.RepairPrepopulatedSearchEngines.
  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  prefs->SetString(prefs::kLastPromptedGoogleURL, "http://www.foo.com/");

  // Make sure TemplateURLService has loaded.
  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE);

  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(profile());
  TemplateURLService::TemplateURLVector urls = model->GetTemplateURLs();

  // The second call should produce no effect.
  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE);

  EXPECT_EQ(urls, model->GetTemplateURLs());
  EXPECT_EQ(std::string(), prefs->GetString(prefs::kLastPromptedGoogleURL));
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

TEST_F(ProfileResetterTest, ResetHomepagePartially) {
  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
  prefs->SetString(prefs::kHomePage, "http://www.foo.com");
  prefs->SetBoolean(prefs::kShowHomeButton, true);

  ResetAndWait(ProfileResetter::HOMEPAGE);

  EXPECT_TRUE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
  EXPECT_EQ("http://www.foo.com", prefs->GetString(prefs::kHomePage));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kShowHomeButton));
}

TEST_F(ProfileResetterTest, ResetContentSettings) {
  HostContentSettingsMap* host_content_settings_map =
      profile()->GetHostContentSettingsMap();
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.org");
  std::map<ContentSettingsType, ContentSetting> default_settings;

  for (int type = 0; type < CONTENT_SETTINGS_NUM_TYPES; ++type) {
    if (type == CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE ||
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

TEST_F(ProfileResetterTest, ResetExtensionsByDisabling) {
  service_->Init();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<Extension> theme =
      CreateExtension(base::ASCIIToUTF16("example1"),
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
      base::ASCIIToUTF16("example2"),
      base::FilePath(FILE_PATH_LITERAL("//nonexistent")),
      Manifest::INVALID_LOCATION,
      extensions::Manifest::TYPE_EXTENSION,
      false);
  service_->AddExtension(ext2.get());
  // Component extensions and policy-managed extensions shouldn't be disabled.
  scoped_refptr<Extension> ext3 = CreateExtension(
      base::ASCIIToUTF16("example3"),
      base::FilePath(FILE_PATH_LITERAL("//nonexistent2")),
      Manifest::COMPONENT,
      extensions::Manifest::TYPE_EXTENSION,
      false);
  service_->AddExtension(ext3.get());
  scoped_refptr<Extension> ext4 =
      CreateExtension(base::ASCIIToUTF16("example4"),
                      base::FilePath(FILE_PATH_LITERAL("//nonexistent3")),
                      Manifest::EXTERNAL_POLICY_DOWNLOAD,
                      extensions::Manifest::TYPE_EXTENSION,
                      false);
  service_->AddExtension(ext4.get());
  scoped_refptr<Extension> ext5 = CreateExtension(
      base::ASCIIToUTF16("example5"),
      base::FilePath(FILE_PATH_LITERAL("//nonexistent4")),
      Manifest::EXTERNAL_COMPONENT,
      extensions::Manifest::TYPE_EXTENSION,
      false);
  service_->AddExtension(ext5.get());
  scoped_refptr<Extension> ext6 = CreateExtension(
      base::ASCIIToUTF16("example6"),
      base::FilePath(FILE_PATH_LITERAL("//nonexistent5")),
      Manifest::EXTERNAL_POLICY,
      extensions::Manifest::TYPE_EXTENSION,
      false);
  service_->AddExtension(ext6.get());
  EXPECT_EQ(6u, service_->extensions()->size());

  ResetAndWait(ProfileResetter::EXTENSIONS);
  EXPECT_EQ(4u, service_->extensions()->size());
  EXPECT_FALSE(service_->extensions()->Contains(theme->id()));
  EXPECT_FALSE(service_->extensions()->Contains(ext2->id()));
  EXPECT_TRUE(service_->extensions()->Contains(ext3->id()));
  EXPECT_TRUE(service_->extensions()->Contains(ext4->id()));
  EXPECT_TRUE(service_->extensions()->Contains(ext5->id()));
  EXPECT_TRUE(service_->extensions()->Contains(ext6->id()));
  EXPECT_TRUE(theme_service->UsingDefaultTheme());
}

TEST_F(ProfileResetterTest, ResetExtensionsByDisablingNonOrganic) {
  scoped_refptr<Extension> ext2 = CreateExtension(
      base::ASCIIToUTF16("example2"),
      base::FilePath(FILE_PATH_LITERAL("//nonexistent")),
      Manifest::INVALID_LOCATION,
      extensions::Manifest::TYPE_EXTENSION,
      false);
  service_->AddExtension(ext2.get());
  // Components and external policy extensions shouldn't be deleted.
  scoped_refptr<Extension> ext3 = CreateExtension(
      base::ASCIIToUTF16("example3"),
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
      CreateExtension(base::ASCIIToUTF16("example1"),
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
      CreateExtension(base::ASCIIToUTF16("example2"),
                      base::FilePath(FILE_PATH_LITERAL("//nonexistent2")),
                      Manifest::INVALID_LOCATION,
                      extensions::Manifest::TYPE_EXTENSION,
                      false);
  service_->AddExtension(ext2.get());

  scoped_refptr<Extension> ext3 =
      CreateExtension(base::ASCIIToUTF16("example2"),
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


TEST_F(ProfileResetterTest, ResetStartPagePartially) {
  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);

  const GURL urls[] = {GURL("http://foo"), GURL("http://bar")};
  SessionStartupPref startup_pref(SessionStartupPref::URLS);
  startup_pref.urls.assign(urls, urls + arraysize(urls));
  SessionStartupPref::SetStartupPref(prefs, startup_pref);

  ResetAndWait(ProfileResetter::STARTUP_PAGES, std::string());

  startup_pref = SessionStartupPref::GetStartupPref(prefs);
  EXPECT_EQ(SessionStartupPref::GetDefaultStartupType(), startup_pref.type);
  EXPECT_EQ(std::vector<GURL>(urls, urls + arraysize(urls)), startup_pref.urls);
}

TEST_F(PinnedTabsResetTest, ResetPinnedTabs) {
  scoped_refptr<Extension> extension_app = CreateExtension(
      base::ASCIIToUTF16("hello!"),
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

TEST_F(ProfileResetterTest, ResetShortcuts) {
  ShortcutHandler shortcut;
  ShortcutCommand command_line = shortcut.CreateWithArguments(
      base::ASCIIToUTF16("chrome.lnk"),
      base::ASCIIToUTF16("--profile-directory=Default foo.com"));
  shortcut.CheckShortcutHasArguments(base::ASCIIToUTF16(
      "--profile-directory=Default foo.com"));

  ResetAndWait(ProfileResetter::SHORTCUTS);

  shortcut.CheckShortcutHasArguments(base::ASCIIToUTF16(
      "--profile-directory=Default"));
}

TEST_F(ProfileResetterTest, ResetFewFlags) {
  // mock_object_ is a StrictMock, so we verify that it is called only once.
  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE |
               ProfileResetter::HOMEPAGE |
               ProfileResetter::CONTENT_SETTINGS);
}

// Tries to load unavailable config file.
TEST_F(ConfigParserTest, NoConnectivity) {
  const GURL url("http://test");
  factory().SetFakeResponse(url, "", net::HTTP_INTERNAL_SERVER_ERROR,
                            net::URLRequestStatus::FAILED);

  scoped_ptr<BrandcodeConfigFetcher> fetcher = WaitForRequest(GURL(url));
  EXPECT_FALSE(fetcher->GetSettings());
}

// Tries to load available config file.
TEST_F(ConfigParserTest, ParseConfig) {
  const GURL url("http://test");
  std::string xml_config(kXmlConfig);
  ReplaceString(&xml_config, "placeholder_for_data", kDistributionConfig);
  ReplaceString(&xml_config,
                "placeholder_for_id",
                "abbaabbaabbaabbaabbaabbaabbaabba");
  factory().SetFakeResponse(url, xml_config, net::HTTP_OK,
                            net::URLRequestStatus::SUCCESS);

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
      base::ASCIIToUTF16("example"),
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
               ProfileResetter::STARTUP_PAGES,
               master_prefs);
  ShortcutHandler shortcut_hijacked;
  ShortcutCommand command_line = shortcut_hijacked.CreateWithArguments(
      base::ASCIIToUTF16("chrome1.lnk"),
      base::ASCIIToUTF16("--profile-directory=Default foo.com"));
  shortcut_hijacked.CheckShortcutHasArguments(
      base::ASCIIToUTF16("--profile-directory=Default foo.com"));
  ShortcutHandler shortcut_ok;
  shortcut_ok.CreateWithArguments(
      base::ASCIIToUTF16("chrome2.lnk"),
      base::ASCIIToUTF16("--profile-directory=Default1"));

  ResettableSettingsSnapshot nonorganic_snap(profile());
  nonorganic_snap.RequestShortcuts(base::Closure());
  // Let it enumerate shortcuts on the FILE thread.
  base::MessageLoop::current()->RunUntilIdle();
  int diff_fields = ResettableSettingsSnapshot::ALL_FIELDS;
  if (!ShortcutHandler::IsSupported())
    diff_fields &= ~ResettableSettingsSnapshot::SHORTCUTS;
  EXPECT_EQ(diff_fields,
            empty_snap.FindDifferentFields(nonorganic_snap));
  empty_snap.Subtract(nonorganic_snap);
  EXPECT_TRUE(empty_snap.startup_urls().empty());
  EXPECT_EQ(SessionStartupPref::GetDefaultStartupType(),
            empty_snap.startup_type());
  EXPECT_TRUE(empty_snap.homepage().empty());
  EXPECT_TRUE(empty_snap.homepage_is_ntp());
  EXPECT_NE(std::string::npos, empty_snap.dse_url().find("{google:baseURL}"));
  EXPECT_EQ(ResettableSettingsSnapshot::ExtensionList(),
            empty_snap.enabled_extensions());
  EXPECT_EQ(std::vector<ShortcutCommand>(), empty_snap.shortcuts());

  // Reset to organic defaults.
  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE |
               ProfileResetter::HOMEPAGE |
               ProfileResetter::STARTUP_PAGES |
               ProfileResetter::EXTENSIONS |
               ProfileResetter::SHORTCUTS);

  ResettableSettingsSnapshot organic_snap(profile());
  organic_snap.RequestShortcuts(base::Closure());
  // Let it enumerate shortcuts on the FILE thread.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(diff_fields, nonorganic_snap.FindDifferentFields(organic_snap));
  nonorganic_snap.Subtract(organic_snap);
  const GURL urls[] = {GURL("http://foo.de"), GURL("http://goo.gl")};
  EXPECT_EQ(std::vector<GURL>(urls, urls + arraysize(urls)),
            nonorganic_snap.startup_urls());
  EXPECT_EQ(SessionStartupPref::URLS, nonorganic_snap.startup_type());
  EXPECT_EQ("http://www.foo.com", nonorganic_snap.homepage());
  EXPECT_FALSE(nonorganic_snap.homepage_is_ntp());
  EXPECT_EQ("http://www.foo.com/s?q={searchTerms}", nonorganic_snap.dse_url());
  EXPECT_EQ(ResettableSettingsSnapshot::ExtensionList(
      1, std::make_pair(ext_id, "example")),
      nonorganic_snap.enabled_extensions());
  if (ShortcutHandler::IsSupported()) {
    std::vector<ShortcutCommand> shortcuts = nonorganic_snap.shortcuts();
    ASSERT_EQ(1u, shortcuts.size());
    EXPECT_EQ(command_line.first.value(), shortcuts[0].first.value());
    EXPECT_EQ(command_line.second, shortcuts[0].second);
  }
}

TEST_F(ProfileResetterTest, FeedbackSerializationTest) {
  // Reset to non organic defaults.
  ResetAndWait(ProfileResetter::DEFAULT_SEARCH_ENGINE |
               ProfileResetter::HOMEPAGE |
               ProfileResetter::STARTUP_PAGES,
               kDistributionConfig);

  scoped_refptr<Extension> ext = CreateExtension(
      base::ASCIIToUTF16("example"),
      base::FilePath(FILE_PATH_LITERAL("//nonexistent")),
      Manifest::INVALID_LOCATION,
      extensions::Manifest::TYPE_EXTENSION,
      false);
  ASSERT_TRUE(ext);
  service_->AddExtension(ext.get());

  ShortcutHandler shortcut;
  ShortcutCommand command_line = shortcut.CreateWithArguments(
      base::ASCIIToUTF16("chrome.lnk"),
      base::ASCIIToUTF16("--profile-directory=Default foo.com"));

  ResettableSettingsSnapshot nonorganic_snap(profile());
  nonorganic_snap.RequestShortcuts(base::Closure());
  // Let it enumerate shortcuts on the FILE thread.
  base::MessageLoop::current()->RunUntilIdle();

  COMPILE_ASSERT(ResettableSettingsSnapshot::ALL_FIELDS == 31,
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

    base::ListValue* startup_urls = NULL;
    int startup_type = 0;
    std::string homepage;
    bool homepage_is_ntp = true;
    std::string default_search_engine;
    base::ListValue* extensions = NULL;
    base::ListValue* shortcuts = NULL;

    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::STARTUP_MODE),
              dict->GetList("startup_urls", &startup_urls));
    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::STARTUP_MODE),
              dict->GetInteger("startup_type", &startup_type));
    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::HOMEPAGE),
              dict->GetString("homepage", &homepage));
    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::HOMEPAGE),
              dict->GetBoolean("homepage_is_ntp", &homepage_is_ntp));
    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::DSE_URL),
              dict->GetString("default_search_engine", &default_search_engine));
    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::EXTENSIONS),
              dict->GetList("enabled_extensions", &extensions));
    EXPECT_EQ(!!(field_mask & ResettableSettingsSnapshot::SHORTCUTS),
              dict->GetList("shortcuts", &shortcuts));
  }
}

struct FeedbackCapture {
  void SetFeedback(Profile* profile,
                   const ResettableSettingsSnapshot& snapshot) {
    list_ = GetReadableFeedbackForSnapshot(profile, snapshot).Pass();
    OnUpdatedList();
  }

  void Fail() {
    ADD_FAILURE() << "This method shouldn't be called.";
  }

  MOCK_METHOD0(OnUpdatedList, void(void));

  scoped_ptr<base::ListValue> list_;
};

// Make sure GetReadableFeedback handles non-ascii letters.
TEST_F(ProfileResetterTest, GetReadableFeedback) {
  scoped_refptr<Extension> ext = CreateExtension(
      base::WideToUTF16(L"Tiësto"),
      base::FilePath(FILE_PATH_LITERAL("//nonexistent")),
      Manifest::INVALID_LOCATION,
      extensions::Manifest::TYPE_EXTENSION,
      false);
  ASSERT_TRUE(ext);
  service_->AddExtension(ext.get());

  PrefService* prefs = profile()->GetPrefs();
  DCHECK(prefs);
  // The URL is "http://россия.рф".
  std::wstring url(L"http://"
    L"\u0440\u043e\u0441\u0441\u0438\u044f.\u0440\u0444");
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
  prefs->SetString(prefs::kHomePage, base::WideToUTF8(url));

  SessionStartupPref startup_pref(SessionStartupPref::URLS);
  startup_pref.urls.push_back(GURL(base::WideToUTF8(url)));
  SessionStartupPref::SetStartupPref(prefs, startup_pref);

  ShortcutHandler shortcut;
  ShortcutCommand command_line = shortcut.CreateWithArguments(
      base::ASCIIToUTF16("chrome.lnk"),
      base::ASCIIToUTF16("--profile-directory=Default foo.com"));

  FeedbackCapture capture;
  EXPECT_CALL(capture, OnUpdatedList());
  ResettableSettingsSnapshot snapshot(profile());
  snapshot.RequestShortcuts(base::Bind(&FeedbackCapture::SetFeedback,
                                       base::Unretained(&capture),
                                       profile(),
                                       base::ConstRef(snapshot)));
  // Let it enumerate shortcuts on the FILE thread.
  base::MessageLoop::current()->RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(&capture);
  // The homepage and the startup page are in punycode. They are unreadable.
  // Trying to find the extension name.
  scoped_ptr<base::ListValue> list = capture.list_.Pass();
  ASSERT_TRUE(list);
  bool checked_extensions = false;
  bool checked_shortcuts = false;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    base::DictionaryValue* dict = NULL;
    ASSERT_TRUE(list->GetDictionary(i, &dict));
    std::string value;
    ASSERT_TRUE(dict->GetString("key", &value));
    if (value == "Extensions") {
      base::string16 extensions;
      EXPECT_TRUE(dict->GetString("value", &extensions));
      EXPECT_EQ(base::WideToUTF16(L"Tiësto"), extensions);
      checked_extensions = true;
    } else if (value == "Shortcut targets") {
      base::string16 targets;
      EXPECT_TRUE(dict->GetString("value", &targets));
      EXPECT_NE(base::string16::npos,
                targets.find(base::ASCIIToUTF16("foo.com"))) << targets;
      checked_shortcuts = true;
    }
  }
  EXPECT_TRUE(checked_extensions);
  EXPECT_EQ(ShortcutHandler::IsSupported(), checked_shortcuts);
}

TEST_F(ProfileResetterTest, DestroySnapshotFast) {
  FeedbackCapture capture;
  scoped_ptr<ResettableSettingsSnapshot> deleted_snapshot(
      new ResettableSettingsSnapshot(profile()));
  deleted_snapshot->RequestShortcuts(base::Bind(&FeedbackCapture::Fail,
                                                base::Unretained(&capture)));
  deleted_snapshot.reset();
  // Running remaining tasks shouldn't trigger the callback to be called as
  // |deleted_snapshot| was deleted before it could run.
  base::MessageLoop::current()->RunUntilIdle();
}

}  // namespace
