// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/supervised_user/custodian_profile_downloader_service.h"
#include "chrome/browser/supervised_user/custodian_profile_downloader_service_factory.h"
#include "chrome/browser/supervised_user/permission_request_creator.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#endif

using content::MessageLoopRunner;

namespace {

void OnProfileDownloadedFail(const base::string16& full_name) {
  ASSERT_TRUE(false) << "Profile download should not have succeeded.";
}

class SupervisedUserURLFilterObserver :
    public SupervisedUserURLFilter::Observer {
 public:
  explicit SupervisedUserURLFilterObserver(SupervisedUserURLFilter* url_filter)
      : url_filter_(url_filter) {
    Reset();
    url_filter_->AddObserver(this);
  }

  ~SupervisedUserURLFilterObserver() {
    url_filter_->RemoveObserver(this);
  }

  void Wait() {
    message_loop_runner_->Run();
    Reset();
  }

  // SupervisedUserURLFilter::Observer
  void OnSiteListUpdated() override { message_loop_runner_->Quit(); }

 private:
  void Reset() {
    message_loop_runner_ = new MessageLoopRunner;
  }

  SupervisedUserURLFilter* url_filter_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

class AsyncResultHolder {
 public:
  AsyncResultHolder() : result_(false) {}
  ~AsyncResultHolder() {}

  void SetResult(bool result) {
    result_ = result;
    run_loop_.Quit();
  }

  bool GetResult() {
    run_loop_.Run();
    return result_;
  }

 private:
  base::RunLoop run_loop_;
  bool result_;

  DISALLOW_COPY_AND_ASSIGN(AsyncResultHolder);
};

class SupervisedUserServiceTest : public ::testing::Test {
 public:
  SupervisedUserServiceTest() {}

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                              BuildFakeProfileOAuth2TokenService);
    profile_ = builder.Build();
    supervised_user_service_ =
        SupervisedUserServiceFactory::GetForProfile(profile_.get());
  }

  void TearDown() override { profile_.reset(); }

  ~SupervisedUserServiceTest() override {}

 protected:
  void AddAccessRequest(const GURL& url, AsyncResultHolder* result_holder) {
    supervised_user_service_->AddAccessRequest(
        url, base::Bind(&AsyncResultHolder::SetResult,
                        base::Unretained(result_holder)));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;
  SupervisedUserService* supervised_user_service_;
};

}  // namespace

TEST_F(SupervisedUserServiceTest, ChangesIncludedSessionOnChangedSettings) {
  supervised_user_service_->Init();
  EXPECT_TRUE(supervised_user_service_->IncludesSyncSessionsType());
  profile_->GetPrefs()->SetBoolean(prefs::kRecordHistory, false);
  EXPECT_FALSE(supervised_user_service_->IncludesSyncSessionsType());
}

// Ensure that the CustodianProfileDownloaderService shuts down cleanly. If no
// DCHECK is hit when the service is destroyed, this test passed.
TEST_F(SupervisedUserServiceTest, ShutDownCustodianProfileDownloader) {
  CustodianProfileDownloaderService* downloader_service =
      CustodianProfileDownloaderServiceFactory::GetForProfile(profile_.get());

  // Emulate being logged in, then start to download a profile so a
  // ProfileDownloader gets created.
  SigninManagerFactory::GetForProfile(profile_.get())->
      SetAuthenticatedUsername("Logged In");
  downloader_service->DownloadProfile(base::Bind(&OnProfileDownloadedFail));
}

namespace {

class MockPermissionRequestCreator : public PermissionRequestCreator {
 public:
  MockPermissionRequestCreator() : enabled_(false) {}
  ~MockPermissionRequestCreator() override {}

  void set_enabled(bool enabled) {
    enabled_ = enabled;
  }

  const std::vector<GURL>& requested_urls() const {
    return requested_urls_;
  }

  void AnswerRequest(size_t index, bool result) {
    ASSERT_LT(index, requested_urls_.size());
    callbacks_[index].Run(result);
    callbacks_.erase(callbacks_.begin() + index);
    requested_urls_.erase(requested_urls_.begin() + index);
  }

 private:
  // PermissionRequestCreator:
  bool IsEnabled() const override { return enabled_; }

  void CreatePermissionRequest(const GURL& url_requested,
                               const SuccessCallback& callback) override {
    ASSERT_TRUE(enabled_);
    requested_urls_.push_back(url_requested);
    callbacks_.push_back(callback);
  }

  bool enabled_;
  std::vector<GURL> requested_urls_;
  std::vector<SuccessCallback> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(MockPermissionRequestCreator);
};

}  // namespace

