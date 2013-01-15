// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_manager_fake.h"
#include "chrome/browser/signin/signin_names_io_thread.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_renderer_host.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace {

// Explicit URLs are sign in URLs created by chrome for specific sign in access
// points.  Implicit URLs are those to sign for some Google service, like gmail
// or drive.  In former case, with a valid URL, we don't want to offer the
// interstitial.  In all other cases we do.

const char kImplicitURLString[] =
    "https://accounts.google.com/ServiceLogin"
    "?service=foo&continue=http://foo.google.com";

class SigninManagerMock : public FakeSigninManager {
 public:
  explicit SigninManagerMock(Profile* profile)
      : FakeSigninManager(profile) {}
  MOCK_CONST_METHOD1(IsAllowedUsername, bool(const std::string& username));
};

class TestProfileIOData : public ProfileIOData {
 public:
  TestProfileIOData(bool is_incognito, PrefService* pref_service,
                    PrefService* local_state, CookieSettings* cookie_settings)
      : ProfileIOData(is_incognito) {
    // Initialize the IO members required for these tests, but keep them on
    // this thread since we don't use a background thread here.
    google_services_username()->Init(prefs::kGoogleServicesUsername,
                                     pref_service);
    reverse_autologin_enabled()->Init(prefs::kReverseAutologinEnabled,
                                      pref_service);
    one_click_signin_rejected_email_list()->Init(
        prefs::kReverseAutologinRejectedEmailList, pref_service);

    google_services_username_pattern()->Init(
        prefs::kGoogleServicesUsernamePattern, local_state);

    sync_disabled()->Init(prefs::kSyncManaged, pref_service);

    set_signin_names_for_testing(new SigninNamesOnIOThread());
    SetCookieSettingsForTesting(cookie_settings);
  }

  virtual ~TestProfileIOData() {
    signin_names()->ReleaseResourcesOnUIThread();
  }

  // ProfileIOData overrides:
  virtual void LazyInitializeInternal(
      ProfileParams* profile_params) const OVERRIDE {
    NOTREACHED();
  }
  virtual void InitializeExtensionsRequestContext(
      ProfileParams* profile_params) const OVERRIDE {
    NOTREACHED();
  }
  virtual ChromeURLRequestContext* InitializeAppRequestContext(
      ChromeURLRequestContext* main_context,
      const StoragePartitionDescriptor& details,
      scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
          protocol_handler_interceptor) const OVERRIDE {
    NOTREACHED();
    return NULL;
  }
  virtual ChromeURLRequestContext* InitializeMediaRequestContext(
      ChromeURLRequestContext* original_context,
      const StoragePartitionDescriptor& details) const OVERRIDE {
    NOTREACHED();
    return NULL;
  }
  virtual ChromeURLRequestContext*
      AcquireMediaRequestContext() const OVERRIDE {
    NOTREACHED();
    return NULL;
  }
  virtual ChromeURLRequestContext*
      AcquireIsolatedAppRequestContext(
          ChromeURLRequestContext* main_context,
          const StoragePartitionDescriptor& partition_descriptor,
          scoped_ptr<ProtocolHandlerRegistry::JobInterceptorFactory>
              protocol_handler_interceptor) const OVERRIDE {
    NOTREACHED();
    return NULL;
  }
  virtual ChromeURLRequestContext*
      AcquireIsolatedMediaRequestContext(
          ChromeURLRequestContext* app_context,
          const StoragePartitionDescriptor& partition_descriptor)
          const OVERRIDE {
    NOTREACHED();
    return NULL;
  }
  virtual chrome_browser_net::LoadTimeStats* GetLoadTimeStats(
      IOThread::Globals* io_thread_globals) const OVERRIDE {
    NOTREACHED();
    return NULL;
  }
};

class TestURLRequest : public base::SupportsUserData {
public:
  TestURLRequest() {}
  virtual ~TestURLRequest() {}
};

class OneClickTestProfileSyncService : public TestProfileSyncService {
  public:
   virtual ~OneClickTestProfileSyncService() {}

