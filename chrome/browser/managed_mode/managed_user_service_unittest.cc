// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::MessageLoopRunner;

namespace {

class ManagedModeURLFilterObserver : public ManagedModeURLFilter::Observer {
 public:
  explicit ManagedModeURLFilterObserver(ManagedModeURLFilter* url_filter)
      : url_filter_(url_filter) {
    Reset();
    url_filter_->AddObserver(this);
  }

  ~ManagedModeURLFilterObserver() {
    url_filter_->RemoveObserver(this);
  }

  void Wait() {
    message_loop_runner_->Run();
    Reset();
  }

  // ManagedModeURLFilter::Observer
  virtual void OnSiteListUpdated() OVERRIDE {
    message_loop_runner_->Quit();
  }

 private:
  void Reset() {
    message_loop_runner_ = new MessageLoopRunner;
  }

  ManagedModeURLFilter* url_filter_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

class ManagedUserServiceTest : public ::testing::Test {
 public:
  ManagedUserServiceTest() : ui_thread_(content::BrowserThread::UI,
                                        &message_loop_) {
    managed_user_service_ = ManagedUserServiceFactory::GetForProfile(&profile_);
  }

  virtual ~ManagedUserServiceTest() {}

 protected:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  TestingProfile profile_;
  ManagedUserService* managed_user_service_;
};

}  // namespace

TEST_F(ManagedUserServiceTest, GetManualExceptionsForHost) {
  GURL kExampleFooURL("http://www.example.com/foo");
  GURL kExampleBarURL("http://www.example.com/bar");
  GURL kExampleFooNoWWWURL("http://example.com/foo");
  GURL kBlurpURL("http://blurp.net/bla");
  GURL kMooseURL("http://moose.org/baz");
  {
    DictionaryPrefUpdate update(profile_.GetPrefs(),
                                prefs::kManagedModeManualURLs);
    base::DictionaryValue* dict = update.Get();
    dict->SetBooleanWithoutPathExpansion(kExampleFooURL.spec(), true);
    dict->SetBooleanWithoutPathExpansion(kExampleBarURL.spec(), false);
    dict->SetBooleanWithoutPathExpansion(kExampleFooNoWWWURL.spec(), true);
    dict->SetBooleanWithoutPathExpansion(kBlurpURL.spec(), true);
  }

  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForURL(kExampleFooURL));
  EXPECT_EQ(ManagedUserService::MANUAL_BLOCK,
            managed_user_service_->GetManualBehaviorForURL(kExampleBarURL));
  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForURL(
                kExampleFooNoWWWURL));
  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForURL(kBlurpURL));
  EXPECT_EQ(ManagedUserService::MANUAL_NONE,
            managed_user_service_->GetManualBehaviorForURL(kMooseURL));
  std::vector<GURL> exceptions;
  managed_user_service_->GetManualExceptionsForHost("www.example.com",
                                                    &exceptions);
  ASSERT_EQ(2u, exceptions.size());
  EXPECT_EQ(kExampleBarURL, exceptions[0]);
  EXPECT_EQ(kExampleFooURL, exceptions[1]);

  {
    DictionaryPrefUpdate update(profile_.GetPrefs(),
                                prefs::kManagedModeManualURLs);
    base::DictionaryValue* dict = update.Get();
    for (std::vector<GURL>::iterator it = exceptions.begin();
         it != exceptions.end(); ++it) {
      dict->RemoveWithoutPathExpansion(it->spec(), NULL);
    }
  }

  EXPECT_EQ(ManagedUserService::MANUAL_NONE,
            managed_user_service_->GetManualBehaviorForURL(kExampleFooURL));
  EXPECT_EQ(ManagedUserService::MANUAL_NONE,
            managed_user_service_->GetManualBehaviorForURL(kExampleBarURL));
  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForURL(
                kExampleFooNoWWWURL));
  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForURL(kBlurpURL));
  EXPECT_EQ(ManagedUserService::MANUAL_NONE,
            managed_user_service_->GetManualBehaviorForURL(kMooseURL));
}

class ManagedUserServiceExtensionTest : public ExtensionServiceTestBase {
 public:
  ManagedUserServiceExtensionTest() {}
  virtual ~ManagedUserServiceExtensionTest() {}

  virtual void SetUp() OVERRIDE {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
  }

  virtual void TearDown() OVERRIDE {
    ExtensionServiceTestBase::TearDown();
  }

 protected:
  ScopedVector<ManagedModeSiteList> GetActiveSiteLists(
      ManagedUserService* managed_user_service) {
    return managed_user_service->GetActiveSiteLists();
  }