TEST_F(SupervisedUserServiceTest, CreatePermissionRequest) {
  GURL url("http://www.example.com");

  // Without any permission request creators, it should be disabled, and any
  // AddAccessRequest() calls should fail.
  EXPECT_FALSE(supervised_user_service_->AccessRequestsEnabled());
  {
    AsyncResultHolder result_holder;
    AddAccessRequest(url, &result_holder);
    EXPECT_FALSE(result_holder.GetResult());
  }

  // Add a disabled permission request creator. This should not change anything.
  MockPermissionRequestCreator* creator = new MockPermissionRequestCreator;
  supervised_user_service_->AddPermissionRequestCreatorForTesting(creator);

  EXPECT_FALSE(supervised_user_service_->AccessRequestsEnabled());
  {
    AsyncResultHolder result_holder;
    AddAccessRequest(url, &result_holder);
    EXPECT_FALSE(result_holder.GetResult());
  }

  // Enable the permission request creator. This should enable permission
  // requests and queue them up.
  creator->set_enabled(true);
  EXPECT_TRUE(supervised_user_service_->AccessRequestsEnabled());
  {
    AsyncResultHolder result_holder;
    AddAccessRequest(url, &result_holder);
    ASSERT_EQ(1u, creator->requested_urls().size());
    EXPECT_EQ(url.spec(), creator->requested_urls()[0].spec());

    creator->AnswerRequest(0, true);
    EXPECT_TRUE(result_holder.GetResult());
  }

  {
    AsyncResultHolder result_holder;
    AddAccessRequest(url, &result_holder);
    ASSERT_EQ(1u, creator->requested_urls().size());
    EXPECT_EQ(url.spec(), creator->requested_urls()[0].spec());

    creator->AnswerRequest(0, false);
    EXPECT_FALSE(result_holder.GetResult());
  }

  // Add a second permission request creator.
  MockPermissionRequestCreator* creator_2 = new MockPermissionRequestCreator;
  creator_2->set_enabled(true);
  supervised_user_service_->AddPermissionRequestCreatorForTesting(creator_2);

  {
    AsyncResultHolder result_holder;
    AddAccessRequest(url, &result_holder);
    ASSERT_EQ(1u, creator->requested_urls().size());
    EXPECT_EQ(url.spec(), creator->requested_urls()[0].spec());

    // Make the first creator succeed. This should make the whole thing succeed.
    creator->AnswerRequest(0, true);
    EXPECT_TRUE(result_holder.GetResult());
  }

  {
    AsyncResultHolder result_holder;
    AddAccessRequest(url, &result_holder);
    ASSERT_EQ(1u, creator->requested_urls().size());
    EXPECT_EQ(url.spec(), creator->requested_urls()[0].spec());

    // Make the first creator fail. This should fall back to the second one.
    creator->AnswerRequest(0, false);
    ASSERT_EQ(1u, creator_2->requested_urls().size());
    EXPECT_EQ(url.spec(), creator_2->requested_urls()[0].spec());

    // Make the second creator succeed, which will make the whole thing succeed.
    creator_2->AnswerRequest(0, true);
    EXPECT_TRUE(result_holder.GetResult());
  }
}

#if defined(ENABLE_EXTENSIONS)
class SupervisedUserServiceExtensionTestBase
    : public extensions::ExtensionServiceTestBase {
 public:
  explicit SupervisedUserServiceExtensionTestBase(bool is_supervised)
      : is_supervised_(is_supervised),
        channel_(chrome::VersionInfo::CHANNEL_DEV) {}
  ~SupervisedUserServiceExtensionTestBase() override {}

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    ExtensionServiceTestBase::ExtensionServiceInitParams params =
        CreateDefaultInitParams();
    params.profile_is_supervised = is_supervised_;
    InitializeExtensionService(params);
    SupervisedUserServiceFactory::GetForProfile(profile_.get())->Init();
  }

 protected:
  ScopedVector<SupervisedUserSiteList> GetActiveSiteLists(
      SupervisedUserService* supervised_user_service) {
    return supervised_user_service->GetActiveSiteLists();
  }

  scoped_refptr<extensions::Extension> MakeThemeExtension() {
    scoped_ptr<base::DictionaryValue> source(new base::DictionaryValue());
    source->SetString(extensions::manifest_keys::kName, "Theme");
    source->Set(extensions::manifest_keys::kTheme, new base::DictionaryValue());
    source->SetString(extensions::manifest_keys::kVersion, "1.0");
    extensions::ExtensionBuilder builder;
    scoped_refptr<extensions::Extension> extension =
        builder.SetManifest(source.Pass()).Build();
    return extension;
  }

  scoped_refptr<extensions::Extension> MakeExtension(bool by_custodian) {
    scoped_ptr<base::DictionaryValue> manifest = extensions::DictionaryBuilder()
      .Set(extensions::manifest_keys::kName, "Extension")
      .Set(extensions::manifest_keys::kVersion, "1.0")
      .Build();
    int creation_flags = extensions::Extension::NO_FLAGS;
    if (by_custodian)
      creation_flags |= extensions::Extension::WAS_INSTALLED_BY_CUSTODIAN;
    extensions::ExtensionBuilder builder;
    scoped_refptr<extensions::Extension> extension =
        builder.SetManifest(manifest.Pass()).AddFlags(creation_flags).Build();
    return extension;
  }

  bool is_supervised_;
  extensions::ScopedCurrentChannel channel_;
};

