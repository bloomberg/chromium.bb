// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/storage/durable_storage_permission_context.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/domain_reliability/clear_mode.h"
#include "components/domain_reliability/monitor.h"
#include "components/domain_reliability/service.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/ntp_snippets/bookmarks/bookmark_last_visit_utils.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_transaction_factory.h"
#include "net/reporting/reporting_browsing_data_remover.h"
#include "net/reporting/reporting_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/favicon_size.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/webapps/webapp_registry.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "components/signin/core/account_id/account_id.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/mock_extension_special_storage_policy.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/browsing_data/mock_browsing_data_flash_lso_helper.h"
#endif

using content::BrowsingDataFilterBuilder;
using domain_reliability::CLEAR_BEACONS;
using domain_reliability::CLEAR_CONTEXTS;
using domain_reliability::DomainReliabilityClearMode;
using domain_reliability::DomainReliabilityMonitor;
using domain_reliability::DomainReliabilityService;
using domain_reliability::DomainReliabilityServiceFactory;
using testing::_;
using testing::ByRef;
using testing::Eq;
using testing::FloatEq;
using testing::Invoke;
using testing::IsEmpty;
using testing::Matcher;
using testing::MakeMatcher;
using testing::MatcherInterface;
using testing::MatchResultListener;
using testing::Not;
using testing::Return;
using testing::SizeIs;
using testing::WithArgs;

namespace {

const char kTestOrigin1[] = "http://host1.com:1/";
const char kTestRegisterableDomain1[] = "host1.com";
const char kTestOrigin2[] = "http://host2.com:1/";
const char kTestOrigin3[] = "http://host3.com:1/";
const char kTestRegisterableDomain3[] = "host3.com";
const char kTestOrigin4[] = "https://host3.com:1/";
const char kTestOriginExt[] = "chrome-extension://abcdefghijklmnopqrstuvwxyz/";
const char kTestOriginDevTools[] = "chrome-devtools://abcdefghijklmnopqrstuvw/";

// For HTTP auth.
const char kTestRealm[] = "TestRealm";

// For Autofill.
const char kWebOrigin[] = "https://www.example.com/";

const GURL kOrigin1(kTestOrigin1);
const GURL kOrigin2(kTestOrigin2);
const GURL kOrigin3(kTestOrigin3);
const GURL kOrigin4(kTestOrigin4);

const GURL kOriginExt(kTestOriginExt);
const GURL kOriginDevTools(kTestOriginDevTools);

// Shorthands for origin types.
#if BUILDFLAG(ENABLE_EXTENSIONS)
const int kExtension = ChromeBrowsingDataRemoverDelegate::ORIGIN_TYPE_EXTENSION;
#endif
const int kProtected = content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;
const int kUnprotected =
    content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;

// Testers --------------------------------------------------------------------

#if defined(OS_ANDROID)
class TestWebappRegistry : public WebappRegistry {
 public:
  TestWebappRegistry() : WebappRegistry() { }

  void UnregisterWebappsForUrls(
      const base::Callback<bool(const GURL&)>& url_filter) override {
    // Mocks out a JNI call.
  }

  void ClearWebappHistoryForUrls(
      const base::Callback<bool(const GURL&)>& url_filter) override {
    // Mocks out a JNI call.
  }
};
#endif

#if defined(OS_CHROMEOS)
// Customized fake class to count TpmAttestationDeleteKeys call.
class FakeCryptohomeClient : public chromeos::FakeCryptohomeClient {
 public:
  void TpmAttestationDeleteKeys(
      chromeos::attestation::AttestationKeyType key_type,
      const cryptohome::Identification& cryptohome_id,
      const std::string& key_prefix,
      const chromeos::BoolDBusMethodCallback& callback) override {
    ++delete_keys_call_count_;
    chromeos::FakeCryptohomeClient::TpmAttestationDeleteKeys(
        key_type, cryptohome_id, key_prefix, callback);
  }

  int delete_keys_call_count() const { return delete_keys_call_count_; }

 private:
  int delete_keys_call_count_ = 0;
};
#endif

class RemoveCookieTester {
 public:
  RemoveCookieTester() {}

  // Returns true, if the given cookie exists in the cookie store.
  bool ContainsCookie() {
    scoped_refptr<content::MessageLoopRunner> message_loop_runner =
        new content::MessageLoopRunner;
    quit_closure_ = message_loop_runner->QuitClosure();
    get_cookie_success_ = false;
    cookie_store_->GetCookiesWithOptionsAsync(
        kOrigin1, net::CookieOptions(),
        base::BindOnce(&RemoveCookieTester::GetCookieCallback,
                       base::Unretained(this)));
    message_loop_runner->Run();
    return get_cookie_success_;
  }

  void AddCookie() {
    scoped_refptr<content::MessageLoopRunner> message_loop_runner =
        new content::MessageLoopRunner;
    quit_closure_ = message_loop_runner->QuitClosure();
    cookie_store_->SetCookieWithOptionsAsync(
        kOrigin1, "A=1", net::CookieOptions(),
        base::BindOnce(&RemoveCookieTester::SetCookieCallback,
                       base::Unretained(this)));
    message_loop_runner->Run();
  }

 protected:
  void SetCookieStore(net::CookieStore* cookie_store) {
    cookie_store_ = cookie_store;
  }

 private:
  void GetCookieCallback(const std::string& cookies) {
    if (cookies == "A=1") {
      get_cookie_success_ = true;
    } else {
      EXPECT_EQ("", cookies);
      get_cookie_success_ = false;
    }
    quit_closure_.Run();
  }

  void SetCookieCallback(bool result) {
    ASSERT_TRUE(result);
    quit_closure_.Run();
  }

  bool get_cookie_success_ = false;
  base::Closure quit_closure_;

  // CookieStore must out live |this|.
  net::CookieStore* cookie_store_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RemoveCookieTester);
};

void RunClosureAfterCookiesCleared(const base::Closure& task,
                                   uint32_t cookies_deleted) {
  task.Run();
}

class RemoveSafeBrowsingCookieTester : public RemoveCookieTester {
 public:
  RemoveSafeBrowsingCookieTester()
      : browser_process_(TestingBrowserProcess::GetGlobal()) {
    scoped_refptr<safe_browsing::SafeBrowsingService> sb_service =
        safe_browsing::SafeBrowsingService::CreateSafeBrowsingService();
    browser_process_->SetSafeBrowsingService(sb_service.get());
    sb_service->Initialize();
    base::RunLoop().RunUntilIdle();

    // Make sure the safe browsing cookie store has no cookies.
    // TODO(mmenke): Is this really needed?
    base::RunLoop run_loop;
    net::URLRequestContext* request_context =
        sb_service->url_request_context()->GetURLRequestContext();
    request_context->cookie_store()->DeleteAllAsync(
        base::BindOnce(&RunClosureAfterCookiesCleared, run_loop.QuitClosure()));
    run_loop.Run();

    SetCookieStore(request_context->cookie_store());
  }

  virtual ~RemoveSafeBrowsingCookieTester() {
    browser_process_->safe_browsing_service()->ShutDown();
    base::RunLoop().RunUntilIdle();
    browser_process_->SetSafeBrowsingService(nullptr);
  }

 private:
  TestingBrowserProcess* browser_process_;

  DISALLOW_COPY_AND_ASSIGN(RemoveSafeBrowsingCookieTester);
};

class RemoveHistoryTester {
 public:
  RemoveHistoryTester() {}

  bool Init(TestingProfile* profile) WARN_UNUSED_RESULT {
    if (!profile->CreateHistoryService(true, false))
      return false;
    history_service_ = HistoryServiceFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    return true;
  }

  // Returns true, if the given URL exists in the history service.
  bool HistoryContainsURL(const GURL& url) {
    scoped_refptr<content::MessageLoopRunner> message_loop_runner =
        new content::MessageLoopRunner;
    quit_closure_ = message_loop_runner->QuitClosure();
    history_service_->QueryURL(
        url,
        true,
        base::Bind(&RemoveHistoryTester::SaveResultAndQuit,
                   base::Unretained(this)),
        &tracker_);
    message_loop_runner->Run();
    return query_url_success_;
  }

  void AddHistory(const GURL& url, base::Time time) {
    history_service_->AddPage(url, time, nullptr, 0, GURL(),
                              history::RedirectList(), ui::PAGE_TRANSITION_LINK,
                              history::SOURCE_BROWSED, false);
  }

 private:
  // Callback for HistoryService::QueryURL.
  void SaveResultAndQuit(bool success,
                         const history::URLRow&,
                         const history::VisitVector&) {
    query_url_success_ = success;
    quit_closure_.Run();
  }

  // For History requests.
  base::CancelableTaskTracker tracker_;
  bool query_url_success_ = false;
  base::Closure quit_closure_;

  // TestingProfile owns the history service; we shouldn't delete it.
  history::HistoryService* history_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RemoveHistoryTester);
};

class RemoveFaviconTester {
 public:
  RemoveFaviconTester() {}