   // Helper routine to be used in conjunction with
   // ProfileKeyedServiceFactory::SetTestingFactory().
   static ProfileKeyedService* Build(Profile* profile) {
     return new OneClickTestProfileSyncService(profile);
   }

   // Need to control this for certain tests.
   virtual bool FirstSetupInProgress() const OVERRIDE {
     return first_setup_in_progress_;
   }

   // Controls return value of FirstSetupInProgress. Because some bits
   // of UI depend on that value, it's useful to control it separately
   // from the internal work and components that are triggered (such as
   // ReconfigureDataTypeManager) to facilitate unit tests.
   void set_first_setup_in_progress(bool in_progress) {
     first_setup_in_progress_ = in_progress;
   }

   // Override ProfileSyncService::Shutdown() to avoid CHECK on
   // |invalidator_registrar_|.
   void Shutdown() OVERRIDE {};

  private:
   explicit OneClickTestProfileSyncService(Profile* profile)
       : TestProfileSyncService(NULL,
                                profile,
                                NULL,
                                ProfileSyncService::MANUAL_START,
                                false),  // synchronous_backend_init
         first_setup_in_progress_(false) {}

   bool first_setup_in_progress_;
};

static ProfileKeyedService* BuildSigninManagerMock(Profile* profile) {
  return new SigninManagerMock(profile);
}

}  // namespace

class OneClickSigninHelperTest : public content::RenderViewHostTestHarness {
 public:
  OneClickSigninHelperTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Creates the sign-in manager for tests.  If |use_incognito| is true then
  // a WebContents for an incognito profile is created.  If |username| is
  // is not empty, the profile of the mock WebContents will be connected to
  // the given account.
  void CreateSigninManager(bool use_incognito, const std::string& username);

  void AddEmailToOneClickRejectedList(const std::string& email);
  void EnableOneClick(bool enable);
  void AllowSigninCookies(bool enable);
  void SetAllowedUsernamePattern(const std::string& pattern);

  SigninManagerMock* signin_manager_;

 protected:
  TestingProfile* profile_;

 private:
  // Members to fake that we are on the UI thread.
  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninHelperTest);
};

OneClickSigninHelperTest::OneClickSigninHelperTest()
    : profile_(NULL),
      ui_thread_(content::BrowserThread::UI, &message_loop_) {
}

void OneClickSigninHelperTest::SetUp() {
  SyncPromoUI::ForceWebBasedSigninFlowForTesting(true);
  profile_ = new TestingProfile();
  browser_context_.reset(profile_);
  content::RenderViewHostTestHarness::SetUp();
}

void OneClickSigninHelperTest::TearDown() {
  SyncPromoUI::ForceWebBasedSigninFlowForTesting(false);
  content::RenderViewHostTestHarness::TearDown();
}

void OneClickSigninHelperTest::CreateSigninManager(
    bool use_incognito,
    const std::string& username) {
  profile_->set_incognito(use_incognito);
  signin_manager_ = static_cast<SigninManagerMock*>(
      SigninManagerFactory::GetInstance()->SetTestingFactoryAndUse(
          profile_, BuildSigninManagerMock));

  if (!username.empty()) {
    signin_manager_->StartSignIn(username, std::string(), std::string(),
                                std::string());
  }
}

void OneClickSigninHelperTest::EnableOneClick(bool enable) {
  PrefService* pref_service = Profile::FromBrowserContext(
      browser_context_.get())->GetPrefs();
  pref_service->SetBoolean(prefs::kReverseAutologinEnabled, enable);
}

void OneClickSigninHelperTest::AddEmailToOneClickRejectedList(
    const std::string& email) {
  PrefService* pref_service = Profile::FromBrowserContext(
      browser_context_.get())->GetPrefs();
  ListPrefUpdate updater(pref_service,
                         prefs::kReverseAutologinRejectedEmailList);
  updater->AppendIfNotPresent(new base::StringValue(email));
}