class SupervisedUserServiceExtensionTestUnsupervised
    : public SupervisedUserServiceExtensionTestBase {
 public:
  SupervisedUserServiceExtensionTestUnsupervised()
      : SupervisedUserServiceExtensionTestBase(false) {}
};

class SupervisedUserServiceExtensionTest
    : public SupervisedUserServiceExtensionTestBase {
 public:
  SupervisedUserServiceExtensionTest()
      : SupervisedUserServiceExtensionTestBase(true) {}
};

TEST_F(SupervisedUserServiceExtensionTestUnsupervised,
       ExtensionManagementPolicyProvider) {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(profile_->IsSupervised());

  scoped_refptr<extensions::Extension> extension = MakeExtension(false);
  base::string16 error_1;
  EXPECT_TRUE(supervised_user_service->UserMayLoad(extension.get(), &error_1));
  EXPECT_EQ(base::string16(), error_1);

  base::string16 error_2;
  EXPECT_TRUE(
      supervised_user_service->UserMayModifySettings(extension.get(),
                                                     &error_2));
  EXPECT_EQ(base::string16(), error_2);
}

TEST_F(SupervisedUserServiceExtensionTest, ExtensionManagementPolicyProvider) {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_.get());
  SupervisedUserURLFilterObserver observer(
      supervised_user_service->GetURLFilterForUIThread());
  ASSERT_TRUE(profile_->IsSupervised());
  // Wait for the initial update to finish (otherwise we'll get leaks).
  observer.Wait();

  // Check that a supervised user can install a theme.
  scoped_refptr<extensions::Extension> theme = MakeThemeExtension();
  base::string16 error_1;
  EXPECT_TRUE(supervised_user_service->UserMayLoad(theme.get(), &error_1));
  EXPECT_TRUE(error_1.empty());
  EXPECT_TRUE(
      supervised_user_service->UserMayModifySettings(theme.get(), &error_1));
  EXPECT_TRUE(error_1.empty());

  // Now check a different kind of extension.
  scoped_refptr<extensions::Extension> extension = MakeExtension(false);
  EXPECT_FALSE(supervised_user_service->UserMayLoad(extension.get(), &error_1));
  EXPECT_FALSE(error_1.empty());

  base::string16 error_2;
  EXPECT_FALSE(supervised_user_service->UserMayModifySettings(extension.get(),
                                                              &error_2));
  EXPECT_FALSE(error_2.empty());

  // Check that an extension that was installed by the custodian may be loaded.
  base::string16 error_3;
  scoped_refptr<extensions::Extension> extension_2 = MakeExtension(true);
  EXPECT_TRUE(supervised_user_service->UserMayLoad(extension_2.get(),
                                                   &error_3));
  EXPECT_TRUE(error_3.empty());

  // The supervised user should still not be able to uninstall or disable the
  // extension.
  base::string16 error_4;
  EXPECT_FALSE(supervised_user_service->UserMayModifySettings(extension_2.get(),
                                                              &error_4));
  EXPECT_FALSE(error_4.empty());

#ifndef NDEBUG
  EXPECT_FALSE(supervised_user_service->GetDebugPolicyProviderName().empty());
#endif
}

TEST_F(SupervisedUserServiceExtensionTest, NoContentPacks) {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_.get());
  SupervisedUserURLFilter* url_filter =
      supervised_user_service->GetURLFilterForUIThread();

  GURL url("http://youtube.com");
  ScopedVector<SupervisedUserSiteList> site_lists =
      GetActiveSiteLists(supervised_user_service);
  ASSERT_EQ(0u, site_lists.size());
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            url_filter->GetFilteringBehaviorForURL(url));
}