  bool Init(TestingProfile* profile) WARN_UNUSED_RESULT {
    // Create the history service if it has not been created yet.
    history_service_ = HistoryServiceFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    if (!history_service_) {
      if (!profile->CreateHistoryService(true, false))
        return false;
      history_service_ = HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
    }

    profile->CreateFaviconService();
    favicon_service_ = FaviconServiceFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    return true;
  }

  // Returns true if there is a favicon stored for |page_url| in the favicon
  // database.
  bool HasFaviconForPageURL(const GURL& page_url) {
    RequestFaviconSyncForPageURL(page_url);
    return got_favicon_;
  }

  // Returns true if:
  // - There is a favicon stored for |page_url| in the favicon database.
  // - The stored favicon is expired.
  bool HasExpiredFaviconForPageURL(const GURL& page_url) {
    RequestFaviconSyncForPageURL(page_url);
    return got_expired_favicon_;
  }

  // Adds a visit to history and stores an arbitrary favicon bitmap for
  // |page_url|.
  void VisitAndAddFavicon(const GURL& page_url) {
    history_service_->AddPage(page_url, base::Time::Now(), nullptr, 0, GURL(),
        history::RedirectList(), ui::PAGE_TRANSITION_LINK,
        history::SOURCE_BROWSED, false);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(gfx::kFaviconSize, gfx::kFaviconSize);
    bitmap.eraseColor(SK_ColorBLUE);
    favicon_service_->SetFavicons(page_url, page_url, favicon_base::FAVICON,
                                  gfx::Image::CreateFrom1xBitmap(bitmap));
  }

 private:
  // Synchronously requests the favicon for |page_url| from the favicon
  // database.
  void RequestFaviconSyncForPageURL(const GURL& page_url) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    favicon_service_->GetRawFaviconForPageURL(
        page_url,
        favicon_base::FAVICON,
        gfx::kFaviconSize,
        base::Bind(&RemoveFaviconTester::SaveResultAndQuit,
                   base::Unretained(this)),
        &tracker_);
    run_loop.Run();
  }

  // Callback for HistoryService::QueryURL.
  void SaveResultAndQuit(const favicon_base::FaviconRawBitmapResult& result) {
    got_favicon_ = result.is_valid();
    got_expired_favicon_ = result.is_valid() && result.expired;
    quit_closure_.Run();
  }

  // For favicon requests.
  base::CancelableTaskTracker tracker_;
  bool got_favicon_ = false;
  bool got_expired_favicon_ = false;
  base::Closure quit_closure_;

  // Owned by TestingProfile.
  history::HistoryService* history_service_ = nullptr;
  favicon::FaviconService* favicon_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RemoveFaviconTester);
};

class MockDomainReliabilityService : public DomainReliabilityService {
 public:
  MockDomainReliabilityService() {}

  ~MockDomainReliabilityService() override {}

  std::unique_ptr<DomainReliabilityMonitor> CreateMonitor(
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
      override {
    NOTREACHED();
    return std::unique_ptr<DomainReliabilityMonitor>();
  }

  void ClearBrowsingData(
      DomainReliabilityClearMode clear_mode,
      const base::Callback<bool(const GURL&)>& origin_filter,
      const base::Closure& callback) override {
    clear_count_++;
    last_clear_mode_ = clear_mode;
    last_filter_ = origin_filter;
    callback.Run();
  }

  void GetWebUIData(const base::Callback<void(std::unique_ptr<base::Value>)>&
                        callback) const override {
    NOTREACHED();
  }

  void SetDiscardUploadsForTesting(bool discard_uploads) override {
    NOTREACHED();
  }

  void AddContextForTesting(
      std::unique_ptr<const domain_reliability::DomainReliabilityConfig> config)
      override {
    NOTREACHED();
  }

  void ForceUploadsForTesting() override { NOTREACHED(); }

  int clear_count() const { return clear_count_; }

  DomainReliabilityClearMode last_clear_mode() const {
    return last_clear_mode_;
  }

  const base::Callback<bool(const GURL&)>& last_filter() const {
    return last_filter_;
  }

 private:
  unsigned clear_count_ = 0;
  DomainReliabilityClearMode last_clear_mode_;
  base::Callback<bool(const GURL&)> last_filter_;
};

struct TestingDomainReliabilityServiceFactoryUserData
    : public base::SupportsUserData::Data {
  TestingDomainReliabilityServiceFactoryUserData(
      content::BrowserContext* context,
      MockDomainReliabilityService* service)
      : context(context),
        service(service),
        attached(false) {}
  ~TestingDomainReliabilityServiceFactoryUserData() override {}

  content::BrowserContext* const context;
  MockDomainReliabilityService* const service;
  bool attached;

  static const void* kKey;
};

// static
const void* TestingDomainReliabilityServiceFactoryUserData::kKey =
    &TestingDomainReliabilityServiceFactoryUserData::kKey;

std::unique_ptr<KeyedService> TestingDomainReliabilityServiceFactoryFunction(
    content::BrowserContext* context) {
  const void* kKey = TestingDomainReliabilityServiceFactoryUserData::kKey;

  TestingDomainReliabilityServiceFactoryUserData* data =
      static_cast<TestingDomainReliabilityServiceFactoryUserData*>(
          context->GetUserData(kKey));
  EXPECT_TRUE(data);
  EXPECT_EQ(data->context, context);
  EXPECT_FALSE(data->attached);

  data->attached = true;
  return base::WrapUnique(data->service);
}

class ClearDomainReliabilityTester {
 public:
  explicit ClearDomainReliabilityTester(TestingProfile* profile) :
      profile_(profile),
      mock_service_(new MockDomainReliabilityService()) {
    AttachService();
  }

  unsigned clear_count() const { return mock_service_->clear_count(); }

  DomainReliabilityClearMode last_clear_mode() const {
    return mock_service_->last_clear_mode();
  }

  const base::Callback<bool(const GURL&)>& last_filter() const {
    return mock_service_->last_filter();
  }

 private:
  void AttachService() {
    const void* kKey = TestingDomainReliabilityServiceFactoryUserData::kKey;

    // Attach kludgey UserData struct to profile.
    TestingDomainReliabilityServiceFactoryUserData* data =
        new TestingDomainReliabilityServiceFactoryUserData(profile_,
                                                           mock_service_);
    EXPECT_FALSE(profile_->GetUserData(kKey));
    profile_->SetUserData(kKey, base::WrapUnique(data));

    // Set and use factory that will attach service stuffed in kludgey struct.
    DomainReliabilityServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile_,
        &TestingDomainReliabilityServiceFactoryFunction);

    // Verify and detach kludgey struct.
    EXPECT_EQ(data, profile_->GetUserData(kKey));
    EXPECT_TRUE(data->attached);
    profile_->RemoveUserData(kKey);
  }

  TestingProfile* profile_;
  MockDomainReliabilityService* mock_service_;
};

class RemovePasswordsTester {
 public:
  explicit RemovePasswordsTester(TestingProfile* testing_profile) {
    PasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
        testing_profile,
        password_manager::BuildPasswordStore<
            content::BrowserContext,
            testing::NiceMock<password_manager::MockPasswordStore>>);

    store_ = static_cast<password_manager::MockPasswordStore*>(
        PasswordStoreFactory::GetInstance()
            ->GetForProfile(testing_profile, ServiceAccessType::EXPLICIT_ACCESS)
            .get());

    OSCryptMocker::SetUpWithSingleton();
  }

  ~RemovePasswordsTester() { OSCryptMocker::TearDown(); }

  password_manager::MockPasswordStore* store() { return store_; }

 private:
  password_manager::MockPasswordStore* store_;

  DISALLOW_COPY_AND_ASSIGN(RemovePasswordsTester);
};

class RemovePermissionPromptCountsTest {
 public:
  explicit RemovePermissionPromptCountsTest(TestingProfile* profile)
      : autoblocker_(PermissionDecisionAutoBlocker::GetForProfile(profile)) {}

  int GetDismissCount(const GURL& url, ContentSettingsType permission) {
    return autoblocker_->GetDismissCount(url, permission);
  }

  int GetIgnoreCount(const GURL& url, ContentSettingsType permission) {
    return autoblocker_->GetIgnoreCount(url, permission);
  }

  bool RecordIgnoreAndEmbargo(const GURL& url, ContentSettingsType permission) {
    return autoblocker_->RecordIgnoreAndEmbargo(url, permission);
  }

  bool RecordDismissAndEmbargo(const GURL& url,
                               ContentSettingsType permission) {
    return autoblocker_->RecordDismissAndEmbargo(url, permission);
  }

  void CheckEmbargo(const GURL& url,
                    ContentSettingsType permission,
                    ContentSetting expected_setting) {
    EXPECT_EQ(expected_setting,
              autoblocker_->GetEmbargoResult(url, permission).content_setting);
  }

 private:
  PermissionDecisionAutoBlocker* autoblocker_;

  DISALLOW_COPY_AND_ASSIGN(RemovePermissionPromptCountsTest);
};