void OneClickSigninHelperTest::AllowSigninCookies(bool enable) {
  CookieSettings* cookie_settings =
      CookieSettings::Factory::GetForProfile(
          Profile::FromBrowserContext(browser_context_.get()));
  cookie_settings->SetDefaultCookieSetting(
      enable ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
}

void OneClickSigninHelperTest::SetAllowedUsernamePattern(
    const std::string& pattern) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kGoogleServicesUsernamePattern, pattern);
}

class OneClickSigninHelperIOTest : public OneClickSigninHelperTest {
 public:
  OneClickSigninHelperIOTest();
  virtual ~OneClickSigninHelperIOTest();

  virtual void SetUp() OVERRIDE;

  TestProfileIOData* CreateTestProfileIOData(bool is_incognito);

 protected:
  TestingProfileManager testing_profile_manager_;
  TestURLRequest request_;
  const GURL valid_gaia_url_;

 private:
  content::TestBrowserThread db_thread_;
  content::TestBrowserThread fub_thread_;
  content::TestBrowserThread io_thread_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninHelperIOTest);
};

OneClickSigninHelperIOTest::OneClickSigninHelperIOTest()
    : testing_profile_manager_(
          TestingBrowserProcess::GetGlobal()),
      valid_gaia_url_("https://accounts.google.com/"),
      db_thread_(content::BrowserThread::DB, &message_loop_),
      fub_thread_(content::BrowserThread::FILE_USER_BLOCKING, &message_loop_),
      io_thread_(content::BrowserThread::IO, &message_loop_) {
}

OneClickSigninHelperIOTest::~OneClickSigninHelperIOTest() {
}

void OneClickSigninHelperIOTest::SetUp() {
  OneClickSigninHelperTest::SetUp();
  ASSERT_TRUE(testing_profile_manager_.SetUp());
  OneClickSigninHelper::AssociateWithRequestForTesting(&request_,
                                                       "user@gmail.com");
}

TestProfileIOData* OneClickSigninHelperIOTest::CreateTestProfileIOData(
    bool is_incognito) {
  PrefService* pref_service = profile_->GetPrefs();
  PrefService* local_state = g_browser_process->local_state();
  CookieSettings* cookie_settings =
      CookieSettings::Factory::GetForProfile(profile_);
  TestProfileIOData* io_data = new TestProfileIOData(
      is_incognito, pref_service, local_state, cookie_settings);
  return io_data;
}

TEST_F(OneClickSigninHelperTest, CanOfferNoContents) {
  int error_message_id = 0;
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      NULL, OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "user@gmail.com", &error_message_id));
  EXPECT_EQ(0, error_message_id);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      NULL, OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "user@gmail.com", &error_message_id));
  EXPECT_EQ(0, error_message_id);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      NULL, OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "", &error_message_id));
  EXPECT_EQ(0, error_message_id);
}

TEST_F(OneClickSigninHelperTest, CanOffer) {
  CreateSigninManager(false, "");

  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_)).
        WillRepeatedly(Return(true));

  EnableOneClick(true);
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "user@gmail.com", NULL));
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "user@gmail.com", NULL));
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
                  web_contents(),
                  OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
                  "", NULL));

  EnableOneClick(false);

  int error_message_id = 0;
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "user@gmail.com", &error_message_id));
  EXPECT_EQ(0, error_message_id);

  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "user@gmail.com", &error_message_id));
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
                  web_contents(),
                  OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
                  "", &error_message_id));
  EXPECT_EQ(0, error_message_id);
}

TEST_F(OneClickSigninHelperTest, CanOfferFirstSetup) {
  CreateSigninManager(false, "");

  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_)).
        WillRepeatedly(Return(true));

  // Invoke OneClickTestProfileSyncService factory function and grab result.
  OneClickTestProfileSyncService* sync =
      static_cast<OneClickTestProfileSyncService*>(
          ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              static_cast<Profile*>(browser_context()),
              OneClickTestProfileSyncService::Build));

  sync->set_first_setup_in_progress(true);

  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(),
      OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "foo@gmail.com", NULL));
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(),
      OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "foo@gmail.com", NULL));
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "", NULL));
}

