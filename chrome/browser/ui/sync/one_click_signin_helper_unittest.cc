// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_names_io_thread.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/autofill/core/common/password_form.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/mock_render_process_host.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::_;
using ::testing::AtLeast;
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
  explicit SigninManagerMock(Profile* profile) : FakeSigninManager(profile) {
    Initialize(profile, NULL);
  }
  MOCK_CONST_METHOD1(IsAllowedUsername, bool(const std::string& username));
};

static BrowserContextKeyedService* BuildSigninManagerMock(
    content::BrowserContext* profile) {
  return new SigninManagerMock(static_cast<Profile*>(profile));
}

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

    signin_allowed()->Init(prefs::kSigninAllowed, pref_service);

    set_signin_names_for_testing(new SigninNamesOnIOThread());
    SetCookieSettingsForTesting(cookie_settings);
  }

  virtual ~TestProfileIOData() {
    signin_names()->ReleaseResourcesOnUIThread();
  }

  // ProfileIOData overrides:
  virtual void InitializeInternal(
      ProfileParams* profile_params,
      content::ProtocolHandlerMap* protocol_handlers) const OVERRIDE {
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
          protocol_handler_interceptor,
      content::ProtocolHandlerMap* protocol_handlers) const OVERRIDE {
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
              protocol_handler_interceptor,
          content::ProtocolHandlerMap* protocol_handlers) const OVERRIDE {
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
   // BrowserContextKeyedServiceFactory::SetTestingFactory().
   static BrowserContextKeyedService* Build(content::BrowserContext* profile) {
     return new OneClickTestProfileSyncService(static_cast<Profile*>(profile));
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
   virtual void Shutdown() OVERRIDE {};

  private:
   explicit OneClickTestProfileSyncService(Profile* profile)
       : TestProfileSyncService(NULL,
                                profile,
                                NULL,
                                NULL,
                                ProfileSyncService::MANUAL_START,
                                false),  // synchronous_backend_init
         first_setup_in_progress_(false) {}

   bool first_setup_in_progress_;
};

}  // namespace

class OneClickSigninHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  OneClickSigninHelperTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Creates the sign-in manager for tests.  If |username| is
  // is not empty, the profile of the mock WebContents will be connected to
  // the given account.
  void CreateSigninManager(const std::string& username);

  // Set the ID of the signin process that the test will assume to be the
  // only process allowed to sign the user in to Chrome.
  void SetTrustedSigninProcessID(int id);

  void AddEmailToOneClickRejectedList(const std::string& email);
  void EnableOneClick(bool enable);
  void AllowSigninCookies(bool enable);
  void SetAllowedUsernamePattern(const std::string& pattern);
  ProfileSyncServiceMock* CreateProfileSyncServiceMock();
  void SubmitGAIAPassword(OneClickSigninHelper* helper);

  SigninManagerMock* signin_manager_;
  FakeProfileOAuth2TokenService* fake_oauth2_token_service_;

 protected:
  GoogleServiceAuthError no_error_;

 private:
  // ChromeRenderViewHostTestHarness overrides:
  virtual content::BrowserContext* CreateBrowserContext() OVERRIDE;

  // The ID of the signin process the test will assume to be trusted.
  // By default, set to the test RenderProcessHost's process ID, but
  // overridden by SetTrustedSigninProcessID.
  int trusted_signin_process_id_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninHelperTest);
};

OneClickSigninHelperTest::OneClickSigninHelperTest()
    : signin_manager_(NULL),
      fake_oauth2_token_service_(NULL),
      no_error_(GoogleServiceAuthError::NONE),
      trusted_signin_process_id_(-1) {
}

void OneClickSigninHelperTest::SetUp() {
  signin::ForceWebBasedSigninFlowForTesting(true);
  content::RenderViewHostTestHarness::SetUp();
  SetTrustedSigninProcessID(process()->GetID());
}

void OneClickSigninHelperTest::TearDown() {
  signin::ForceWebBasedSigninFlowForTesting(false);
  content::RenderViewHostTestHarness::TearDown();
}

void OneClickSigninHelperTest::SetTrustedSigninProcessID(int id) {
  trusted_signin_process_id_ = id;
}