  scoped_refptr<extensions::Extension> MakeThemeExtension() {
    scoped_ptr<DictionaryValue> source(new DictionaryValue());
    source->SetString(extensions::manifest_keys::kName, "Theme");
    source->Set(extensions::manifest_keys::kTheme, new DictionaryValue());
    source->SetString(extensions::manifest_keys::kVersion, "1.0");
    extensions::ExtensionBuilder builder;
    scoped_refptr<extensions::Extension> extension =
        builder.SetManifest(source.Pass()).Build();
    return extension;
  }

  scoped_refptr<extensions::Extension> MakeExtension() {
    scoped_ptr<DictionaryValue> manifest = extensions::DictionaryBuilder()
      .Set(extensions::manifest_keys::kName, "Extension")
      .Set(extensions::manifest_keys::kVersion, "1.0")
      .Build();
    extensions::ExtensionBuilder builder;
    scoped_refptr<extensions::Extension> extension =
        builder.SetManifest(manifest.Pass()).Build();
    return extension;
  }
};

TEST_F(ManagedUserServiceExtensionTest,
       ExtensionManagementPolicyProviderUnmanaged) {
  ManagedUserService* managed_user_service =
      ManagedUserServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(profile_->IsManaged());

  scoped_refptr<extensions::Extension> extension = MakeExtension();
  string16 error_1;
  EXPECT_TRUE(managed_user_service->UserMayLoad(extension.get(), &error_1));
  EXPECT_EQ(string16(), error_1);

  string16 error_2;
  EXPECT_TRUE(
      managed_user_service->UserMayModifySettings(extension.get(), &error_2));
  EXPECT_EQ(string16(), error_2);
}

TEST_F(ManagedUserServiceExtensionTest,
       ExtensionManagementPolicyProviderManaged) {
  ManagedUserService* managed_user_service =
      ManagedUserServiceFactory::GetForProfile(profile_.get());
  managed_user_service->InitForTesting();
  ManagedModeURLFilterObserver observer(
      managed_user_service->GetURLFilterForUIThread());
  EXPECT_TRUE(profile_->IsManaged());
  // Wait for the initial update to finish (otherwise we'll get leaks).
  observer.Wait();

  // Check that a supervised user can install a theme.
  scoped_refptr<extensions::Extension> theme = MakeThemeExtension();
  string16 error_1;
  EXPECT_TRUE(managed_user_service->UserMayLoad(theme.get(), &error_1));
  EXPECT_TRUE(error_1.empty());
  EXPECT_TRUE(
      managed_user_service->UserMayModifySettings(theme.get(), &error_1));
  EXPECT_TRUE(error_1.empty());

  // Now check a different kind of extension.
  scoped_refptr<extensions::Extension> extension = MakeExtension();
  EXPECT_FALSE(managed_user_service->UserMayLoad(extension.get(), &error_1));
  EXPECT_FALSE(error_1.empty());

  string16 error_2;
  EXPECT_FALSE(
      managed_user_service->UserMayModifySettings(extension.get(), &error_2));
  EXPECT_FALSE(error_2.empty());

#ifndef NDEBUG
  EXPECT_FALSE(managed_user_service->GetDebugPolicyProviderName().empty());
#endif
}


TEST_F(ManagedUserServiceExtensionTest, NoContentPacks) {
  ManagedUserService* managed_user_service =
      ManagedUserServiceFactory::GetForProfile(profile_.get());
  managed_user_service->Init();
  ManagedModeURLFilter* url_filter =
      managed_user_service->GetURLFilterForUIThread();

  GURL url("http://youtube.com");
  ScopedVector<ManagedModeSiteList> site_lists =
      GetActiveSiteLists(managed_user_service);
  ASSERT_EQ(0u, site_lists.size());
  EXPECT_EQ(ManagedModeURLFilter::ALLOW,
            url_filter->GetFilteringBehaviorForURL(url));
}