TEST_F(OneClickSigninHelperTest, CanOfferProfileConnected) {
  CreateSigninManager(false, "foo@gmail.com");

  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_)).
      WillRepeatedly(Return(true));

  int error_message_id = 0;
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "foo@gmail.com", &error_message_id));
  EXPECT_EQ(IDS_SYNC_SETUP_ERROR, error_message_id);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "user@gmail.com", &error_message_id));
  EXPECT_EQ(IDS_SYNC_SETUP_ERROR, error_message_id);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "foo@gmail.com", &error_message_id));
  EXPECT_EQ(IDS_SYNC_SETUP_ERROR, error_message_id);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "user@gmail.com", &error_message_id));
  EXPECT_EQ(IDS_SYNC_SETUP_ERROR, error_message_id);
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "", &error_message_id));
}

TEST_F(OneClickSigninHelperTest, CanOfferUsernameNotAllowed) {
  CreateSigninManager(false, "");

  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_)).
      WillRepeatedly(Return(false));

  int error_message_id = 0;
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "foo@gmail.com", &error_message_id));
  EXPECT_EQ(IDS_SYNC_LOGIN_NAME_PROHIBITED, error_message_id);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "foo@gmail.com", &error_message_id));
  EXPECT_EQ(IDS_SYNC_LOGIN_NAME_PROHIBITED, error_message_id);
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "", &error_message_id));
}

TEST_F(OneClickSigninHelperTest, CanOfferWithRejectedEmail) {
  CreateSigninManager(false, "");

  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_)).
        WillRepeatedly(Return(true));

  AddEmailToOneClickRejectedList("foo@gmail.com");
  AddEmailToOneClickRejectedList("user@gmail.com");

  int error_message_id = 0;
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "foo@gmail.com", &error_message_id));
  EXPECT_EQ(0, error_message_id);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "user@gmail.com", &error_message_id));
  EXPECT_EQ(0, error_message_id);
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "foo@gmail.com", &error_message_id));
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "user@gmail.com", &error_message_id));
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "john@gmail.com", &error_message_id));
}

TEST_F(OneClickSigninHelperTest, CanOfferIncognito) {
  CreateSigninManager(true, "");

  int error_message_id = 0;
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "user@gmail.com", &error_message_id));
  EXPECT_EQ(0, error_message_id);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "user@gmail.com", &error_message_id));
  EXPECT_EQ(0, error_message_id);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "", &error_message_id));
  EXPECT_EQ(0, error_message_id);
}

TEST_F(OneClickSigninHelperTest, CanOfferNoSigninCookies) {
  CreateSigninManager(false, "");
  AllowSigninCookies(false);

  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_)).
        WillRepeatedly(Return(true));

  int error_message_id = 0;
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "user@gmail.com", &error_message_id));
  EXPECT_EQ(0, error_message_id);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "user@gmail.com", &error_message_id));
  EXPECT_EQ(0, error_message_id);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "", &error_message_id));
  EXPECT_EQ(0, error_message_id);
}

TEST_F(OneClickSigninHelperTest, CanOfferDisabledByPolicy) {
  CreateSigninManager(false, "");

  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_)).
        WillRepeatedly(Return(true));

  EnableOneClick(true);
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "user@gmail.com", NULL));

  // Simulate a policy disabling sync by writing kSyncManaged directly.
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kSyncManaged, base::Value::CreateBooleanValue(true));

  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "user@gmail.com", NULL));
}

// Should not crash if a helper instance is not associated with an incognito
// web contents.
TEST_F(OneClickSigninHelperTest, ShowInfoBarUIThreadIncognito) {
  CreateSigninManager(true, "");
  OneClickSigninHelper* helper =
      OneClickSigninHelper::FromWebContents(web_contents());
  EXPECT_EQ(NULL, helper);

  OneClickSigninHelper::ShowInfoBarUIThread(
      "session_index", "email", OneClickSigninHelper::AUTO_ACCEPT_ACCEPTED,
      SyncPromoUI::SOURCE_UNKNOWN, GURL(), process()->GetID(),
      rvh()->GetRoutingID());
}