#if BUILDFLAG(ENABLE_PLUGINS)
// A small modification to MockBrowsingDataFlashLSOHelper so that it responds
// immediately and does not wait for the Notify() call. Otherwise it would
// deadlock BrowsingDataRemoverImpl::RemoveImpl.
class TestBrowsingDataFlashLSOHelper : public MockBrowsingDataFlashLSOHelper {
 public:
  explicit TestBrowsingDataFlashLSOHelper(TestingProfile* profile)
      : MockBrowsingDataFlashLSOHelper(profile) {}

  void StartFetching(const GetSitesWithFlashDataCallback& callback) override {
    MockBrowsingDataFlashLSOHelper::StartFetching(callback);
    Notify();
  }

 private:
  ~TestBrowsingDataFlashLSOHelper() override {}

  DISALLOW_COPY_AND_ASSIGN(TestBrowsingDataFlashLSOHelper);
};

class RemovePluginDataTester {
 public:
  explicit RemovePluginDataTester(TestingProfile* profile)
      : helper_(new TestBrowsingDataFlashLSOHelper(profile)) {
    static_cast<ChromeBrowsingDataRemoverDelegate*>(
        profile->GetBrowsingDataRemoverDelegate())
        ->OverrideFlashLSOHelperForTesting(helper_);
  }

  void AddDomain(const std::string& domain) {
    helper_->AddFlashLSODomain(domain);
  }

  const std::vector<std::string>& GetDomains() {
    // TestBrowsingDataFlashLSOHelper is synchronous, so we can immediately
    // return the fetched domains.
    helper_->StartFetching(
        base::Bind(&RemovePluginDataTester::OnSitesWithFlashDataFetched,
                   base::Unretained(this)));
    return domains_;
  }

 private:
  void OnSitesWithFlashDataFetched(const std::vector<std::string>& sites) {
    domains_ = sites;
  }

  std::vector<std::string> domains_;
  scoped_refptr<TestBrowsingDataFlashLSOHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(RemovePluginDataTester);
};
#endif

// Custom matcher to test the equivalence of two URL filters. Since those are
// blackbox predicates, we can only approximate the equivalence by testing
// whether the filter give the same answer for several URLs. This is currently
// good enough for our testing purposes, to distinguish whitelists
// and blacklists, empty and non-empty filters and such.
// TODO(msramek): BrowsingDataRemover and some of its backends support URL
// filters, but its constructor currently only takes a single URL and constructs
// its own url filter. If an url filter was directly passed to
// BrowsingDataRemover (what should eventually be the case), we can use the same
// instance in the test as well, and thus simply test base::Callback::Equals()
// in this matcher.
class ProbablySameFilterMatcher
    : public MatcherInterface<const base::Callback<bool(const GURL&)>&> {
 public:
  explicit ProbablySameFilterMatcher(
      const base::Callback<bool(const GURL&)>& filter)
      : to_match_(filter) {
  }

  virtual bool MatchAndExplain(const base::Callback<bool(const GURL&)>& filter,
                               MatchResultListener* listener) const {
    if (filter.is_null() && to_match_.is_null())
      return true;
    if (filter.is_null() != to_match_.is_null())
      return false;

    const GURL urls_to_test_[] =
        {kOrigin1, kOrigin2, kOrigin3, GURL("invalid spec")};
    for (GURL url : urls_to_test_) {
      if (filter.Run(url) != to_match_.Run(url)) {
        if (listener)
          *listener << "The filters differ on the URL " << url;
        return false;
      }
    }
    return true;
  }

  virtual void DescribeTo(::std::ostream* os) const {
    *os << "is probably the same url filter as " << &to_match_;
  }

  virtual void DescribeNegationTo(::std::ostream* os) const {
    *os << "is definitely NOT the same url filter as " << &to_match_;
  }

 private:
  const base::Callback<bool(const GURL&)>& to_match_;
};

inline Matcher<const base::Callback<bool(const GURL&)>&> ProbablySameFilter(
    const base::Callback<bool(const GURL&)>& filter) {
  return MakeMatcher(new ProbablySameFilterMatcher(filter));
}

bool ProbablySameFilters(
    const base::Callback<bool(const GURL&)>& filter1,
    const base::Callback<bool(const GURL&)>& filter2) {
  return ProbablySameFilter(filter1).MatchAndExplain(filter2, nullptr);
}

base::Time AnHourAgo() {
  return base::Time::Now() - base::TimeDelta::FromHours(1);
}

class RemoveDownloadsTester {
 public:
  explicit RemoveDownloadsTester(TestingProfile* testing_profile)
      : download_manager_(new content::MockDownloadManager()),
        chrome_download_manager_delegate_(testing_profile) {
    content::BrowserContext::SetDownloadManagerForTesting(
        testing_profile, base::WrapUnique(download_manager_));
    EXPECT_EQ(download_manager_,
              content::BrowserContext::GetDownloadManager(testing_profile));

    EXPECT_CALL(*download_manager_, GetDelegate())
        .WillOnce(Return(&chrome_download_manager_delegate_));
    EXPECT_CALL(*download_manager_, Shutdown());
  }

  ~RemoveDownloadsTester() { chrome_download_manager_delegate_.Shutdown(); }

  content::MockDownloadManager* download_manager() { return download_manager_; }

 private:
  content::MockDownloadManager* download_manager_;  // Owned by testing profile.
  ChromeDownloadManagerDelegate chrome_download_manager_delegate_;

  DISALLOW_COPY_AND_ASSIGN(RemoveDownloadsTester);
};

}  // namespace

// RemoveAutofillTester is not a part of the anonymous namespace above, as
// PersonalDataManager declares it a friend in an empty namespace.
class RemoveAutofillTester : public autofill::PersonalDataManagerObserver {
 public:
  explicit RemoveAutofillTester(TestingProfile* profile)
      : personal_data_manager_(
            autofill::PersonalDataManagerFactory::GetForProfile(profile)) {
    autofill::test::DisableSystemServices(profile->GetPrefs());
    personal_data_manager_->AddObserver(this);
  }

  ~RemoveAutofillTester() override {
    personal_data_manager_->RemoveObserver(this);
    autofill::test::ReenableSystemServices();
  }

  // Returns true if there are autofill profiles.
  bool HasProfile() {
    return !personal_data_manager_->GetProfiles().empty() &&
           !personal_data_manager_->GetCreditCards().empty();
  }

  bool HasOrigin(const std::string& origin) {
    const std::vector<autofill::AutofillProfile*>& profiles =
        personal_data_manager_->GetProfiles();
    for (const autofill::AutofillProfile* profile : profiles) {
      if (profile->origin() == origin)
        return true;
    }

    const std::vector<autofill::CreditCard*>& credit_cards =
        personal_data_manager_->GetCreditCards();
    for (const autofill::CreditCard* credit_card : credit_cards) {
      if (credit_card->origin() == origin)
        return true;
    }

    return false;
  }

  // Add two profiles and two credit cards to the database.  In each pair, one
  // entry has a web origin and the other has a Chrome origin.
  void AddProfilesAndCards() {
    std::vector<autofill::AutofillProfile> profiles;
    autofill::AutofillProfile profile;
    profile.set_guid(base::GenerateGUID());
    profile.set_origin(kWebOrigin);
    profile.SetRawInfo(autofill::NAME_FIRST, base::ASCIIToUTF16("Bob"));
    profile.SetRawInfo(autofill::NAME_LAST, base::ASCIIToUTF16("Smith"));
    profile.SetRawInfo(autofill::ADDRESS_HOME_ZIP, base::ASCIIToUTF16("94043"));
    profile.SetRawInfo(autofill::EMAIL_ADDRESS,
                       base::ASCIIToUTF16("sue@example.com"));
    profile.SetRawInfo(autofill::COMPANY_NAME, base::ASCIIToUTF16("Company X"));
    profiles.push_back(profile);

    profile.set_guid(base::GenerateGUID());
    profile.set_origin(autofill::kSettingsOrigin);
    profiles.push_back(profile);

    personal_data_manager_->SetProfiles(&profiles);
    base::RunLoop().Run();

    std::vector<autofill::CreditCard> cards;
    autofill::CreditCard card;
    card.set_guid(base::GenerateGUID());
    card.set_origin(kWebOrigin);
    card.SetRawInfo(autofill::CREDIT_CARD_NUMBER,
                    base::ASCIIToUTF16("1234-5678-9012-3456"));
    cards.push_back(card);

    card.set_guid(base::GenerateGUID());
    card.set_origin(autofill::kSettingsOrigin);
    cards.push_back(card);

    personal_data_manager_->SetCreditCards(&cards);
    base::RunLoop().Run();
  }

 private:
  void OnPersonalDataChanged() override {
    base::MessageLoop::current()->QuitWhenIdle();
  }

  autofill::PersonalDataManager* personal_data_manager_;
  DISALLOW_COPY_AND_ASSIGN(RemoveAutofillTester);
};