void OneClickSigninHelperTest::CreateSigninManager(
    const std::string& username) {
  signin_manager_ = static_cast<SigninManagerMock*>(
      SigninManagerFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), BuildSigninManagerMock));
  if (signin_manager_)
    signin_manager_->SetSigninProcess(trusted_signin_process_id_);

  if (!username.empty()) {
    ASSERT_TRUE(signin_manager_);
    signin_manager_->SetAuthenticatedUsername(username);
  }
}

void OneClickSigninHelperTest::EnableOneClick(bool enable) {
  PrefService* pref_service = profile()->GetPrefs();
  pref_service->SetBoolean(prefs::kReverseAutologinEnabled, enable);
}

void OneClickSigninHelperTest::AddEmailToOneClickRejectedList(
    const std::string& email) {
  PrefService* pref_service = profile()->GetPrefs();
  ListPrefUpdate updater(pref_service,
                         prefs::kReverseAutologinRejectedEmailList);
  updater->AppendIfNotPresent(new base::StringValue(email));
}

void OneClickSigninHelperTest::AllowSigninCookies(bool enable) {
  CookieSettings* cookie_settings =
      CookieSettings::Factory::GetForProfile(profile()).get();
  cookie_settings->SetDefaultCookieSetting(enable ? CONTENT_SETTING_ALLOW
                                                  : CONTENT_SETTING_BLOCK);
}

void OneClickSigninHelperTest::SetAllowedUsernamePattern(
    const std::string& pattern) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kGoogleServicesUsernamePattern, pattern);
}

ProfileSyncServiceMock*
OneClickSigninHelperTest::CreateProfileSyncServiceMock() {
  ProfileSyncServiceMock* sync_service = static_cast<ProfileSyncServiceMock*>(
      ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(),
          ProfileSyncServiceMock::BuildMockProfileSyncService));
  EXPECT_CALL(*sync_service, FirstSetupInProgress()).WillRepeatedly(
      Return(false));
  EXPECT_CALL(*sync_service, sync_initialized()).WillRepeatedly(Return(true));
  EXPECT_CALL(*sync_service, GetAuthError()).
      WillRepeatedly(::testing::ReturnRef(no_error_));
  EXPECT_CALL(*sync_service, sync_initialized()).WillRepeatedly(Return(false));
  sync_service->Initialize();
  return sync_service;
}

void OneClickSigninHelperTest::SubmitGAIAPassword(
    OneClickSigninHelper* helper) {
  autofill::PasswordForm password_form;
  password_form.origin = GURL("https://accounts.google.com");
  password_form.signon_realm = "https://accounts.google.com";
  password_form.password_value = UTF8ToUTF16("password");
  helper->PasswordSubmitted(password_form);
}

content::BrowserContext* OneClickSigninHelperTest::CreateBrowserContext() {
  TestingProfile::Builder builder;
  builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                            FakeProfileOAuth2TokenService::Build);
  scoped_ptr<TestingProfile> profile = builder.Build();

  fake_oauth2_token_service_ =
      static_cast<FakeProfileOAuth2TokenService*>(
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile.get()));

  return profile.release();
}

class OneClickSigninHelperIOTest : public OneClickSigninHelperTest {
 public:
  OneClickSigninHelperIOTest();

  virtual void SetUp() OVERRIDE;

  TestProfileIOData* CreateTestProfileIOData(bool is_incognito);

 protected:
  TestingProfileManager testing_profile_manager_;
  TestURLRequest request_;
  const GURL valid_gaia_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OneClickSigninHelperIOTest);
};

OneClickSigninHelperIOTest::OneClickSigninHelperIOTest()
    : testing_profile_manager_(
          TestingBrowserProcess::GetGlobal()),
      valid_gaia_url_("https://accounts.google.com/") {
}

void OneClickSigninHelperIOTest::SetUp() {
  OneClickSigninHelperTest::SetUp();
  ASSERT_TRUE(testing_profile_manager_.SetUp());
}

TestProfileIOData* OneClickSigninHelperIOTest::CreateTestProfileIOData(
    bool is_incognito) {
  PrefService* pref_service = profile()->GetPrefs();
  PrefService* local_state = g_browser_process->local_state();
  CookieSettings* cookie_settings =
      CookieSettings::Factory::GetForProfile(profile()).get();
  TestProfileIOData* io_data = new TestProfileIOData(
      is_incognito, pref_service, local_state, cookie_settings);
  io_data->set_reverse_autologin_pending_email("user@gmail.com");
  return io_data;
}