TEST_F(SupervisedUserServiceExtensionTest, InstallContentPacks) {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_.get());
  SupervisedUserURLFilter* url_filter =
      supervised_user_service->GetURLFilterForUIThread();
  SupervisedUserURLFilterObserver observer(url_filter);
  observer.Wait();

  GURL example_url("http://example.com");
  GURL moose_url("http://moose.org");
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            url_filter->GetFilteringBehaviorForURL(example_url));

  profile_->GetPrefs()->SetInteger(
      prefs::kDefaultSupervisedUserFilteringBehavior,
      SupervisedUserURLFilter::BLOCK);
  EXPECT_EQ(SupervisedUserURLFilter::BLOCK,
            url_filter->GetFilteringBehaviorForURL(example_url));

  profile_->GetPrefs()->SetInteger(
      prefs::kDefaultSupervisedUserFilteringBehavior,
      SupervisedUserURLFilter::WARN);
  EXPECT_EQ(SupervisedUserURLFilter::WARN,
            url_filter->GetFilteringBehaviorForURL(example_url));

  supervised_user_service->set_elevated_for_testing(true);

  // Load a content pack.
  scoped_refptr<extensions::UnpackedInstaller> installer(
      extensions::UnpackedInstaller::Create(service_));
  installer->set_prompt_for_plugins(false);
  base::FilePath test_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  base::FilePath extension_path =
      test_data_dir.AppendASCII("extensions/supervised_user/content_pack");
  content::WindowedNotificationObserver extension_load_observer(
      extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
      content::Source<Profile>(profile_.get()));
  installer->Load(extension_path);
  extension_load_observer.Wait();
  observer.Wait();
  content::Details<extensions::Extension> details =
      extension_load_observer.details();
  scoped_refptr<extensions::Extension> extension =
      make_scoped_refptr(details.ptr());
  ASSERT_TRUE(extension.get());

  ScopedVector<SupervisedUserSiteList> site_lists =
      GetActiveSiteLists(supervised_user_service);
  ASSERT_EQ(1u, site_lists.size());
  std::vector<SupervisedUserSiteList::Site> sites;
  site_lists[0]->GetSites(&sites);
  ASSERT_EQ(3u, sites.size());
  EXPECT_EQ(base::ASCIIToUTF16("YouTube"), sites[0].name);
  EXPECT_EQ(base::ASCIIToUTF16("Homestar Runner"), sites[1].name);
  EXPECT_EQ(base::string16(), sites[2].name);

  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            url_filter->GetFilteringBehaviorForURL(example_url));
  EXPECT_EQ(SupervisedUserURLFilter::WARN,
            url_filter->GetFilteringBehaviorForURL(moose_url));

  // Load a second content pack.
  installer = extensions::UnpackedInstaller::Create(service_);
  extension_path =
      test_data_dir.AppendASCII("extensions/supervised_user/content_pack_2");
  installer->Load(extension_path);
  observer.Wait();

  site_lists = GetActiveSiteLists(supervised_user_service);
  ASSERT_EQ(2u, site_lists.size());
  sites.clear();
  site_lists[0]->GetSites(&sites);
  site_lists[1]->GetSites(&sites);
  ASSERT_EQ(4u, sites.size());
  // The site lists might be returned in any order, so we put them into a set.
  std::set<std::string> site_names;
  for (const SupervisedUserSiteList::Site& site : sites)
    site_names.insert(base::UTF16ToUTF8(site.name));
  EXPECT_EQ(1u, site_names.count("YouTube"));
  EXPECT_EQ(1u, site_names.count("Homestar Runner"));
  EXPECT_EQ(1u, site_names.count(std::string()));
  EXPECT_EQ(1u, site_names.count("Moose"));

  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            url_filter->GetFilteringBehaviorForURL(example_url));
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            url_filter->GetFilteringBehaviorForURL(moose_url));

  // Disable the first content pack.
  service_->DisableExtension(extension->id(),
                             extensions::Extension::DISABLE_USER_ACTION);
  observer.Wait();

  site_lists = GetActiveSiteLists(supervised_user_service);
  ASSERT_EQ(1u, site_lists.size());
  sites.clear();
  site_lists[0]->GetSites(&sites);
  ASSERT_EQ(1u, sites.size());
  EXPECT_EQ(base::ASCIIToUTF16("Moose"), sites[0].name);

  EXPECT_EQ(SupervisedUserURLFilter::WARN,
            url_filter->GetFilteringBehaviorForURL(example_url));
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            url_filter->GetFilteringBehaviorForURL(moose_url));
}
#endif  // defined(ENABLE_EXTENSIONS)