// I/O thread tests

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThread) {
  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  EXPECT_EQ(OneClickSigninHelper::CAN_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, "", &request_, io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadIncognito) {
  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(true));
  EXPECT_EQ(OneClickSigninHelper::DONT_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, "", &request_, io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadNoIOData) {
  EXPECT_EQ(OneClickSigninHelper::DONT_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, "", &request_, NULL));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadBadURL) {
  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  EXPECT_EQ(OneClickSigninHelper::IGNORE_REQUEST,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                GURL("https://foo.com/"), "", &request_, io_data.get()));
  EXPECT_EQ(OneClickSigninHelper::IGNORE_REQUEST,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                GURL("http://accounts.google.com/"), "",
                &request_, io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadReferrer) {
  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  std::string continue_url(SyncPromoUI::GetSyncPromoURL(
      GURL(), SyncPromoUI::SOURCE_START_PAGE, false).spec());

  EXPECT_EQ(OneClickSigninHelper::CAN_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, continue_url, &request_, io_data.get()));

  EXPECT_EQ(OneClickSigninHelper::CAN_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, kImplicitURLString, &request_, io_data.get()));

  std::string bad_url_1 = continue_url;
  const std::string service_name = "chromiumsync";
  bad_url_1.replace(bad_url_1.find(service_name), service_name.length(),
                    "foo");

  EXPECT_EQ(OneClickSigninHelper::CAN_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, bad_url_1, &request_, io_data.get()));

  std::string bad_url_2 = continue_url;
  const std::string source_num = "%3D0";
  bad_url_2.replace(bad_url_1.find(source_num), source_num.length(), "%3D10");

  EXPECT_EQ(OneClickSigninHelper::CAN_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, bad_url_2, &request_, io_data.get()));

  std::string bad_url_3 = continue_url;
  const std::string source = "source%3D0";
  bad_url_3.erase(bad_url_1.find(source), source.length());

  EXPECT_EQ(OneClickSigninHelper::CAN_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, bad_url_3, &request_, io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadDisabled) {
  EnableOneClick(false);
  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  EXPECT_EQ(OneClickSigninHelper::DONT_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, "", &request_, io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadSignedIn) {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetString(prefs::kGoogleServicesUsername, "user@gmail.com");

  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  EXPECT_EQ(OneClickSigninHelper::DONT_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, "", &request_, io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadEmailNotAllowed) {
  SetAllowedUsernamePattern("*@example.com");
  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  EXPECT_EQ(OneClickSigninHelper::DONT_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, "", &request_, io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadEmailAlreadyUsed) {
  ProfileInfoCache* cache = testing_profile_manager_.profile_info_cache();
  const FilePath& user_data_dir = cache->GetUserDataDir();
  cache->AddProfileToCache(user_data_dir.Append(FILE_PATH_LITERAL("user")),
                           UTF8ToUTF16("user"),
                           UTF8ToUTF16("user@gmail.com"), 0, false);

  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  EXPECT_EQ(OneClickSigninHelper::DONT_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, "", &request_, io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadWithRejectedEmail) {
  AddEmailToOneClickRejectedList("user@gmail.com");
  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  EXPECT_EQ(OneClickSigninHelper::DONT_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, "", &request_, io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadNoSigninCookies) {
  AllowSigninCookies(false);
  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  EXPECT_EQ(OneClickSigninHelper::DONT_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, "", &request_, io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadDisabledByPolicy) {
  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  EXPECT_EQ(OneClickSigninHelper::CAN_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, "", &request_, io_data.get()));

  // Simulate a policy disabling sync by writing kSyncManaged directly.
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kSyncManaged, base::Value::CreateBooleanValue(true));
  EXPECT_EQ(OneClickSigninHelper::DONT_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, "", &request_, io_data.get()));
}