class OneClickSigninHelperIncognitoTest : public OneClickSigninHelperTest {
 protected:
  // content::RenderViewHostTestHarness.
  virtual content::BrowserContext* CreateBrowserContext() OVERRIDE;
};

content::BrowserContext*
OneClickSigninHelperIncognitoTest::CreateBrowserContext() {
  // Builds an incognito profile to run this test.
  TestingProfile::Builder builder;
  builder.SetIncognito();
  return builder.Build().release();
}

TEST_F(OneClickSigninHelperTest, CanOfferNoContents) {
  std::string error_message;
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      NULL, OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "user@gmail.com", &error_message));
  EXPECT_EQ("", error_message);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      NULL, OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "user@gmail.com", &error_message));
  EXPECT_EQ("", error_message);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      NULL,
      OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      std::string(),
      &error_message));
  EXPECT_EQ("", error_message);
}

TEST_F(OneClickSigninHelperTest, CanOffer) {
  CreateSigninManager(std::string());

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
      std::string(),
      NULL));

  EnableOneClick(false);

  std::string error_message;
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "user@gmail.com", &error_message));
  EXPECT_EQ("", error_message);

  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "user@gmail.com", &error_message));
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(),
      OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      std::string(),
      &error_message));
  EXPECT_EQ("", error_message);
}

TEST_F(OneClickSigninHelperTest, CanOfferFirstSetup) {
  CreateSigninManager(std::string());

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
      web_contents(),
      OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      std::string(),
      NULL));
}

TEST_F(OneClickSigninHelperTest, CanOfferProfileConnected) {
  CreateSigninManager("foo@gmail.com");

  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_)).
      WillRepeatedly(Return(true));

  std::string error_message;
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "foo@gmail.com", &error_message));
  EXPECT_EQ("", error_message);
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "foo", &error_message));
  EXPECT_EQ("", error_message);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "user@gmail.com", &error_message));
  EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_SYNC_WRONG_EMAIL,
                                      UTF8ToUTF16("foo@gmail.com")),
            error_message);
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "foo@gmail.com", &error_message));
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "foo", &error_message));
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "user@gmail.com", &error_message));
  EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_SYNC_WRONG_EMAIL,
                                      UTF8ToUTF16("foo@gmail.com")),
            error_message);
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(),
      OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      std::string(),
      &error_message));
}

TEST_F(OneClickSigninHelperTest, CanOfferUsernameNotAllowed) {
  CreateSigninManager(std::string());

  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_)).
      WillRepeatedly(Return(false));

  std::string error_message;
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "foo@gmail.com", &error_message));
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SYNC_LOGIN_NAME_PROHIBITED),
            error_message);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "foo@gmail.com", &error_message));
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SYNC_LOGIN_NAME_PROHIBITED),
            error_message);
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(),
      OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      std::string(),
      &error_message));
}

TEST_F(OneClickSigninHelperTest, CanOfferWithRejectedEmail) {
  CreateSigninManager(std::string());

  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_)).
        WillRepeatedly(Return(true));

  AddEmailToOneClickRejectedList("foo@gmail.com");
  AddEmailToOneClickRejectedList("user@gmail.com");

  std::string error_message;
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "foo@gmail.com", &error_message));
  EXPECT_EQ("", error_message);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "user@gmail.com", &error_message));
  EXPECT_EQ("", error_message);
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "foo@gmail.com", &error_message));
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "user@gmail.com", &error_message));
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "john@gmail.com", &error_message));
}

TEST_F(OneClickSigninHelperIncognitoTest, CanOfferIncognito) {
  CreateSigninManager(std::string());

  std::string error_message;
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "user@gmail.com", &error_message));
  EXPECT_EQ("", error_message);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "user@gmail.com", &error_message));
  EXPECT_EQ("", error_message);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(),
      OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      std::string(),
      &error_message));
  EXPECT_EQ("", error_message);
}

TEST_F(OneClickSigninHelperTest, CanOfferNoSigninCookies) {
  CreateSigninManager(std::string());
  AllowSigninCookies(false);

  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_)).
        WillRepeatedly(Return(true));

  std::string error_message;
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      "user@gmail.com", &error_message));
  EXPECT_EQ("", error_message);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "user@gmail.com", &error_message));
  EXPECT_EQ("", error_message);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(),
      OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
      std::string(),
      &error_message));
  EXPECT_EQ("", error_message);
}