class MockReportingService : public net::ReportingService {
 public:
  MockReportingService() {}

  // net::ReportingService implementation:

  ~MockReportingService() override {}

  void QueueReport(const GURL& url,
                   const std::string& group,
                   const std::string& type,
                   std::unique_ptr<const base::Value> body) override {
    NOTREACHED();
  }

  void ProcessHeader(const GURL& url,
                     const std::string& header_value) override {
    NOTREACHED();
  }

  void RemoveBrowsingData(
      int data_type_mask,
      base::Callback<bool(const GURL&)> origin_filter) override {
    ++remove_calls_;
    last_data_type_mask_ = data_type_mask;
    last_origin_filter_ = origin_filter;
  }

  int remove_calls() const { return remove_calls_; }
  int last_data_type_mask() const { return last_data_type_mask_; }
  base::Callback<bool(const GURL&)> last_origin_filter() const {
    return last_origin_filter_;
  }

 private:
  int remove_calls_ = 0;
  int last_data_type_mask_ = 0;
  base::Callback<bool(const GURL&)> last_origin_filter_;

  DISALLOW_COPY_AND_ASSIGN(MockReportingService);
};

class ClearReportingCacheTester {
 public:
  ClearReportingCacheTester(TestingProfile* profile, bool create_service)
      : profile_(profile) {
    if (create_service)
      service_ = base::MakeUnique<MockReportingService>();

    net::URLRequestContext* request_context =
        profile_->GetRequestContext()->GetURLRequestContext();
    old_service_ = request_context->reporting_service();
    request_context->set_reporting_service(service_.get());
  }

  ~ClearReportingCacheTester() {
    net::URLRequestContext* request_context =
        profile_->GetRequestContext()->GetURLRequestContext();
    DCHECK_EQ(service_.get(), request_context->reporting_service());
    request_context->set_reporting_service(old_service_);
  }

  void GetMockInfo(int* remove_calls_out,
                   int* last_data_type_mask_out,
                   base::Callback<bool(const GURL&)>* last_origin_filter_out) {
    DCHECK_NE(nullptr, service_.get());

    *remove_calls_out = service_->remove_calls();
    *last_data_type_mask_out = service_->last_data_type_mask();
    *last_origin_filter_out = service_->last_origin_filter();
  }

 private:
  TestingProfile* profile_;
  std::unique_ptr<MockReportingService> service_;
  net::ReportingService* old_service_;
};

// Test Class -----------------------------------------------------------------

class ChromeBrowsingDataRemoverDelegateTest : public testing::Test {
 public:
  ChromeBrowsingDataRemoverDelegateTest()
      : profile_(new TestingProfile()),
        clear_domain_reliability_tester_(profile_.get()) {
    remover_ = content::BrowserContext::GetBrowsingDataRemover(profile_.get());

#if defined(OS_ANDROID)
    static_cast<ChromeBrowsingDataRemoverDelegate*>(
        profile_->GetBrowsingDataRemoverDelegate())
        ->OverrideWebappRegistryForTesting(
            base::WrapUnique<WebappRegistry>(new TestWebappRegistry()));
#endif
  }

  void TearDown() override {
    // TestingProfile contains a DOMStorageContext.  BrowserContext's destructor
    // posts a message to the WEBKIT thread to delete some of its member
    // variables. We need to ensure that the profile is destroyed, and that
    // the message loop is cleared out, before destroying the threads and loop.
    // Otherwise we leak memory.
    profile_.reset();
    base::RunLoop().RunUntilIdle();

    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  ~ChromeBrowsingDataRemoverDelegateTest() override {}

  void BlockUntilBrowsingDataRemoved(const base::Time& delete_begin,
                                     const base::Time& delete_end,
                                     int remove_mask,
                                     bool include_protected_origins) {
    int origin_type_mask =
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;
    if (include_protected_origins) {
      origin_type_mask |=
          content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;
    }

    content::BrowsingDataRemoverCompletionObserver completion_observer(
        remover_);
    remover_->RemoveAndReply(
        delete_begin, delete_end, remove_mask, origin_type_mask,
        &completion_observer);
    completion_observer.BlockUntilCompletion();
  }

  void BlockUntilOriginDataRemoved(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder) {
    content::BrowsingDataRemoverCompletionObserver completion_observer(
        remover_);
    remover_->RemoveWithFilterAndReply(
        delete_begin, delete_end, remove_mask,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        std::move(filter_builder), &completion_observer);
    completion_observer.BlockUntilCompletion();
  }

  const base::Time& GetBeginTime() {
    return remover_->GetLastUsedBeginTime();
  }

  int GetRemovalMask() {
    return remover_->GetLastUsedRemovalMask();
  }

  int GetOriginTypeMask() {
    return remover_->GetLastUsedOriginTypeMask();
  }

  TestingProfile* GetProfile() {
    return profile_.get();
  }

  const ClearDomainReliabilityTester& clear_domain_reliability_tester() {
    return clear_domain_reliability_tester_;
  }

  bool Match(const GURL& origin,
             int mask,
             storage::SpecialStoragePolicy* policy) {
    return remover_->DoesOriginMatchMask(mask, origin, policy);
  }

 private:
  // Cached pointer to BrowsingDataRemover for access to testing methods.
  content::BrowsingDataRemover* remover_;

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;

  // Needed to mock out DomainReliabilityService, even for unrelated tests.
  ClearDomainReliabilityTester clear_domain_reliability_tester_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowsingDataRemoverDelegateTest);
};

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveSafeBrowsingCookieForever) {
  RemoveSafeBrowsingCookieTester tester;

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                false);

  EXPECT_EQ(content::BrowsingDataRemover::DATA_TYPE_COOKIES, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_FALSE(tester.ContainsCookie());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       RemoveSafeBrowsingCookieLastHour) {
  RemoveSafeBrowsingCookieTester tester;

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                false);

  EXPECT_EQ(content::BrowsingDataRemover::DATA_TYPE_COOKIES, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  // Removing with time period other than all time should not clear safe
  // browsing cookies.
  EXPECT_TRUE(tester.ContainsCookie());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       RemoveSafeBrowsingCookieForeverWithPredicate) {
  RemoveSafeBrowsingCookieTester tester;

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());
  std::unique_ptr<BrowsingDataFilterBuilder> filter(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST));
  filter->AddRegisterableDomain(kTestRegisterableDomain1);
  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                              std::move(filter));

  EXPECT_EQ(content::BrowsingDataRemover::DATA_TYPE_COOKIES, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_TRUE(tester.ContainsCookie());

  std::unique_ptr<BrowsingDataFilterBuilder> filter2(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  filter2->AddRegisterableDomain(kTestRegisterableDomain1);
  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                              std::move(filter2));
  EXPECT_FALSE(tester.ContainsCookie());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveHistoryForever) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));

  tester.AddHistory(kOrigin1, base::Time::Now());
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);

  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_FALSE(tester.HistoryContainsURL(kOrigin1));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveHistoryForLastHour) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  tester.AddHistory(kOrigin1, base::Time::Now());
  tester.AddHistory(kOrigin2, two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin2));

  BlockUntilBrowsingDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);

  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_FALSE(tester.HistoryContainsURL(kOrigin1));
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin2));
}

// This should crash (DCHECK) in Debug, but death tests don't work properly
// here.
// TODO(msramek): To make this testable, the refusal to delete history should
// be made a part of interface (e.g. a success value) as opposed to a DCHECK.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveHistoryProhibited) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowDeletingBrowserHistory, false);

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  tester.AddHistory(kOrigin1, base::Time::Now());
  tester.AddHistory(kOrigin2, two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin2));

  BlockUntilBrowsingDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);
  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Nothing should have been deleted.
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin1));
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin2));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       RemoveMultipleTypesHistoryProhibited) {
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowDeletingBrowserHistory, false);

  // Add some history.
  RemoveHistoryTester history_tester;
  ASSERT_TRUE(history_tester.Init(GetProfile()));
  history_tester.AddHistory(kOrigin1, base::Time::Now());
  ASSERT_TRUE(history_tester.HistoryContainsURL(kOrigin1));

  // Expect that passwords will be deleted, as they do not depend
  // on |prefs::kAllowDeletingBrowserHistory|.
  RemovePasswordsTester tester(GetProfile());
  EXPECT_CALL(*tester.store(), RemoveLoginsByURLAndTimeImpl(_, _, _));

  int removal_mask = ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY |
                     ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS;

  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                removal_mask, false);
  EXPECT_EQ(removal_mask, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify that history was not deleted.
  EXPECT_TRUE(history_tester.HistoryContainsURL(kOrigin1));
}
#endif

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveExternalProtocolData) {
  TestingProfile* profile = GetProfile();
  // Add external protocol data on profile.
  base::DictionaryValue prefs;
  prefs.SetBoolean("tel", true);
  profile->GetPrefs()->Set(prefs::kExcludedSchemes, prefs);

  EXPECT_FALSE(
      profile->GetPrefs()->GetDictionary(prefs::kExcludedSchemes)->empty());

  BlockUntilBrowsingDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_EXTERNAL_PROTOCOL_DATA,
      false);
  EXPECT_TRUE(
      profile->GetPrefs()->GetDictionary(prefs::kExcludedSchemes)->empty());
}