TEST_F(ManagedUserServiceExtensionTest, InstallContentPacks) {
  ManagedUserService* managed_user_service =
      ManagedUserServiceFactory::GetForProfile(profile_.get());
  managed_user_service->InitForTesting();
  ManagedModeURLFilter* url_filter =
      managed_user_service->GetURLFilterForUIThread();
  ManagedModeURLFilterObserver observer(url_filter);
  observer.Wait();

  GURL example_url("http://example.com");
  GURL moose_url("http://moose.org");
  EXPECT_EQ(ManagedModeURLFilter::ALLOW,
            url_filter->GetFilteringBehaviorForURL(example_url));

  profile_->GetPrefs()->SetInteger(prefs::kDefaultManagedModeFilteringBehavior,
                                   ManagedModeURLFilter::BLOCK);
  EXPECT_EQ(ManagedModeURLFilter::BLOCK,
            url_filter->GetFilteringBehaviorForURL(example_url));

  profile_->GetPrefs()->SetInteger(prefs::kDefaultManagedModeFilteringBehavior,
                                   ManagedModeURLFilter::WARN);
  EXPECT_EQ(ManagedModeURLFilter::WARN,
            url_filter->GetFilteringBehaviorForURL(example_url));

  managed_user_service->set_elevated_for_testing(true);

  // Load a content pack.
  scoped_refptr<extensions::UnpackedInstaller> installer(
      extensions::UnpackedInstaller::Create(service_));
  installer->set_prompt_for_plugins(false);
  base::FilePath test_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  base::FilePath extension_path =
      test_data_dir.AppendASCII("extensions/managed_mode/content_pack");
  content::WindowedNotificationObserver extension_load_observer(
      chrome::NOTIFICATION_EXTENSION_LOADED,
      content::Source<Profile>(profile_.get()));
  installer->Load(extension_path);
  extension_load_observer.Wait();
  observer.Wait();
  content::Details<extensions::Extension> details =
      extension_load_observer.details();
  scoped_refptr<extensions::Extension> extension =
      make_scoped_refptr(details.ptr());
  ASSERT_TRUE(extension.get());

  ScopedVector<ManagedModeSiteList> site_lists =
      GetActiveSiteLists(managed_user_service);
  ASSERT_EQ(1u, site_lists.size());
  std::vector<ManagedModeSiteList::Site> sites;
  site_lists[0]->GetSites(&sites);
  ASSERT_EQ(3u, sites.size());
  EXPECT_EQ(ASCIIToUTF16("YouTube"), sites[0].name);
  EXPECT_EQ(ASCIIToUTF16("Homestar Runner"), sites[1].name);
  EXPECT_EQ(string16(), sites[2].name);

  EXPECT_EQ(ManagedModeURLFilter::ALLOW,
            url_filter->GetFilteringBehaviorForURL(example_url));
  EXPECT_EQ(ManagedModeURLFilter::WARN,
            url_filter->GetFilteringBehaviorForURL(moose_url));

  // Load a second content pack.
  installer = extensions::UnpackedInstaller::Create(service_);
  extension_path =
      test_data_dir.AppendASCII("extensions/managed_mode/content_pack_2");
  installer->Load(extension_path);
  observer.Wait();

  site_lists = GetActiveSiteLists(managed_user_service);
  ASSERT_EQ(2u, site_lists.size());
  sites.clear();
  site_lists[0]->GetSites(&sites);
  site_lists[1]->GetSites(&sites);
  ASSERT_EQ(4u, sites.size());
  // The site lists might be returned in any order, so we put them into a set.
  std::set<std::string> site_names;
  for (std::vector<ManagedModeSiteList::Site>::const_iterator it =
      sites.begin(); it != sites.end(); ++it) {
    site_names.insert(UTF16ToUTF8(it->name));
  }
  EXPECT_TRUE(site_names.count("YouTube") == 1u);
  EXPECT_TRUE(site_names.count("Homestar Runner") == 1u);
  EXPECT_TRUE(site_names.count(std::string()) == 1u);
  EXPECT_TRUE(site_names.count("Moose") == 1u);

  EXPECT_EQ(ManagedModeURLFilter::ALLOW,
            url_filter->GetFilteringBehaviorForURL(example_url));
  EXPECT_EQ(ManagedModeURLFilter::ALLOW,
            url_filter->GetFilteringBehaviorForURL(moose_url));

  // Disable the first content pack.
  service_->DisableExtension(extension->id(),
                             extensions::Extension::DISABLE_USER_ACTION);
  observer.Wait();

  site_lists = GetActiveSiteLists(managed_user_service);
  ASSERT_EQ(1u, site_lists.size());
  sites.clear();
  site_lists[0]->GetSites(&sites);
  ASSERT_EQ(1u, sites.size());
  EXPECT_EQ(ASCIIToUTF16("Moose"), sites[0].name);

  EXPECT_EQ(ManagedModeURLFilter::WARN,
            url_filter->GetFilteringBehaviorForURL(example_url));
  EXPECT_EQ(ManagedModeURLFilter::ALLOW,
            url_filter->GetFilteringBehaviorForURL(moose_url));
}