TEST_F(OneClickSigninHelperTest, CanOfferDisabledByPolicy) {
  CreateSigninManager(std::string());

  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_)).
        WillRepeatedly(Return(true));

  EnableOneClick(true);
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "user@gmail.com", NULL));

  // Simulate a policy disabling signin by writing kSigninAllowed directly.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kSigninAllowed, base::Value::CreateBooleanValue(false));

  EXPECT_FALSE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "user@gmail.com", NULL));

  // Reset the preference value to true.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kSigninAllowed, base::Value::CreateBooleanValue(true));

  // Simulate a policy disabling sync by writing kSyncManaged directly.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kSyncManaged, base::Value::CreateBooleanValue(true));

  // Should still offer even if sync is disabled by policy.
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(
      web_contents(), OneClickSigninHelper::CAN_OFFER_FOR_ALL,
      "user@gmail.com", NULL));
}

// Should not crash if a helper instance is not associated with an incognito
// web contents.
TEST_F(OneClickSigninHelperIncognitoTest, ShowInfoBarUIThreadIncognito) {
  CreateSigninManager(std::string());
  OneClickSigninHelper* helper =
      OneClickSigninHelper::FromWebContents(web_contents());
  EXPECT_EQ(NULL, helper);

  OneClickSigninHelper::ShowInfoBarUIThread(
      "session_index", "email", OneClickSigninHelper::AUTO_ACCEPT_ACCEPTED,
      signin::SOURCE_UNKNOWN, GURL(), process()->GetID(),
      rvh()->GetRoutingID());
}

// If Chrome signin is triggered from a webstore install, and user chooses to
// config sync, then Chrome should redirect immediately to sync settings page,
// and upon successful setup, redirect back to webstore.
TEST_F(OneClickSigninHelperTest, SigninFromWebstoreWithConfigSyncfirst) {
  CreateSigninManager(std::string());
  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_))
      .WillRepeatedly(Return(true));

  ProfileSyncServiceMock* sync_service = CreateProfileSyncServiceMock();
  EXPECT_CALL(*sync_service, AddObserver(_)).Times(AtLeast(1));
  EXPECT_CALL(*sync_service, RemoveObserver(_)).Times(AtLeast(1));
  EXPECT_CALL(*sync_service, sync_initialized()).WillRepeatedly(Return(true));

  content::WebContents* contents = web_contents();

  OneClickSigninHelper::CreateForWebContentsWithPasswordManager(contents, NULL);
  OneClickSigninHelper* helper =
      OneClickSigninHelper::FromWebContents(contents);
  helper->SetDoNotClearPendingEmailForTesting();
  helper->set_do_not_start_sync_for_testing();

  GURL continueUrl("https://chrome.google.com/webstore?source=5");
  OneClickSigninHelper::ShowInfoBarUIThread(
      "session_index", "user@gmail.com",
      OneClickSigninHelper::AUTO_ACCEPT_EXPLICIT,
      signin::SOURCE_WEBSTORE_INSTALL,
      continueUrl, process()->GetID(), rvh()->GetRoutingID());

  SubmitGAIAPassword(helper);

  NavigateAndCommit(GURL("https://chrome.google.com/webstore?source=3"));
  helper->DidStopLoading(rvh());
  helper->OnStateChanged();
  EXPECT_EQ(GURL(continueUrl), contents->GetURL());
}

// Checks that the state of OneClickSigninHelper is cleaned when there is a
// navigation away from the sign in flow that is not triggered by the
// web contents.
TEST_F(OneClickSigninHelperTest, CleanTransientStateOnNavigate) {
  content::WebContents* contents = web_contents();

  OneClickSigninHelper::CreateForWebContentsWithPasswordManager(contents, NULL);
  OneClickSigninHelper* helper =
      OneClickSigninHelper::FromWebContents(contents);
  helper->SetDoNotClearPendingEmailForTesting();
  helper->auto_accept_ = OneClickSigninHelper::AUTO_ACCEPT_EXPLICIT;

  content::LoadCommittedDetails details;
  content::FrameNavigateParams params;
  params.url = GURL("http://crbug.com");
  params.transition = content::PAGE_TRANSITION_TYPED;
  helper->DidNavigateMainFrame(details, params);

  EXPECT_EQ(OneClickSigninHelper::AUTO_ACCEPT_NONE, helper->auto_accept_);
}