// Test that clearing history deletes favicons not associated with bookmarks.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveFaviconsForever) {
  GURL page_url("http://a");

  RemoveFaviconTester favicon_tester;
  ASSERT_TRUE(favicon_tester.Init(GetProfile()));
  favicon_tester.VisitAndAddFavicon(page_url);
  ASSERT_TRUE(favicon_tester.HasFaviconForPageURL(page_url));

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);
  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
            GetRemovalMask());
  EXPECT_FALSE(favicon_tester.HasFaviconForPageURL(page_url));
}

// Test that a bookmark's favicon is expired and not deleted when clearing
// history. Expiring the favicon causes the bookmark's favicon to be updated
// when the user next visits the bookmarked page. Expiring the bookmark's
// favicon is useful when the bookmark's favicon becomes incorrect (See
// crbug.com/474421 for a sample bug which causes this).
TEST_F(ChromeBrowsingDataRemoverDelegateTest, ExpireBookmarkFavicons) {
  GURL bookmarked_page("http://a");

  TestingProfile* profile = GetProfile();
  profile->CreateBookmarkModel(true);
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
  bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 0,
                         base::ASCIIToUTF16("a"), bookmarked_page);

  RemoveFaviconTester favicon_tester;
  ASSERT_TRUE(favicon_tester.Init(GetProfile()));
  favicon_tester.VisitAndAddFavicon(bookmarked_page);
  ASSERT_TRUE(favicon_tester.HasFaviconForPageURL(bookmarked_page));

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);
  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
            GetRemovalMask());
  EXPECT_TRUE(favicon_tester.HasExpiredFaviconForPageURL(bookmarked_page));
}

// TODO(crbug.com/589586): Disabled, since history is not yet marked as
// a filterable datatype.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_TimeBasedHistoryRemoval) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  tester.AddHistory(kOrigin1, base::Time::Now());
  tester.AddHistory(kOrigin2, two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin2));

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST));
  BlockUntilOriginDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, std::move(builder));

  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_FALSE(tester.HistoryContainsURL(kOrigin1));
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin2));
}

// Verify that clearing autofill form data works.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, AutofillRemovalLastHour) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());

  ASSERT_FALSE(tester.HasProfile());
  tester.AddProfilesAndCards();
  ASSERT_TRUE(tester.HasProfile());

  BlockUntilBrowsingDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA, false);

  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  ASSERT_FALSE(tester.HasProfile());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, AutofillRemovalEverything) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());

  ASSERT_FALSE(tester.HasProfile());
  tester.AddProfilesAndCards();
  ASSERT_TRUE(tester.HasProfile());

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA, false);

  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  ASSERT_FALSE(tester.HasProfile());
}

// Verify that clearing autofill form data works.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       AutofillOriginsRemovedWithHistory) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());

  tester.AddProfilesAndCards();
  EXPECT_FALSE(tester.HasOrigin(std::string()));
  EXPECT_TRUE(tester.HasOrigin(kWebOrigin));
  EXPECT_TRUE(tester.HasOrigin(autofill::kSettingsOrigin));

  BlockUntilBrowsingDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);

  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_TRUE(tester.HasOrigin(std::string()));
  EXPECT_FALSE(tester.HasOrigin(kWebOrigin));
  EXPECT_TRUE(tester.HasOrigin(autofill::kSettingsOrigin));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, ZeroSuggestCacheClear) {
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults,
                   "[\"\", [\"foo\", \"bar\"]]");
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                false);

  // Expect the prefs to be cleared when cookies are removed.
  EXPECT_TRUE(prefs->GetString(omnibox::kZeroSuggestCachedResults).empty());
  EXPECT_EQ(content::BrowsingDataRemover::DATA_TYPE_COOKIES, GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
}

#if defined(OS_CHROMEOS)
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       ContentProtectionPlatformKeysRemoval) {
  chromeos::MockUserManager* mock_user_manager =
      new testing::NiceMock<chromeos::MockUserManager>();
  mock_user_manager->SetActiveUser(
      AccountId::FromUserEmail("test@example.com"));
  chromeos::ScopedUserManagerEnabler user_manager_enabler(mock_user_manager);

  // Owned by DBusThreadManager.
  FakeCryptohomeClient* cryptohome_client = new FakeCryptohomeClient();
  chromeos::DBusThreadManager::GetSetterForTesting()->SetCryptohomeClient(
      base::WrapUnique(cryptohome_client));

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES, false);

  // Expect exactly one call.  No calls means no attempt to delete keys and more
  // than one call means a significant performance problem.
  EXPECT_EQ(1, cryptohome_client->delete_keys_call_count());

  chromeos::DBusThreadManager::Shutdown();
}
#endif

TEST_F(ChromeBrowsingDataRemoverDelegateTest, DomainReliability_Null) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  EXPECT_EQ(0u, tester.clear_count());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, DomainReliability_Beacons) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(CLEAR_BEACONS, tester.last_clear_mode());
  EXPECT_TRUE(ProbablySameFilters(
      BrowsingDataFilterBuilder::BuildNoopFilter(), tester.last_filter()));
}

// TODO(crbug.com/589586): Disabled, since history is not yet marked as
// a filterable datatype.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_DomainReliability_Beacons_WithFilter) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);

  BlockUntilOriginDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, builder->Copy());
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(CLEAR_BEACONS, tester.last_clear_mode());
  EXPECT_TRUE(ProbablySameFilters(
      builder->BuildGeneralFilter(), tester.last_filter()));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, DomainReliability_Contexts) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                false);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(CLEAR_CONTEXTS, tester.last_clear_mode());
  EXPECT_TRUE(ProbablySameFilters(
      BrowsingDataFilterBuilder::BuildNoopFilter(), tester.last_filter()));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DomainReliability_Contexts_WithFilter) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);

  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                              builder->Copy());
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(CLEAR_CONTEXTS, tester.last_clear_mode());
  EXPECT_TRUE(ProbablySameFilters(
      builder->BuildGeneralFilter(), tester.last_filter()));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, DomainReliability_ContextsWin) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY |
          content::BrowsingDataRemover::DATA_TYPE_COOKIES,
      false);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(CLEAR_CONTEXTS, tester.last_clear_mode());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DomainReliability_ProtectedOrigins) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                true);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(CLEAR_CONTEXTS, tester.last_clear_mode());
}

// TODO(juliatuttle): This isn't actually testing the no-monitor case, since
// BrowsingDataRemoverTest now creates one unconditionally, since it's needed
// for some unrelated test cases. This should be fixed so it tests the no-
// monitor case again.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_DomainReliability_NoMonitor) {
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY |
          content::BrowsingDataRemover::DATA_TYPE_COOKIES,
      false);
}

// Tests that the deletion of downloads completes successfully and that
// ChromeDownloadManagerDelegate is correctly created and shut down.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveDownloads) {
  RemoveDownloadsTester tester(GetProfile());

  EXPECT_CALL(
      *tester.download_manager(), RemoveDownloadsByURLAndTime(_, _, _));

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS, false);
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemovePasswordStatistics) {
  RemovePasswordsTester tester(GetProfile());
  base::Callback<bool(const GURL&)> empty_filter;

  EXPECT_CALL(*tester.store(), RemoveStatisticsByOriginAndTimeImpl(
                                   ProbablySameFilter(empty_filter),
                                   base::Time(), base::Time::Max()));
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);
}

// TODO(crbug.com/589586): Disabled, since history is not yet marked as
// a filterable datatype.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_RemovePasswordStatisticsByOrigin) {
  RemovePasswordsTester tester(GetProfile());

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);
  base::Callback<bool(const GURL&)> filter = builder->BuildGeneralFilter();

  EXPECT_CALL(*tester.store(),
              RemoveStatisticsByOriginAndTimeImpl(
                  ProbablySameFilter(filter), base::Time(), base::Time::Max()));
  BlockUntilOriginDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, std::move(builder));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemovePasswordsByTimeOnly) {
  RemovePasswordsTester tester(GetProfile());
  base::Callback<bool(const GURL&)> filter =
      BrowsingDataFilterBuilder::BuildNoopFilter();

  EXPECT_CALL(*tester.store(),
              RemoveLoginsByURLAndTimeImpl(ProbablySameFilter(filter), _, _))
      .WillOnce(Return(password_manager::PasswordStoreChangeList()));
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS, false);
}