// Checks that OneClickSigninHelper doesn't stay an observer of the profile
// sync service after it's deleted.
TEST_F(OneClickSigninHelperTest, RemoveObserverFromProfileSyncService) {
  content::WebContents* contents = web_contents();

  ProfileSyncServiceMock* sync_service = CreateProfileSyncServiceMock();

  OneClickSigninHelper::CreateForWebContentsWithPasswordManager(contents, NULL);
  OneClickSigninHelper* helper =
      OneClickSigninHelper::FromWebContents(contents);
  helper->SetDoNotClearPendingEmailForTesting();

  EXPECT_CALL(*sync_service, RemoveObserver(helper));
  SetContents(NULL);
}

// I/O thread tests

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThread) {
  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  EXPECT_EQ(OneClickSigninHelper::CAN_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, std::string(), &request_, io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadIncognito) {
  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(true));
  EXPECT_EQ(OneClickSigninHelper::DONT_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, std::string(), &request_, io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadNoIOData) {
  EXPECT_EQ(OneClickSigninHelper::DONT_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, std::string(), &request_, NULL));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadBadURL) {
  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  EXPECT_EQ(
      OneClickSigninHelper::IGNORE_REQUEST,
      OneClickSigninHelper::CanOfferOnIOThreadImpl(
          GURL("https://foo.com/"), std::string(), &request_, io_data.get()));
  EXPECT_EQ(OneClickSigninHelper::IGNORE_REQUEST,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                GURL("http://accounts.google.com/"),
                std::string(),
                &request_,
                io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadReferrer) {
  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  std::string continue_url(signin::GetPromoURL(
      signin::SOURCE_START_PAGE, false).spec());

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
                valid_gaia_url_, std::string(), &request_, io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadSignedIn) {
  PrefService* pref_service = profile()->GetPrefs();
  pref_service->SetString(prefs::kGoogleServicesUsername, "user@gmail.com");

  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  EXPECT_EQ(OneClickSigninHelper::DONT_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, std::string(), &request_, io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadEmailNotAllowed) {
  SetAllowedUsernamePattern("*@example.com");
  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  EXPECT_EQ(OneClickSigninHelper::DONT_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, std::string(), &request_, io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadEmailAlreadyUsed) {
  ProfileInfoCache* cache = testing_profile_manager_.profile_info_cache();
  const base::FilePath& user_data_dir = cache->GetUserDataDir();
  cache->AddProfileToCache(user_data_dir.Append(FILE_PATH_LITERAL("user")),
                           UTF8ToUTF16("user"),
                           UTF8ToUTF16("user@gmail.com"), 0, std::string());

  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  EXPECT_EQ(OneClickSigninHelper::DONT_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, std::string(), &request_, io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadWithRejectedEmail) {
  AddEmailToOneClickRejectedList("user@gmail.com");
  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  EXPECT_EQ(OneClickSigninHelper::DONT_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, std::string(), &request_, io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadNoSigninCookies) {
  AllowSigninCookies(false);
  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  EXPECT_EQ(OneClickSigninHelper::DONT_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, std::string(), &request_, io_data.get()));
}

TEST_F(OneClickSigninHelperIOTest, CanOfferOnIOThreadDisabledByPolicy) {
  scoped_ptr<TestProfileIOData> io_data(CreateTestProfileIOData(false));
  EXPECT_EQ(OneClickSigninHelper::CAN_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, std::string(), &request_, io_data.get()));

  // Simulate a policy disabling signin by writing kSigninAllowed directly.
  // We should not offer to sign in the browser.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kSigninAllowed, base::Value::CreateBooleanValue(false));
  EXPECT_EQ(OneClickSigninHelper::DONT_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, std::string(), &request_, io_data.get()));

  // Reset the preference.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kSigninAllowed, base::Value::CreateBooleanValue(true));

  // Simulate a policy disabling sync by writing kSyncManaged directly.
  // We should still offer to sign in the browser.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kSyncManaged, base::Value::CreateBooleanValue(true));
  EXPECT_EQ(OneClickSigninHelper::CAN_OFFER,
            OneClickSigninHelper::CanOfferOnIOThreadImpl(
                valid_gaia_url_, std::string(), &request_, io_data.get()));
}