// Disabled, since passwords are not yet marked as a filterable datatype.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_RemovePasswordsByOrigin) {
  RemovePasswordsTester tester(GetProfile());
  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);
  base::Callback<bool(const GURL&)> filter = builder->BuildGeneralFilter();

  EXPECT_CALL(*tester.store(),
              RemoveLoginsByURLAndTimeImpl(ProbablySameFilter(filter), _, _))
      .WillOnce(Return(password_manager::PasswordStoreChangeList()));
  BlockUntilOriginDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS,
      std::move(builder));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, DisableAutoSignIn) {
  RemovePasswordsTester tester(GetProfile());
  base::Callback<bool(const GURL&)> empty_filter =
      BrowsingDataFilterBuilder::BuildNoopFilter();

  EXPECT_CALL(
      *tester.store(),
      DisableAutoSignInForOriginsImpl(ProbablySameFilter(empty_filter)))
      .WillOnce(Return(password_manager::PasswordStoreChangeList()));

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                false);
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DisableAutoSignInAfterRemovingPasswords) {
  RemovePasswordsTester tester(GetProfile());
  base::Callback<bool(const GURL&)> empty_filter =
      BrowsingDataFilterBuilder::BuildNoopFilter();

  EXPECT_CALL(*tester.store(), RemoveLoginsByURLAndTimeImpl(_, _, _))
      .WillOnce(Return(password_manager::PasswordStoreChangeList()));
  EXPECT_CALL(
      *tester.store(),
      DisableAutoSignInForOriginsImpl(ProbablySameFilter(empty_filter)))
      .WillOnce(Return(password_manager::PasswordStoreChangeList()));

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS,
      false);
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       RemoveContentSettingsWithBlacklist) {
  // Add our settings.
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      kOrigin1, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
      base::MakeUnique<base::DictionaryValue>());
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      kOrigin2, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
      base::MakeUnique<base::DictionaryValue>());
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      kOrigin3, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
      base::MakeUnique<base::DictionaryValue>());
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      kOrigin4, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
      base::MakeUnique<base::DictionaryValue>());

  // Clear all except for origin1 and origin3.
  std::unique_ptr<BrowsingDataFilterBuilder> filter(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST));
  filter->AddRegisterableDomain(kTestRegisterableDomain1);
  filter->AddRegisterableDomain(kTestRegisterableDomain3);
  BlockUntilOriginDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_USAGE_DATA,
      std::move(filter));

  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_USAGE_DATA,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify we only have true, and they're origin1, origin3, and origin4.
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(), &host_settings);
  EXPECT_EQ(3u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(kOrigin1),
            host_settings[0].primary_pattern)
      << host_settings[0].primary_pattern.ToString();
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(kOrigin4),
            host_settings[1].primary_pattern)
      << host_settings[1].primary_pattern.ToString();
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(kOrigin3),
            host_settings[2].primary_pattern)
      << host_settings[2].primary_pattern.ToString();
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveContentSettings) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(GetProfile());
  map->SetContentSettingDefaultScope(kOrigin1, kOrigin1,
                                     CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                     std::string(), CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(kOrigin2, kOrigin2,
                                     CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                     std::string(), CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(kOrigin3, GURL(),
                                     CONTENT_SETTINGS_TYPE_COOKIES,
                                     std::string(), CONTENT_SETTING_BLOCK);
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  map->SetContentSettingCustomScope(pattern, ContentSettingsPattern::Wildcard(),
                                    CONTENT_SETTINGS_TYPE_COOKIES,
                                    std::string(), CONTENT_SETTING_BLOCK);
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_CONTENT_SETTINGS, false);

  // Everything except the default settings should be deleted now.
  ContentSettingsForOneType host_settings;
  map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
                             &host_settings);
  ASSERT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].primary_pattern)
      << host_settings[0].primary_pattern.ToString();
  EXPECT_EQ(CONTENT_SETTING_ASK, host_settings[0].GetContentSetting());

  map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, std::string(),
                             &host_settings);
  ASSERT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].primary_pattern)
      << host_settings[0].primary_pattern.ToString();
  EXPECT_EQ(CONTENT_SETTING_ASK, host_settings[0].GetContentSetting());

  map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_COOKIES, std::string(),
                             &host_settings);
  ASSERT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].primary_pattern)
      << host_settings[0].primary_pattern.ToString();
  EXPECT_EQ(CONTENT_SETTING_ALLOW, host_settings[0].GetContentSetting());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemoveDurablePermission) {
  // Add our settings.
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());

  DurableStoragePermissionContext durable_permission(GetProfile());
  durable_permission.UpdateContentSetting(kOrigin1, GURL(),
                                          CONTENT_SETTING_ALLOW);
  durable_permission.UpdateContentSetting(kOrigin2, GURL(),
                                          CONTENT_SETTING_ALLOW);

  // Clear all except for origin1 and origin3.
  std::unique_ptr<BrowsingDataFilterBuilder> filter(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST));
  filter->AddRegisterableDomain(kTestRegisterableDomain1);
  filter->AddRegisterableDomain(kTestRegisterableDomain3);
  BlockUntilOriginDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_DURABLE_PERMISSION,
      std::move(filter));

  EXPECT_EQ(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_DURABLE_PERMISSION,
            GetRemovalMask());
  EXPECT_EQ(content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify we only have allow for the first origin.
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_DURABLE_STORAGE, std::string(), &host_settings);

  ASSERT_EQ(2u, host_settings.size());
  // Only the first should should have a setting.
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(kOrigin1),
            host_settings[0].primary_pattern)
      << host_settings[0].primary_pattern.ToString();
  EXPECT_EQ(CONTENT_SETTING_ALLOW, host_settings[0].GetContentSetting());

  // And our wildcard.
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[1].primary_pattern)
      << host_settings[1].primary_pattern.ToString();
  EXPECT_EQ(CONTENT_SETTING_ASK, host_settings[1].GetContentSetting());
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DurablePermissionIsPartOfEmbedderDOMStorage) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  DurableStoragePermissionContext durable_permission(GetProfile());
  durable_permission.UpdateContentSetting(kOrigin1, GURL(),
                                          CONTENT_SETTING_ALLOW);
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_DURABLE_STORAGE, std::string(), &host_settings);
  EXPECT_EQ(2u, host_settings.size());

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_EMBEDDER_DOM_STORAGE, false);

  // After the deletion, only the wildcard should remain.
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_DURABLE_STORAGE, std::string(), &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].primary_pattern)
      << host_settings[0].primary_pattern.ToString();
}

// Test that removing passwords clears HTTP auth data.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       ClearHttpAuthCache_RemovePasswords) {
  net::HttpNetworkSession* http_session = GetProfile()
                                              ->GetRequestContext()
                                              ->GetURLRequestContext()
                                              ->http_transaction_factory()
                                              ->GetSession();
  DCHECK(http_session);

  net::HttpAuthCache* http_auth_cache = http_session->http_auth_cache();
  http_auth_cache->Add(kOrigin1, kTestRealm, net::HttpAuth::AUTH_SCHEME_BASIC,
                       "test challenge",
                       net::AuthCredentials(base::ASCIIToUTF16("foo"),
                                            base::ASCIIToUTF16("bar")),
                       "/");
  CHECK(http_auth_cache->Lookup(kOrigin1, kTestRealm,
                                net::HttpAuth::AUTH_SCHEME_BASIC));

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS, false);

  EXPECT_EQ(nullptr, http_auth_cache->Lookup(kOrigin1, kTestRealm,
                                             net::HttpAuth::AUTH_SCHEME_BASIC));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, ClearPermissionPromptCounts) {
  RemovePermissionPromptCountsTest tester(GetProfile());

  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_1(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  filter_builder_1->AddRegisterableDomain(kTestRegisterableDomain1);

  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_2(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST));
  filter_builder_2->AddRegisterableDomain(kTestRegisterableDomain1);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kBlockPromptsIfDismissedOften}, {});

  {
    // Test REMOVE_HISTORY.
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        kOrigin1, CONTENT_SETTINGS_TYPE_GEOLOCATION));
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        kOrigin1, CONTENT_SETTINGS_TYPE_GEOLOCATION));
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        kOrigin1, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
    EXPECT_FALSE(tester.RecordDismissAndEmbargo(
        kOrigin1, CONTENT_SETTINGS_TYPE_MIDI_SYSEX));
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        kOrigin2, CONTENT_SETTINGS_TYPE_DURABLE_STORAGE));
    tester.CheckEmbargo(kOrigin2, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                        CONTENT_SETTING_ASK);
    EXPECT_FALSE(tester.RecordDismissAndEmbargo(
        kOrigin2, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
    EXPECT_FALSE(tester.RecordDismissAndEmbargo(
        kOrigin2, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
    EXPECT_TRUE(tester.RecordDismissAndEmbargo(
        kOrigin2, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
    tester.CheckEmbargo(kOrigin2, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                        CONTENT_SETTING_BLOCK);

    BlockUntilOriginDataRemoved(
        AnHourAgo(), base::Time::Max(),
        ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_USAGE_DATA,
        std::move(filter_builder_1));

    // kOrigin1 should be gone, but kOrigin2 remains.
    EXPECT_EQ(0, tester.GetIgnoreCount(kOrigin1,
                                       CONTENT_SETTINGS_TYPE_GEOLOCATION));
    EXPECT_EQ(0, tester.GetIgnoreCount(kOrigin1,
                                       CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
    EXPECT_EQ(0, tester.GetDismissCount(kOrigin1,
                                        CONTENT_SETTINGS_TYPE_MIDI_SYSEX));
    EXPECT_EQ(1, tester.GetIgnoreCount(
                     kOrigin2, CONTENT_SETTINGS_TYPE_DURABLE_STORAGE));
    EXPECT_EQ(3, tester.GetDismissCount(kOrigin2,
                                        CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
    tester.CheckEmbargo(kOrigin2, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                        CONTENT_SETTING_BLOCK);

    BlockUntilBrowsingDataRemoved(
        AnHourAgo(), base::Time::Max(),
        ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);

    // Everything should be gone.
    EXPECT_EQ(0, tester.GetIgnoreCount(kOrigin1,
                                       CONTENT_SETTINGS_TYPE_GEOLOCATION));
    EXPECT_EQ(0, tester.GetIgnoreCount(kOrigin1,
                                       CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
    EXPECT_EQ(0, tester.GetDismissCount(kOrigin1,
                                        CONTENT_SETTINGS_TYPE_MIDI_SYSEX));
    EXPECT_EQ(0, tester.GetIgnoreCount(
                     kOrigin2, CONTENT_SETTINGS_TYPE_DURABLE_STORAGE));
    EXPECT_EQ(0, tester.GetDismissCount(
                     kOrigin2, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
    tester.CheckEmbargo(kOrigin2, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                        CONTENT_SETTING_ASK);
  }
  {
    // Test REMOVE_SITE_DATA.
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        kOrigin1, CONTENT_SETTINGS_TYPE_GEOLOCATION));
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        kOrigin1, CONTENT_SETTINGS_TYPE_GEOLOCATION));
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        kOrigin1, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
    EXPECT_FALSE(tester.RecordDismissAndEmbargo(
        kOrigin1, CONTENT_SETTINGS_TYPE_MIDI_SYSEX));
    tester.CheckEmbargo(kOrigin1, CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
                        CONTENT_SETTING_ASK);
    EXPECT_FALSE(tester.RecordIgnoreAndEmbargo(
        kOrigin2, CONTENT_SETTINGS_TYPE_DURABLE_STORAGE));
    EXPECT_FALSE(tester.RecordDismissAndEmbargo(
        kOrigin2, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));

    BlockUntilOriginDataRemoved(
        AnHourAgo(), base::Time::Max(),
        ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_USAGE_DATA,
        std::move(filter_builder_2));

    // kOrigin2 should be gone, but kOrigin1 remains.
    EXPECT_EQ(2, tester.GetIgnoreCount(kOrigin1,
                                       CONTENT_SETTINGS_TYPE_GEOLOCATION));
    EXPECT_EQ(1, tester.GetIgnoreCount(kOrigin1,
                                       CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
    EXPECT_EQ(1, tester.GetDismissCount(kOrigin1,
                                        CONTENT_SETTINGS_TYPE_MIDI_SYSEX));
    EXPECT_EQ(0, tester.GetIgnoreCount(
                     kOrigin2, CONTENT_SETTINGS_TYPE_DURABLE_STORAGE));
    EXPECT_EQ(0, tester.GetDismissCount(
                     kOrigin2, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));

    EXPECT_FALSE(tester.RecordDismissAndEmbargo(
        kOrigin1, CONTENT_SETTINGS_TYPE_MIDI_SYSEX));
    EXPECT_TRUE(tester.RecordDismissAndEmbargo(
        kOrigin1, CONTENT_SETTINGS_TYPE_MIDI_SYSEX));
    EXPECT_EQ(
        3, tester.GetDismissCount(kOrigin1, CONTENT_SETTINGS_TYPE_MIDI_SYSEX));
    tester.CheckEmbargo(kOrigin1, CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
                        CONTENT_SETTING_BLOCK);

    BlockUntilBrowsingDataRemoved(
        AnHourAgo(), base::Time::Max(),
        ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_USAGE_DATA, false);

    // Everything should be gone.
    EXPECT_EQ(0, tester.GetIgnoreCount(kOrigin1,
                                       CONTENT_SETTINGS_TYPE_GEOLOCATION));
    EXPECT_EQ(0, tester.GetIgnoreCount(kOrigin1,
                                       CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
    EXPECT_EQ(0, tester.GetDismissCount(kOrigin1,
                                        CONTENT_SETTINGS_TYPE_MIDI_SYSEX));
    EXPECT_EQ(0, tester.GetIgnoreCount(
                     kOrigin2, CONTENT_SETTINGS_TYPE_DURABLE_STORAGE));
    EXPECT_EQ(0, tester.GetDismissCount(
                     kOrigin2, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
    tester.CheckEmbargo(kOrigin1, CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
                        CONTENT_SETTING_ASK);
  }
}

#if BUILDFLAG(ENABLE_PLUGINS)
TEST_F(ChromeBrowsingDataRemoverDelegateTest, RemovePluginData) {
  RemovePluginDataTester tester(GetProfile());

  tester.AddDomain(kOrigin1.host());
  tester.AddDomain(kOrigin2.host());
  tester.AddDomain(kOrigin3.host());

  std::vector<std::string> expected = {
      kOrigin1.host(), kOrigin2.host(), kOrigin3.host() };
  EXPECT_EQ(expected, tester.GetDomains());

  // Delete data with a filter for the registrable domain of |kOrigin3|.
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  filter_builder->AddRegisterableDomain(kTestRegisterableDomain3);
  BlockUntilOriginDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PLUGIN_DATA,
      std::move(filter_builder));

  // Plugin data for |kOrigin3.host()| should have been removed.
  expected.pop_back();
  EXPECT_EQ(expected, tester.GetDomains());

  // TODO(msramek): Mock PluginDataRemover and test the complete deletion
  // of plugin data as well.
}
#endif

// Test that the remover clears bookmark meta data (normally added in a tab
// helper).
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       BookmarkLastVisitDatesGetCleared) {
  GetProfile()->CreateBookmarkModel(true);

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

  const base::Time delete_begin =
      base::Time::Now() - base::TimeDelta::FromDays(1);

  // Create a couple of bookmarks.
  bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 0,
                         base::string16(),
                         GURL("http://foo.org/desktop"));
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::string16(),
                         GURL("http://foo.org/mobile"));

  // Simulate their visits (this is using Time::Now() as timestamps).
  ntp_snippets::UpdateBookmarkOnURLVisitedInMainFrame(
      bookmark_model, GURL("http://foo.org/desktop"),
      /*is_mobile_platform=*/false);
  ntp_snippets::UpdateBookmarkOnURLVisitedInMainFrame(
      bookmark_model, GURL("http://foo.org/mobile"),
      /*is_mobile_platform=*/true);

  // Add a bookmark with a visited timestamp before the deletion interval.
  bookmarks::BookmarkNode::MetaInfoMap meta_info = {
      {"last_visited",
       base::Int64ToString((delete_begin - base::TimeDelta::FromSeconds(1))
                               .ToInternalValue())}};
  bookmark_model->AddURLWithCreationTimeAndMetaInfo(
      bookmark_model->mobile_node(), 0, base::ASCIIToUTF16("my title"),
      GURL("http://foo-2.org/"), delete_begin - base::TimeDelta::FromDays(1),
      &meta_info);

  // There should be some recently visited bookmarks.
  EXPECT_THAT(ntp_snippets::GetRecentlyVisitedBookmarks(
                  bookmark_model, 2, base::Time::UnixEpoch(),
                  /*consider_visits_from_desktop=*/false),
              Not(IsEmpty()));

  // Inject the bookmark model into the remover.
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(GetProfile());

  content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
  remover->RemoveAndReply(delete_begin, base::Time::Max(),
                          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
                          ChromeBrowsingDataRemoverDelegate::ALL_ORIGIN_TYPES,
                          &completion_observer);
  completion_observer.BlockUntilCompletion();

  // There should be only 1 recently visited bookmarks.
  std::vector<const bookmarks::BookmarkNode*> remaining_nodes =
      ntp_snippets::GetRecentlyVisitedBookmarks(
          bookmark_model, 3, base::Time::UnixEpoch(),
          /*consider_visits_from_desktop=*/true);
  EXPECT_THAT(remaining_nodes, SizeIs(1));
  EXPECT_THAT(remaining_nodes[0]->url().spec(), Eq("http://foo-2.org/"));
}

// Test that the remover clears language model data (normally added by the
// LanguageDetectionDriver).
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       LanguageHistogramClearedOnClearingCompleteHistory) {
  language::UrlLanguageHistogram* language_histogram =
      UrlLanguageHistogramFactory::GetInstance()->GetForBrowserContext(
          GetProfile());

  // Simulate browsing.
  for (int i = 0; i < 100; i++) {
    language_histogram->OnPageVisited("en");
    language_histogram->OnPageVisited("en");
    language_histogram->OnPageVisited("en");
    language_histogram->OnPageVisited("es");
  }

  // Clearing a part of the history has no effect.
  BlockUntilBrowsingDataRemoved(
      AnHourAgo(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);

  EXPECT_THAT(language_histogram->GetTopLanguages(), SizeIs(2));
  EXPECT_THAT(language_histogram->GetLanguageFrequency("en"), FloatEq(0.75));
  EXPECT_THAT(language_histogram->GetLanguageFrequency("es"), FloatEq(0.25));

  // Clearing the full history does the trick.
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, false);

  EXPECT_THAT(language_histogram->GetTopLanguages(), SizeIs(0));
  EXPECT_THAT(language_histogram->GetLanguageFrequency("en"), FloatEq(0.0));
  EXPECT_THAT(language_histogram->GetLanguageFrequency("es"), FloatEq(0.0));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(ChromeBrowsingDataRemoverDelegateTest, OriginTypeMasks) {
  scoped_refptr<MockExtensionSpecialStoragePolicy> mock_policy =
      new MockExtensionSpecialStoragePolicy;
  // Protect kOrigin1.
  mock_policy->AddProtected(kOrigin1.GetOrigin());

  EXPECT_FALSE(Match(kOrigin1, kUnprotected, mock_policy.get()));
  EXPECT_TRUE(Match(kOrigin2, kUnprotected, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginExt, kUnprotected, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginDevTools, kUnprotected, mock_policy.get()));

  EXPECT_TRUE(Match(kOrigin1, kProtected, mock_policy.get()));
  EXPECT_FALSE(Match(kOrigin2, kProtected, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginExt, kProtected, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginDevTools, kProtected, mock_policy.get()));

  EXPECT_FALSE(Match(kOrigin1, kExtension, mock_policy.get()));
  EXPECT_FALSE(Match(kOrigin2, kExtension, mock_policy.get()));
  EXPECT_TRUE(Match(kOriginExt, kExtension, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginDevTools, kExtension, mock_policy.get()));

  EXPECT_TRUE(Match(kOrigin1, kUnprotected | kProtected, mock_policy.get()));
  EXPECT_TRUE(Match(kOrigin2, kUnprotected | kProtected, mock_policy.get()));
  EXPECT_FALSE(Match(kOriginExt, kUnprotected | kProtected, mock_policy.get()));
  EXPECT_FALSE(
      Match(kOriginDevTools, kUnprotected | kProtected, mock_policy.get()));

  EXPECT_FALSE(Match(kOrigin1, kUnprotected | kExtension, mock_policy.get()));
  EXPECT_TRUE(Match(kOrigin2, kUnprotected | kExtension, mock_policy.get()));
  EXPECT_TRUE(Match(kOriginExt, kUnprotected | kExtension, mock_policy.get()));
  EXPECT_FALSE(
      Match(kOriginDevTools, kUnprotected | kExtension, mock_policy.get()));

  EXPECT_TRUE(Match(kOrigin1, kProtected | kExtension, mock_policy.get()));
  EXPECT_FALSE(Match(kOrigin2, kProtected | kExtension, mock_policy.get()));
  EXPECT_TRUE(Match(kOriginExt, kProtected | kExtension, mock_policy.get()));
  EXPECT_FALSE(
      Match(kOriginDevTools, kProtected | kExtension, mock_policy.get()));

  EXPECT_TRUE(Match(kOrigin1, kUnprotected | kProtected | kExtension,
                    mock_policy.get()));
  EXPECT_TRUE(Match(kOrigin2, kUnprotected | kProtected | kExtension,
                    mock_policy.get()));
  EXPECT_TRUE(Match(kOriginExt, kUnprotected | kProtected | kExtension,
                    mock_policy.get()));
  EXPECT_FALSE(Match(kOriginDevTools, kUnprotected | kProtected | kExtension,
                     mock_policy.get()));
}
#endif

// If extensions are disabled, there is no policy.
TEST_F(ChromeBrowsingDataRemoverDelegateTest, OriginTypeMasksNoPolicy) {
  EXPECT_TRUE(Match(kOrigin1, kUnprotected, nullptr));
  EXPECT_FALSE(Match(kOriginExt, kUnprotected, nullptr));
  EXPECT_FALSE(Match(kOriginDevTools, kUnprotected, nullptr));

  EXPECT_FALSE(Match(kOrigin1, kProtected, nullptr));
  EXPECT_FALSE(Match(kOriginExt, kProtected, nullptr));
  EXPECT_FALSE(Match(kOriginDevTools, kProtected, nullptr));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_FALSE(Match(kOrigin1, kExtension, nullptr));
  EXPECT_TRUE(Match(kOriginExt, kExtension, nullptr));
  EXPECT_FALSE(Match(kOriginDevTools, kExtension, nullptr));
#endif
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, ReportingCache_NoService) {
  ClearReportingCacheTester tester(GetProfile(), false);

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
      true);

  // Nothing to check, since there's no mock service; we're just making sure
  // nothing crashes without a service.
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, ReportingCache_Cookies) {
  ClearReportingCacheTester tester(GetProfile(), true);

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                                true);

  int remove_count;
  int data_type_mask;
  base::Callback<bool(const GURL&)> origin_filter;
  tester.GetMockInfo(&remove_count, &data_type_mask, &origin_filter);

  EXPECT_EQ(1, remove_count);
  EXPECT_EQ(net::ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS,
            data_type_mask);
  EXPECT_TRUE(ProbablySameFilters(BrowsingDataFilterBuilder::BuildNoopFilter(),
                                  origin_filter));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       ReportingCache_Cookies_WithFilter) {
  ClearReportingCacheTester tester(GetProfile(), true);

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);

  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                              builder->Copy());

  int remove_count;
  int data_type_mask;
  base::Callback<bool(const GURL&)> origin_filter;
  tester.GetMockInfo(&remove_count, &data_type_mask, &origin_filter);

  EXPECT_EQ(1, remove_count);
  EXPECT_EQ(net::ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS,
            data_type_mask);
  EXPECT_TRUE(
      ProbablySameFilters(builder->BuildGeneralFilter(), origin_filter));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest, ReportingCache_History) {
  ClearReportingCacheTester tester(GetProfile(), true);

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, true);

  int remove_count;
  int data_type_mask;
  base::Callback<bool(const GURL&)> origin_filter;
  tester.GetMockInfo(&remove_count, &data_type_mask, &origin_filter);

  EXPECT_EQ(1, remove_count);
  EXPECT_EQ(net::ReportingBrowsingDataRemover::DATA_TYPE_REPORTS,
            data_type_mask);
  EXPECT_TRUE(ProbablySameFilters(BrowsingDataFilterBuilder::BuildNoopFilter(),
                                  origin_filter));
}

// TODO(crbug.com/589586): Disabled, since history is not yet marked as
// a filterable datatype.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_ReportingCache_History_WithFilter) {
  ClearReportingCacheTester tester(GetProfile(), true);

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);

  BlockUntilOriginDataRemoved(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, builder->Copy());

  int remove_count;
  int data_type_mask;
  base::Callback<bool(const GURL&)> origin_filter;
  tester.GetMockInfo(&remove_count, &data_type_mask, &origin_filter);

  EXPECT_EQ(1, remove_count);
  EXPECT_EQ(net::ReportingBrowsingDataRemover::DATA_TYPE_REPORTS,
            data_type_mask);
  EXPECT_TRUE(
      ProbablySameFilters(builder->BuildGeneralFilter(), origin_filter));
}

TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       ReportingCache_CookiesAndHistory) {
  ClearReportingCacheTester tester(GetProfile(), true);

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
      true);

  int remove_count;
  int data_type_mask;
  base::Callback<bool(const GURL&)> origin_filter;
  tester.GetMockInfo(&remove_count, &data_type_mask, &origin_filter);

  EXPECT_EQ(1, remove_count);
  EXPECT_EQ(net::ReportingBrowsingDataRemover::DATA_TYPE_REPORTS |
                net::ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS,
            data_type_mask);
  EXPECT_TRUE(ProbablySameFilters(BrowsingDataFilterBuilder::BuildNoopFilter(),
                                  origin_filter));
}

// TODO(crbug.com/589586): Disabled, since history is not yet marked as
// a filterable datatype.
TEST_F(ChromeBrowsingDataRemoverDelegateTest,
       DISABLED_ReportingCache_CookiesAndHistory_WithFilter) {
  ClearReportingCacheTester tester(GetProfile(), true);

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);

  BlockUntilOriginDataRemoved(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
      builder->Copy());

  int remove_count;
  int data_type_mask;
  base::Callback<bool(const GURL&)> origin_filter;
  tester.GetMockInfo(&remove_count, &data_type_mask, &origin_filter);

  EXPECT_EQ(1, remove_count);
  EXPECT_EQ(net::ReportingBrowsingDataRemover::DATA_TYPE_REPORTS |
                net::ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS,
            data_type_mask);
  EXPECT_TRUE(
      ProbablySameFilters(builder->BuildGeneralFilter(), origin_filter));
}
