// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_remover.h"

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browsing_data/browsing_data_filter_builder.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "chrome/browser/browsing_data/browsing_data_remover_test_util.h"
#include "chrome/browser/browsing_data/registrable_domain_filter_builder.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
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
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/cookies/cookie_store.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/channel_id_store.h"
#include "net/ssl/ssl_client_cert_type.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/favicon_size.h"
#include "url/origin.h"

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/android/webapps/webapp_registry.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#include "components/signin/core/account_id/account_id.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/mock_extension_special_storage_policy.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "chrome/browser/browsing_data/mock_browsing_data_flash_lso_helper.h"
#endif

class MockExtensionSpecialStoragePolicy;

using content::BrowserThread;
using content::StoragePartition;
using domain_reliability::CLEAR_BEACONS;
using domain_reliability::CLEAR_CONTEXTS;
using domain_reliability::DomainReliabilityClearMode;
using domain_reliability::DomainReliabilityMonitor;
using domain_reliability::DomainReliabilityService;
using domain_reliability::DomainReliabilityServiceFactory;
using testing::_;
using testing::ByRef;
using testing::Invoke;
using testing::Matcher;
using testing::MakeMatcher;
using testing::MatcherInterface;
using testing::MatchResultListener;
using testing::Return;
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

// For Autofill.
const char kWebOrigin[] = "https://www.example.com/";

const GURL kOrigin1(kTestOrigin1);
const GURL kOrigin2(kTestOrigin2);
const GURL kOrigin3(kTestOrigin3);
const GURL kOrigin4(kTestOrigin4);
const GURL kOriginExt(kTestOriginExt);
const GURL kOriginDevTools(kTestOriginDevTools);

const base::FilePath::CharType kDomStorageOrigin1[] =
    FILE_PATH_LITERAL("http_host1_1.localstorage");

const base::FilePath::CharType kDomStorageOrigin2[] =
    FILE_PATH_LITERAL("http_host2_1.localstorage");

const base::FilePath::CharType kDomStorageOrigin3[] =
    FILE_PATH_LITERAL("http_host3_1.localstorage");

const base::FilePath::CharType kDomStorageExt[] = FILE_PATH_LITERAL(
    "chrome-extension_abcdefghijklmnopqrstuvwxyz_0.localstorage");

bool MatchPrimaryPattern(const ContentSettingsPattern& expected_primary,
                         const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern) {
  return expected_primary == primary_pattern;
}

#if defined(OS_CHROMEOS)
void FakeDBusCall(const chromeos::BoolDBusMethodCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, chromeos::DBUS_METHOD_CALL_SUCCESS, true));
}
#endif

struct StoragePartitionRemovalData {
  uint32_t remove_mask = 0;
  uint32_t quota_storage_remove_mask = 0;
  base::Time remove_begin;
  base::Time remove_end;
  StoragePartition::OriginMatcherFunction origin_matcher;
  StoragePartition::CookieMatcherFunction cookie_matcher;

  StoragePartitionRemovalData() {}
};

net::CanonicalCookie CreateCookieWithHost(const GURL& source) {
  std::unique_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
      source, "A", "1", std::string(), "/", base::Time::Now(),
      base::Time::Now(), false, false, net::CookieSameSite::DEFAULT_MODE, false,
      net::COOKIE_PRIORITY_MEDIUM));
  EXPECT_TRUE(cookie);
  return *cookie;
}

class TestStoragePartition : public StoragePartition {
 public:
  TestStoragePartition() {}
  ~TestStoragePartition() override {}

  // content::StoragePartition implementation.
  base::FilePath GetPath() override { return base::FilePath(); }
  net::URLRequestContextGetter* GetURLRequestContext() override {
    return nullptr;
  }
  net::URLRequestContextGetter* GetMediaURLRequestContext() override {
    return nullptr;
  }
  storage::QuotaManager* GetQuotaManager() override { return nullptr; }
  content::AppCacheService* GetAppCacheService() override { return nullptr; }
  storage::FileSystemContext* GetFileSystemContext() override {
    return nullptr;
  }
  storage::DatabaseTracker* GetDatabaseTracker() override { return nullptr; }
  content::DOMStorageContext* GetDOMStorageContext() override {
    return nullptr;
  }
  content::IndexedDBContext* GetIndexedDBContext() override { return nullptr; }
  content::ServiceWorkerContext* GetServiceWorkerContext() override {
    return nullptr;
  }
  content::CacheStorageContext* GetCacheStorageContext() override {
    return nullptr;
  }
  content::PlatformNotificationContext* GetPlatformNotificationContext()
      override {
    return nullptr;
  }
  content::HostZoomMap* GetHostZoomMap() override { return nullptr; }
  content::HostZoomLevelContext* GetHostZoomLevelContext() override {
    return nullptr;
  }
  content::ZoomLevelDelegate* GetZoomLevelDelegate() override {
    return nullptr;
  }

  void ClearDataForOrigin(uint32_t remove_mask,
                          uint32_t quota_storage_remove_mask,
                          const GURL& storage_origin,
                          net::URLRequestContextGetter* rq_context,
                          const base::Closure& callback) override {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&TestStoragePartition::AsyncRunCallback,
                                       base::Unretained(this),
                                       callback));
  }

  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 const GURL& storage_origin,
                 const OriginMatcherFunction& origin_matcher,
                 const base::Time begin,
                 const base::Time end,
                 const base::Closure& callback) override {
    // Store stuff to verify parameters' correctness later.
    storage_partition_removal_data_.remove_mask = remove_mask;
    storage_partition_removal_data_.quota_storage_remove_mask =
        quota_storage_remove_mask;
    storage_partition_removal_data_.remove_begin = begin;
    storage_partition_removal_data_.remove_end = end;
    storage_partition_removal_data_.origin_matcher = origin_matcher;

    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&TestStoragePartition::AsyncRunCallback,
                   base::Unretained(this), callback));
  }

  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 const OriginMatcherFunction& origin_matcher,
                 const CookieMatcherFunction& cookie_matcher,
                 const base::Time begin,
                 const base::Time end,
                 const base::Closure& callback) override {
    // Store stuff to verify parameters' correctness later.
    storage_partition_removal_data_.remove_mask = remove_mask;
    storage_partition_removal_data_.quota_storage_remove_mask =
        quota_storage_remove_mask;
    storage_partition_removal_data_.remove_begin = begin;
    storage_partition_removal_data_.remove_end = end;
    storage_partition_removal_data_.origin_matcher = origin_matcher;
    storage_partition_removal_data_.cookie_matcher = cookie_matcher;

    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&TestStoragePartition::AsyncRunCallback,
                                       base::Unretained(this), callback));
  }

  void Flush() override {}

  StoragePartitionRemovalData GetStoragePartitionRemovalData() {
    return storage_partition_removal_data_;
  }

 private:
  void AsyncRunCallback(const base::Closure& callback) {
    callback.Run();
  }

  StoragePartitionRemovalData storage_partition_removal_data_;

  DISALLOW_COPY_AND_ASSIGN(TestStoragePartition);
};

#if BUILDFLAG(ANDROID_JAVA_UI)
class TestWebappRegistry : public WebappRegistry {
 public:
  TestWebappRegistry() : WebappRegistry() { }

  void UnregisterWebappsForUrls(
      const base::Callback<bool(const GURL&)>& url_filter,
      const base::Closure& callback) override {
    // Mocks out a JNI call and runs the callback as a delayed task.
    BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE, callback,
                                   base::TimeDelta::FromMilliseconds(10));
  }

  void ClearWebappHistory(const base::Closure& callback) override {
    // Mocks out a JNI call and runs the callback as a delayed task.
    BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE, callback,
                                   base::TimeDelta::FromMilliseconds(10));
  }
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

}  // namespace

// Testers -------------------------------------------------------------------

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
        base::Bind(&RemoveCookieTester::GetCookieCallback,
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
        base::Bind(&RemoveCookieTester::SetCookieCallback,
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
                                   int cookies_deleted) {
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
        base::Bind(&RunClosureAfterCookiesCleared, run_loop.QuitClosure()));
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

class RemoveChannelIDTester : public net::SSLConfigService::Observer {
 public:
  explicit RemoveChannelIDTester(TestingProfile* profile) {
    channel_id_service_ = profile->GetRequestContext()->
        GetURLRequestContext()->channel_id_service();
    ssl_config_service_ = profile->GetSSLConfigService();
    ssl_config_service_->AddObserver(this);
  }

  ~RemoveChannelIDTester() override {
    ssl_config_service_->RemoveObserver(this);
  }

  int ChannelIDCount() { return channel_id_service_->channel_id_count(); }

  // Add a server bound cert for |server| with specific creation and expiry
  // times.  The cert and key data will be filled with dummy values.
  void AddChannelIDWithTimes(const std::string& server_identifier,
                             base::Time creation_time) {
    GetChannelIDStore()->SetChannelID(
        base::WrapUnique(new net::ChannelIDStore::ChannelID(
            server_identifier, creation_time, crypto::ECPrivateKey::Create())));
  }

  // Add a server bound cert for |server|, with the current time as the
  // creation time.  The cert and key data will be filled with dummy values.
  void AddChannelID(const std::string& server_identifier) {
    base::Time now = base::Time::Now();
    AddChannelIDWithTimes(server_identifier, now);
  }

  void GetChannelIDList(net::ChannelIDStore::ChannelIDList* channel_ids) {
    GetChannelIDStore()->GetAllChannelIDs(
        base::Bind(&RemoveChannelIDTester::GetAllChannelIDsCallback,
                   channel_ids));
  }

  net::ChannelIDStore* GetChannelIDStore() {
    return channel_id_service_->GetChannelIDStore();
  }

  int ssl_config_changed_count() const {
    return ssl_config_changed_count_;
  }

  // net::SSLConfigService::Observer implementation:
  void OnSSLConfigChanged() override { ssl_config_changed_count_++; }

 private:
  static void GetAllChannelIDsCallback(
      net::ChannelIDStore::ChannelIDList* dest,
      const net::ChannelIDStore::ChannelIDList& result) {
    *dest = result;
  }

  net::ChannelIDService* channel_id_service_;
  scoped_refptr<net::SSLConfigService> ssl_config_service_;
  int ssl_config_changed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(RemoveChannelIDTester);
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

class RemoveLocalStorageTester {
 public:
  explicit RemoveLocalStorageTester(TestingProfile* profile)
      : profile_(profile) {
    dom_storage_context_ =
        content::BrowserContext::GetDefaultStoragePartition(profile)->
            GetDOMStorageContext();
  }

  // Returns true, if the given origin URL exists.
  bool DOMStorageExistsForOrigin(const GURL& origin) {
    scoped_refptr<content::MessageLoopRunner> message_loop_runner =
        new content::MessageLoopRunner;
    quit_closure_ = message_loop_runner->QuitClosure();
    GetLocalStorageUsage();
    message_loop_runner->Run();
    for (size_t i = 0; i < infos_.size(); ++i) {
      if (origin == infos_[i].origin)
        return true;
    }
    return false;
  }

  void AddDOMStorageTestData() {
    // Note: This test depends on details of how the dom_storage library
    // stores data in the host file system.
    base::FilePath storage_path =
        profile_->GetPath().AppendASCII("Local Storage");
    base::CreateDirectory(storage_path);

    // Write some files.
    base::WriteFile(storage_path.Append(kDomStorageOrigin1), nullptr, 0);
    base::WriteFile(storage_path.Append(kDomStorageOrigin2), nullptr, 0);
    base::WriteFile(storage_path.Append(kDomStorageOrigin3), nullptr, 0);
    base::WriteFile(storage_path.Append(kDomStorageExt), nullptr, 0);

    // Tweak their dates.
    base::Time now = base::Time::Now();
    base::TouchFile(storage_path.Append(kDomStorageOrigin1), now, now);

    base::Time one_day_ago = now - base::TimeDelta::FromDays(1);
    base::TouchFile(storage_path.Append(kDomStorageOrigin2),
                    one_day_ago, one_day_ago);

    base::Time sixty_days_ago = now - base::TimeDelta::FromDays(60);
    base::TouchFile(storage_path.Append(kDomStorageOrigin3),
                    sixty_days_ago, sixty_days_ago);

    base::TouchFile(storage_path.Append(kDomStorageExt), now, now);
  }

 private:
  void GetLocalStorageUsage() {
    dom_storage_context_->GetLocalStorageUsage(
        base::Bind(&RemoveLocalStorageTester::OnGotLocalStorageUsage,
                   base::Unretained(this)));
  }
  void OnGotLocalStorageUsage(
      const std::vector<content::LocalStorageUsageInfo>& infos) {
    infos_ = infos;
    quit_closure_.Run();
  }

  // We don't own these pointers.
  TestingProfile* profile_;
  content::DOMStorageContext* dom_storage_context_ = nullptr;

  std::vector<content::LocalStorageUsageInfo> infos_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(RemoveLocalStorageTester);
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
    profile_->SetUserData(kKey, data);

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

class RemoveDownloadsTester {
 public:
  explicit RemoveDownloadsTester(TestingProfile* testing_profile)
      : download_manager_(new content::MockDownloadManager()),
        chrome_download_manager_delegate_(testing_profile) {
    content::BrowserContext::SetDownloadManagerForTesting(testing_profile,
                                                          download_manager_);
    EXPECT_EQ(download_manager_,
              content::BrowserContext::GetDownloadManager(testing_profile));

    EXPECT_CALL(*download_manager_, GetDelegate())
        .WillOnce(Return(&chrome_download_manager_delegate_));
    EXPECT_CALL(*download_manager_, Shutdown());
  }

  ~RemoveDownloadsTester() { chrome_download_manager_delegate_.Shutdown(); }

  content::MockDownloadManager* download_manager() { return download_manager_; }

 private:
  content::MockDownloadManager* download_manager_;
  ChromeDownloadManagerDelegate chrome_download_manager_delegate_;

  DISALLOW_COPY_AND_ASSIGN(RemoveDownloadsTester);
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
      : blocker_(new PermissionDecisionAutoBlocker(profile)),
        profile_(profile) {}

  int GetDismissCount(const GURL& url, content::PermissionType permission) {
    return PermissionDecisionAutoBlocker::GetDismissCount(
        url, permission, profile_);
  }

  int GetIgnoreCount(const GURL& url, content::PermissionType permission) {
    return PermissionDecisionAutoBlocker::GetIgnoreCount(
        url, permission, profile_);
  }

  int RecordIgnore(const GURL& url, content::PermissionType permission) {
    return blocker_->RecordIgnore(url, permission);
  }

  bool ShouldChangeDismissalToBlock(const GURL& url,
                                    content::PermissionType permission) {
    return blocker_->ShouldChangeDismissalToBlock(url, permission);
  }

 private:
  std::unique_ptr<PermissionDecisionAutoBlocker> blocker_;
  TestingProfile* profile_;

  DISALLOW_COPY_AND_ASSIGN(RemovePermissionPromptCountsTest);
};

#if defined(ENABLE_PLUGINS)
// A small modification to MockBrowsingDataFlashLSOHelper so that it responds
// immediately and does not wait for the Notify() call. Otherwise it would
// deadlock BrowsingDataRemover::RemoveImpl.
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
    BrowsingDataRemoverFactory::GetForBrowserContext(profile)
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

// Test Class ----------------------------------------------------------------

class BrowsingDataRemoverTest : public testing::Test {
 public:
  BrowsingDataRemoverTest()
      : profile_(new TestingProfile()),
        clear_domain_reliability_tester_(GetProfile()) {
    remover_ =
        BrowsingDataRemoverFactory::GetForBrowserContext(profile_.get());

#if BUILDFLAG(ANDROID_JAVA_UI)
    remover_->OverrideWebappRegistryForTesting(
        std::unique_ptr<WebappRegistry>(new TestWebappRegistry()));
#endif
  }

  ~BrowsingDataRemoverTest() override {}

  void TearDown() override {
#if defined(ENABLE_EXTENSIONS)
    mock_policy_ = nullptr;
#endif

    // TestingProfile contains a DOMStorageContext.  BrowserContext's destructor
    // posts a message to the WEBKIT thread to delete some of its member
    // variables. We need to ensure that the profile is destroyed, and that
    // the message loop is cleared out, before destroying the threads and loop.
    // Otherwise we leak memory.
    profile_.reset();
    base::RunLoop().RunUntilIdle();

    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  void BlockUntilBrowsingDataRemoved(browsing_data::TimePeriod period,
                                     int remove_mask,
                                     bool include_protected_origins) {
    BrowsingDataRemover* remover =
        BrowsingDataRemoverFactory::GetForBrowserContext(profile_.get());

    TestStoragePartition storage_partition;
    remover->OverrideStoragePartitionForTesting(&storage_partition);

    int origin_type_mask = BrowsingDataHelper::UNPROTECTED_WEB;
    if (include_protected_origins)
      origin_type_mask |= BrowsingDataHelper::PROTECTED_WEB;

    BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->RemoveAndReply(BrowsingDataRemover::Period(period), remove_mask,
                            origin_type_mask, &completion_observer);
    completion_observer.BlockUntilCompletion();

    // Save so we can verify later.
    storage_partition_removal_data_ =
        storage_partition.GetStoragePartitionRemovalData();
  }

  void BlockUntilOriginDataRemoved(
      browsing_data::TimePeriod period,
      int remove_mask,
      const BrowsingDataFilterBuilder& filter_builder) {
    BrowsingDataRemover* remover =
        BrowsingDataRemoverFactory::GetForBrowserContext(profile_.get());
    TestStoragePartition storage_partition;
    remover->OverrideStoragePartitionForTesting(&storage_partition);

    BrowsingDataRemoverCompletionInhibitor completion_inhibitor;
    remover->RemoveImpl(BrowsingDataRemover::Period(period), remove_mask,
                        filter_builder, BrowsingDataHelper::UNPROTECTED_WEB);
    completion_inhibitor.BlockUntilNearCompletion();
    completion_inhibitor.ContinueToCompletion();

    // Save so we can verify later.
    storage_partition_removal_data_ =
        storage_partition.GetStoragePartitionRemovalData();
  }

  TestingProfile* GetProfile() {
    return profile_.get();
  }

  void DestroyProfile() { profile_.reset(); }

  const base::Time& GetBeginTime() {
    return remover_->GetLastUsedBeginTime();
  }

  int GetRemovalMask() {
    return remover_->GetLastUsedRemovalMask();
  }

  int GetOriginTypeMask() {
    return remover_->GetLastUsedOriginTypeMask();
  }

  StoragePartitionRemovalData GetStoragePartitionRemovalData() {
    return storage_partition_removal_data_;
  }

  MockExtensionSpecialStoragePolicy* CreateMockPolicy() {
#if defined(ENABLE_EXTENSIONS)
    mock_policy_ = new MockExtensionSpecialStoragePolicy;
    return mock_policy_.get();
#else
    NOTREACHED();
    return nullptr;
#endif
  }

  storage::SpecialStoragePolicy* mock_policy() {
#if defined(ENABLE_EXTENSIONS)
    return mock_policy_.get();
#else
    return nullptr;
#endif
  }

  // If |kOrigin1| is protected when extensions are enabled, the expected
  // result for tests where the OriginMatcherFunction result is variable.
  bool ShouldRemoveForProtectedOriginOne() const {
#if defined(ENABLE_EXTENSIONS)
    return false;
#else
    return true;
#endif
  }

  const ClearDomainReliabilityTester& clear_domain_reliability_tester() {
    return clear_domain_reliability_tester_;
  }

 private:
  // Cached pointer to BrowsingDataRemover for access to testing methods.
  BrowsingDataRemover* remover_;

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;

  StoragePartitionRemovalData storage_partition_removal_data_;

#if defined(ENABLE_EXTENSIONS)
  scoped_refptr<MockExtensionSpecialStoragePolicy> mock_policy_;
#endif

  // Needed to mock out DomainReliabilityService, even for unrelated tests.
  ClearDomainReliabilityTester clear_domain_reliability_tester_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverTest);
};

// Tests ---------------------------------------------------------------------

TEST_F(BrowsingDataRemoverTest, RemoveCookieForever) {
  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_COOKIES, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the cookies.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_COOKIES);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());
}

TEST_F(BrowsingDataRemoverTest, RemoveCookieLastHour) {
  BlockUntilBrowsingDataRemoved(browsing_data::LAST_HOUR,
                                BrowsingDataRemover::REMOVE_COOKIES, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the cookies.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_COOKIES);
  // Removing with time period other than ALL_TIME should not clear
  // persistent storage data.
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT);
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());
}

TEST_F(BrowsingDataRemoverTest, RemoveCookiesDomainBlacklist) {
  RegistrableDomainFilterBuilder filter(
      RegistrableDomainFilterBuilder::BLACKLIST);
  filter.AddRegisterableDomain(kTestRegisterableDomain1);
  filter.AddRegisterableDomain(kTestRegisterableDomain3);
  BlockUntilOriginDataRemoved(browsing_data::LAST_HOUR,
                              BrowsingDataRemover::REMOVE_COOKIES, filter);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the cookies.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_COOKIES);
  // Removing with time period other than ALL_TIME should not clear
  // persistent storage data.
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT);
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
  // Even though it's a different origin, it's the same domain.
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOrigin4, mock_policy()));

  EXPECT_FALSE(removal_data.cookie_matcher.Run(CreateCookieWithHost(kOrigin1)));
  EXPECT_TRUE(removal_data.cookie_matcher.Run(CreateCookieWithHost(kOrigin2)));
  EXPECT_FALSE(removal_data.cookie_matcher.Run(CreateCookieWithHost(kOrigin3)));
  // This is false, because this is the same domain as 3, just with a different
  // scheme.
  EXPECT_FALSE(removal_data.cookie_matcher.Run(CreateCookieWithHost(kOrigin4)));
}

TEST_F(BrowsingDataRemoverTest, RemoveSafeBrowsingCookieForever) {
  RemoveSafeBrowsingCookieTester tester;

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_COOKIES, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  EXPECT_FALSE(tester.ContainsCookie());
}

TEST_F(BrowsingDataRemoverTest, RemoveSafeBrowsingCookieLastHour) {
  RemoveSafeBrowsingCookieTester tester;

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  BlockUntilBrowsingDataRemoved(browsing_data::LAST_HOUR,
                                BrowsingDataRemover::REMOVE_COOKIES, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  // Removing with time period other than ALL_TIME should not clear safe
  // browsing cookies.
  EXPECT_TRUE(tester.ContainsCookie());
}

TEST_F(BrowsingDataRemoverTest, RemoveSafeBrowsingCookieForeverWithPredicate) {
  RemoveSafeBrowsingCookieTester tester;

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());
  RegistrableDomainFilterBuilder filter(
      RegistrableDomainFilterBuilder::BLACKLIST);
  filter.AddRegisterableDomain(kTestRegisterableDomain1);
  BlockUntilOriginDataRemoved(browsing_data::ALL_TIME,
                              BrowsingDataRemover::REMOVE_COOKIES, filter);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  EXPECT_TRUE(tester.ContainsCookie());

  RegistrableDomainFilterBuilder filter2(
      RegistrableDomainFilterBuilder::WHITELIST);
  filter2.AddRegisterableDomain(kTestRegisterableDomain1);
  BlockUntilOriginDataRemoved(browsing_data::ALL_TIME,
                              BrowsingDataRemover::REMOVE_COOKIES, filter2);
  EXPECT_FALSE(tester.ContainsCookie());
}

TEST_F(BrowsingDataRemoverTest, RemoveChannelIDForever) {
  RemoveChannelIDTester tester(GetProfile());

  tester.AddChannelID(kTestOrigin1);
  EXPECT_EQ(0, tester.ssl_config_changed_count());
  EXPECT_EQ(1, tester.ChannelIDCount());

  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_CHANNEL_IDS, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_CHANNEL_IDS, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  EXPECT_EQ(1, tester.ssl_config_changed_count());
  EXPECT_EQ(0, tester.ChannelIDCount());
}

TEST_F(BrowsingDataRemoverTest, RemoveChannelIDLastHour) {
  RemoveChannelIDTester tester(GetProfile());

  base::Time now = base::Time::Now();
  tester.AddChannelID(kTestOrigin1);
  tester.AddChannelIDWithTimes(kTestOrigin2,
                               now - base::TimeDelta::FromHours(2));
  EXPECT_EQ(0, tester.ssl_config_changed_count());
  EXPECT_EQ(2, tester.ChannelIDCount());

  BlockUntilBrowsingDataRemoved(browsing_data::LAST_HOUR,
                                BrowsingDataRemover::REMOVE_CHANNEL_IDS, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_CHANNEL_IDS, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  EXPECT_EQ(1, tester.ssl_config_changed_count());
  ASSERT_EQ(1, tester.ChannelIDCount());
  net::ChannelIDStore::ChannelIDList channel_ids;
  tester.GetChannelIDList(&channel_ids);
  ASSERT_EQ(1U, channel_ids.size());
  EXPECT_EQ(kTestOrigin2, channel_ids.front().server_identifier());
}

TEST_F(BrowsingDataRemoverTest, RemoveChannelIDsForServerIdentifiers) {
  RemoveChannelIDTester tester(GetProfile());

  tester.AddChannelID(kTestRegisterableDomain1);
  tester.AddChannelID(kTestRegisterableDomain3);
  EXPECT_EQ(2, tester.ChannelIDCount());

  RegistrableDomainFilterBuilder filter_builder(
      RegistrableDomainFilterBuilder::WHITELIST);
  filter_builder.AddRegisterableDomain(kTestRegisterableDomain1);

  BlockUntilOriginDataRemoved(browsing_data::ALL_TIME,
                              BrowsingDataRemover::REMOVE_CHANNEL_IDS,
                              filter_builder);

  EXPECT_EQ(1, tester.ChannelIDCount());
  net::ChannelIDStore::ChannelIDList channel_ids;
  tester.GetChannelIDList(&channel_ids);
  EXPECT_EQ(kTestRegisterableDomain3, channel_ids.front().server_identifier());
}

TEST_F(BrowsingDataRemoverTest, RemoveUnprotectedLocalStorageForever) {
#if defined(ENABLE_EXTENSIONS)
  MockExtensionSpecialStoragePolicy* policy = CreateMockPolicy();
  // Protect kOrigin1.
  policy->AddProtected(kOrigin1.GetOrigin());
#endif

  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_LOCAL_STORAGE,
                                false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_LOCAL_STORAGE, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the data correctly.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());

  // Check origin matcher.
  EXPECT_EQ(ShouldRemoveForProtectedOriginOne(),
            removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOriginExt, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveProtectedLocalStorageForever) {
#if defined(ENABLE_EXTENSIONS)
  // Protect kOrigin1.
  MockExtensionSpecialStoragePolicy* policy = CreateMockPolicy();
  policy->AddProtected(kOrigin1.GetOrigin());
#endif

  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_LOCAL_STORAGE,
                                true);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_LOCAL_STORAGE, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB |
            BrowsingDataHelper::PROTECTED_WEB, GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the data correctly.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());

  // Check origin matcher all http origin will match since we specified
  // both protected and unprotected.
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOriginExt, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveLocalStorageForLastWeek) {
#if defined(ENABLE_EXTENSIONS)
  CreateMockPolicy();
#endif

  BlockUntilBrowsingDataRemoved(browsing_data::LAST_WEEK,
                                BrowsingDataRemover::REMOVE_LOCAL_STORAGE,
                                false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_LOCAL_STORAGE, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the data correctly.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE);
  // Persistent storage won't be deleted.
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT);
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());

  // Check origin matcher.
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOriginExt, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveHistoryForever) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));

  tester.AddHistory(kOrigin1, base::Time::Now());
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));

  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_HISTORY, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  EXPECT_FALSE(tester.HistoryContainsURL(kOrigin1));
}

TEST_F(BrowsingDataRemoverTest, RemoveHistoryForLastHour) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  tester.AddHistory(kOrigin1, base::Time::Now());
  tester.AddHistory(kOrigin2, two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin2));

  BlockUntilBrowsingDataRemoved(browsing_data::LAST_HOUR,
                                BrowsingDataRemover::REMOVE_HISTORY, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  EXPECT_FALSE(tester.HistoryContainsURL(kOrigin1));
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin2));
}

// This should crash (DCHECK) in Debug, but death tests don't work properly
// here.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
TEST_F(BrowsingDataRemoverTest, RemoveHistoryProhibited) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowDeletingBrowserHistory, false);

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  tester.AddHistory(kOrigin1, base::Time::Now());
  tester.AddHistory(kOrigin2, two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin2));

  BlockUntilBrowsingDataRemoved(browsing_data::LAST_HOUR,
                                BrowsingDataRemover::REMOVE_HISTORY, false);
  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Nothing should have been deleted.
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin1));
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin2));
}
#endif

TEST_F(BrowsingDataRemoverTest, RemoveMultipleTypes) {
  // Add some history.
  RemoveHistoryTester history_tester;
  ASSERT_TRUE(history_tester.Init(GetProfile()));
  history_tester.AddHistory(kOrigin1, base::Time::Now());
  ASSERT_TRUE(history_tester.HistoryContainsURL(kOrigin1));

  int removal_mask = BrowsingDataRemover::REMOVE_HISTORY |
                     BrowsingDataRemover::REMOVE_COOKIES;

  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME, removal_mask, false);

  EXPECT_EQ(removal_mask, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  EXPECT_FALSE(history_tester.HistoryContainsURL(kOrigin1));

  // The cookie would be deleted throught the StorageParition, check if the
  // partition was requested to remove cookie.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_COOKIES);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
}

// This should crash (DCHECK) in Debug, but death tests don't work properly
// here.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
TEST_F(BrowsingDataRemoverTest, RemoveMultipleTypesHistoryProhibited) {
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowDeletingBrowserHistory, false);

  // Add some history.
  RemoveHistoryTester history_tester;
  ASSERT_TRUE(history_tester.Init(GetProfile()));
  history_tester.AddHistory(kOrigin1, base::Time::Now());
  ASSERT_TRUE(history_tester.HistoryContainsURL(kOrigin1));

  int removal_mask = BrowsingDataRemover::REMOVE_HISTORY |
                     BrowsingDataRemover::REMOVE_COOKIES;

  BlockUntilBrowsingDataRemoved(browsing_data::LAST_HOUR, removal_mask, false);
  EXPECT_EQ(removal_mask, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // 1/2. History should remain.
  EXPECT_TRUE(history_tester.HistoryContainsURL(kOrigin1));

  // 2/2. The cookie(s) would be deleted throught the StorageParition, check if
  // the partition was requested to remove cookie.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_COOKIES);
  // Persistent storage won't be deleted, since ALL_TIME was not specified.
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT);
}
#endif

// Test that clearing history deletes favicons not associated with bookmarks.
TEST_F(BrowsingDataRemoverTest, RemoveFaviconsForever) {
  GURL page_url("http://a");

  RemoveFaviconTester favicon_tester;
  ASSERT_TRUE(favicon_tester.Init(GetProfile()));
  favicon_tester.VisitAndAddFavicon(page_url);
  ASSERT_TRUE(favicon_tester.HasFaviconForPageURL(page_url));

  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_HISTORY, false);
  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_FALSE(favicon_tester.HasFaviconForPageURL(page_url));
}

// Test that a bookmark's favicon is expired and not deleted when clearing
// history. Expiring the favicon causes the bookmark's favicon to be updated
// when the user next visits the bookmarked page. Expiring the bookmark's
// favicon is useful when the bookmark's favicon becomes incorrect (See
// crbug.com/474421 for a sample bug which causes this).
TEST_F(BrowsingDataRemoverTest, ExpireBookmarkFavicons) {
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

  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_HISTORY, false);
  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_TRUE(favicon_tester.HasExpiredFaviconForPageURL(bookmarked_page));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverBoth) {
  BlockUntilBrowsingDataRemoved(
      browsing_data::ALL_TIME,
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
          BrowsingDataRemover::REMOVE_WEBSQL |
          BrowsingDataRemover::REMOVE_APPCACHE |
          BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
          BrowsingDataRemover::REMOVE_CACHE_STORAGE |
          BrowsingDataRemover::REMOVE_INDEXEDDB,
      false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                BrowsingDataRemover::REMOVE_WEBSQL |
                BrowsingDataRemover::REMOVE_APPCACHE |
                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                BrowsingDataRemover::REMOVE_CACHE_STORAGE |
                BrowsingDataRemover::REMOVE_INDEXEDDB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverOnlyTemporary) {
#if defined(ENABLE_EXTENSIONS)
  CreateMockPolicy();
#endif

  BlockUntilBrowsingDataRemoved(
      browsing_data::ALL_TIME,
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
          BrowsingDataRemover::REMOVE_WEBSQL |
          BrowsingDataRemover::REMOVE_APPCACHE |
          BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
          BrowsingDataRemover::REMOVE_CACHE_STORAGE |
          BrowsingDataRemover::REMOVE_INDEXEDDB,
      false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                BrowsingDataRemover::REMOVE_WEBSQL |
                BrowsingDataRemover::REMOVE_APPCACHE |
                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                BrowsingDataRemover::REMOVE_CACHE_STORAGE |
                BrowsingDataRemover::REMOVE_INDEXEDDB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);

  // Check that all related origin data would be removed, that is, origin
  // matcher would match these origin.
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverOnlyPersistent) {
#if defined(ENABLE_EXTENSIONS)
  CreateMockPolicy();
#endif

  BlockUntilBrowsingDataRemoved(
      browsing_data::ALL_TIME,
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
          BrowsingDataRemover::REMOVE_WEBSQL |
          BrowsingDataRemover::REMOVE_APPCACHE |
          BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
          BrowsingDataRemover::REMOVE_CACHE_STORAGE |
          BrowsingDataRemover::REMOVE_INDEXEDDB,
      false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                BrowsingDataRemover::REMOVE_WEBSQL |
                BrowsingDataRemover::REMOVE_APPCACHE |
                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                BrowsingDataRemover::REMOVE_CACHE_STORAGE |
                BrowsingDataRemover::REMOVE_INDEXEDDB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);

  // Check that all related origin data would be removed, that is, origin
  // matcher would match these origin.
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverNeither) {
#if defined(ENABLE_EXTENSIONS)
  CreateMockPolicy();
#endif

  BlockUntilBrowsingDataRemoved(
      browsing_data::ALL_TIME,
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
          BrowsingDataRemover::REMOVE_WEBSQL |
          BrowsingDataRemover::REMOVE_APPCACHE |
          BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
          BrowsingDataRemover::REMOVE_CACHE_STORAGE |
          BrowsingDataRemover::REMOVE_INDEXEDDB,
      false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                BrowsingDataRemover::REMOVE_WEBSQL |
                BrowsingDataRemover::REMOVE_APPCACHE |
                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                BrowsingDataRemover::REMOVE_CACHE_STORAGE |
                BrowsingDataRemover::REMOVE_INDEXEDDB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);

  // Check that all related origin data would be removed, that is, origin
  // matcher would match these origin.
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverSpecificOrigin) {
  RegistrableDomainFilterBuilder builder(
      RegistrableDomainFilterBuilder::WHITELIST);
  builder.AddRegisterableDomain(kTestRegisterableDomain1);
  // Remove Origin 1.
  BlockUntilOriginDataRemoved(browsing_data::ALL_TIME,
                              BrowsingDataRemover::REMOVE_APPCACHE |
                                  BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                                  BrowsingDataRemover::REMOVE_CACHE_STORAGE |
                                  BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                                  BrowsingDataRemover::REMOVE_INDEXEDDB |
                                  BrowsingDataRemover::REMOVE_WEBSQL,
                              builder);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_APPCACHE |
                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                BrowsingDataRemover::REMOVE_CACHE_STORAGE |
                BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                BrowsingDataRemover::REMOVE_INDEXEDDB |
                BrowsingDataRemover::REMOVE_WEBSQL,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOrigin4, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForLastHour) {
  BlockUntilBrowsingDataRemoved(
      browsing_data::LAST_HOUR,
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
          BrowsingDataRemover::REMOVE_WEBSQL |
          BrowsingDataRemover::REMOVE_APPCACHE |
          BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
          BrowsingDataRemover::REMOVE_CACHE_STORAGE |
          BrowsingDataRemover::REMOVE_INDEXEDDB,
      false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                BrowsingDataRemover::REMOVE_WEBSQL |
                BrowsingDataRemover::REMOVE_APPCACHE |
                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                BrowsingDataRemover::REMOVE_CACHE_STORAGE |
                BrowsingDataRemover::REMOVE_INDEXEDDB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);

  // Persistent data would be left out since we are not removing from
  // beginning of time.
  uint32_t expected_quota_mask =
      ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT;
  EXPECT_EQ(removal_data.quota_storage_remove_mask, expected_quota_mask);
  // Check removal begin time.
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForLastWeek) {
  BlockUntilBrowsingDataRemoved(
      browsing_data::LAST_WEEK,
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
          BrowsingDataRemover::REMOVE_WEBSQL |
          BrowsingDataRemover::REMOVE_APPCACHE |
          BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
          BrowsingDataRemover::REMOVE_CACHE_STORAGE |
          BrowsingDataRemover::REMOVE_INDEXEDDB,
      false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                BrowsingDataRemover::REMOVE_WEBSQL |
                BrowsingDataRemover::REMOVE_APPCACHE |
                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                BrowsingDataRemover::REMOVE_CACHE_STORAGE |
                BrowsingDataRemover::REMOVE_INDEXEDDB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);

  // Persistent data would be left out since we are not removing from
  // beginning of time.
  uint32_t expected_quota_mask =
      ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT;
  EXPECT_EQ(removal_data.quota_storage_remove_mask, expected_quota_mask);
  // Check removal begin time.
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedUnprotectedOrigins) {
#if defined(ENABLE_EXTENSIONS)
  MockExtensionSpecialStoragePolicy* policy = CreateMockPolicy();
  // Protect kOrigin1.
  policy->AddProtected(kOrigin1.GetOrigin());
#endif

  BlockUntilBrowsingDataRemoved(
      browsing_data::ALL_TIME,
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
          BrowsingDataRemover::REMOVE_WEBSQL |
          BrowsingDataRemover::REMOVE_APPCACHE |
          BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
          BrowsingDataRemover::REMOVE_CACHE_STORAGE |
          BrowsingDataRemover::REMOVE_INDEXEDDB,
      false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                BrowsingDataRemover::REMOVE_WEBSQL |
                BrowsingDataRemover::REMOVE_APPCACHE |
                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                BrowsingDataRemover::REMOVE_CACHE_STORAGE |
                BrowsingDataRemover::REMOVE_INDEXEDDB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);

  // Check OriginMatcherFunction.
  EXPECT_EQ(ShouldRemoveForProtectedOriginOne(),
            removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedProtectedSpecificOrigin) {
#if defined(ENABLE_EXTENSIONS)
  MockExtensionSpecialStoragePolicy* policy = CreateMockPolicy();
  // Protect kOrigin1.
  policy->AddProtected(kOrigin1.GetOrigin());
#endif

  RegistrableDomainFilterBuilder builder(
      RegistrableDomainFilterBuilder::WHITELIST);
  builder.AddRegisterableDomain(kTestRegisterableDomain1);

  // Try to remove kOrigin1. Expect failure.
  BlockUntilOriginDataRemoved(browsing_data::ALL_TIME,
                              BrowsingDataRemover::REMOVE_APPCACHE |
                                  BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                                  BrowsingDataRemover::REMOVE_CACHE_STORAGE |
                                  BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                                  BrowsingDataRemover::REMOVE_INDEXEDDB |
                                  BrowsingDataRemover::REMOVE_WEBSQL,
                              builder);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_APPCACHE |
                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                BrowsingDataRemover::REMOVE_CACHE_STORAGE |
                BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                BrowsingDataRemover::REMOVE_INDEXEDDB |
                BrowsingDataRemover::REMOVE_WEBSQL,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);

  // Check OriginMatcherFunction.
  EXPECT_EQ(ShouldRemoveForProtectedOriginOne(),
            removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  // Since we use the matcher function to validate origins now, this should
  // return false for the origins we're not trying to clear.
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedProtectedOrigins) {
#if defined(ENABLE_EXTENSIONS)
  MockExtensionSpecialStoragePolicy* policy = CreateMockPolicy();
  // Protect kOrigin1.
  policy->AddProtected(kOrigin1.GetOrigin());
#endif

  // Try to remove kOrigin1. Expect success.
  BlockUntilBrowsingDataRemoved(
      browsing_data::ALL_TIME,
      BrowsingDataRemover::REMOVE_APPCACHE |
          BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
          BrowsingDataRemover::REMOVE_CACHE_STORAGE |
          BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
          BrowsingDataRemover::REMOVE_INDEXEDDB |
          BrowsingDataRemover::REMOVE_WEBSQL,
      true);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_APPCACHE |
                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                BrowsingDataRemover::REMOVE_CACHE_STORAGE |
                BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                BrowsingDataRemover::REMOVE_INDEXEDDB |
                BrowsingDataRemover::REMOVE_WEBSQL,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::PROTECTED_WEB |
      BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);

  // Check OriginMatcherFunction, |kOrigin1| would match mask since we
  // would have 'protected' specified in origin_type_mask.
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedIgnoreExtensionsAndDevTools) {
#if defined(ENABLE_EXTENSIONS)
  CreateMockPolicy();
#endif

  BlockUntilBrowsingDataRemoved(
      browsing_data::ALL_TIME,
      BrowsingDataRemover::REMOVE_APPCACHE |
          BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
          BrowsingDataRemover::REMOVE_CACHE_STORAGE |
          BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
          BrowsingDataRemover::REMOVE_INDEXEDDB |
          BrowsingDataRemover::REMOVE_WEBSQL,
      false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_APPCACHE |
                BrowsingDataRemover::REMOVE_SERVICE_WORKERS |
                BrowsingDataRemover::REMOVE_CACHE_STORAGE |
                BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                BrowsingDataRemover::REMOVE_INDEXEDDB |
                BrowsingDataRemover::REMOVE_WEBSQL,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);

  // Check that extension and devtools data wouldn't be removed, that is,
  // origin matcher would not match these origin.
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOriginExt, mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOriginDevTools, mock_policy()));
}

TEST_F(BrowsingDataRemoverTest, TimeBasedHistoryRemoval) {
  RemoveHistoryTester tester;
  ASSERT_TRUE(tester.Init(GetProfile()));

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  tester.AddHistory(kOrigin1, base::Time::Now());
  tester.AddHistory(kOrigin2, two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin2));

  RegistrableDomainFilterBuilder builder(
      RegistrableDomainFilterBuilder::BLACKLIST);
  BlockUntilOriginDataRemoved(browsing_data::LAST_HOUR,
                              BrowsingDataRemover::REMOVE_HISTORY, builder);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  EXPECT_FALSE(tester.HistoryContainsURL(kOrigin1));
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin2));
}

// Verify that clearing autofill form data works.
TEST_F(BrowsingDataRemoverTest, AutofillRemovalLastHour) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());

  ASSERT_FALSE(tester.HasProfile());
  tester.AddProfilesAndCards();
  ASSERT_TRUE(tester.HasProfile());

  BlockUntilBrowsingDataRemoved(browsing_data::LAST_HOUR,
                                BrowsingDataRemover::REMOVE_FORM_DATA, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FORM_DATA, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  ASSERT_FALSE(tester.HasProfile());
}

TEST_F(BrowsingDataRemoverTest, AutofillRemovalEverything) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());

  ASSERT_FALSE(tester.HasProfile());
  tester.AddProfilesAndCards();
  ASSERT_TRUE(tester.HasProfile());

  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_FORM_DATA, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FORM_DATA, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  ASSERT_FALSE(tester.HasProfile());
}

// Verify that clearing autofill form data works.
TEST_F(BrowsingDataRemoverTest, AutofillOriginsRemovedWithHistory) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());

  tester.AddProfilesAndCards();
  EXPECT_FALSE(tester.HasOrigin(std::string()));
  EXPECT_TRUE(tester.HasOrigin(kWebOrigin));
  EXPECT_TRUE(tester.HasOrigin(autofill::kSettingsOrigin));

  BlockUntilBrowsingDataRemoved(browsing_data::LAST_HOUR,
                                BrowsingDataRemover::REMOVE_HISTORY, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
  EXPECT_TRUE(tester.HasOrigin(std::string()));
  EXPECT_FALSE(tester.HasOrigin(kWebOrigin));
  EXPECT_TRUE(tester.HasOrigin(autofill::kSettingsOrigin));
}

class InspectableCompletionObserver
    : public BrowsingDataRemoverCompletionObserver {
 public:
  explicit InspectableCompletionObserver(BrowsingDataRemover* remover)
      : BrowsingDataRemoverCompletionObserver(remover) {}
  ~InspectableCompletionObserver() override {}

  bool called() { return called_; }

 protected:
  void OnBrowsingDataRemoverDone() override {
    BrowsingDataRemoverCompletionObserver::OnBrowsingDataRemoverDone();
    called_ = true;
  }

 private:
  bool called_ = false;
};

TEST_F(BrowsingDataRemoverTest, CompletionInhibition) {
  // The |completion_inhibitor| on the stack should prevent removal sessions
  // from completing until after ContinueToCompletion() is called.
  BrowsingDataRemoverCompletionInhibitor completion_inhibitor;

  BrowsingDataRemover* remover =
      BrowsingDataRemoverFactory::GetForBrowserContext(GetProfile());
  InspectableCompletionObserver completion_observer(remover);
  remover->RemoveAndReply(BrowsingDataRemover::Unbounded(),
                          BrowsingDataRemover::REMOVE_HISTORY,
                          BrowsingDataHelper::UNPROTECTED_WEB,
                          &completion_observer);

  // Process messages until the inhibitor is notified, and then some, to make
  // sure we do not complete asynchronously before ContinueToCompletion() is
  // called.
  completion_inhibitor.BlockUntilNearCompletion();
  base::RunLoop().RunUntilIdle();

  // Verify that the removal has not yet been completed and the observer has
  // not been called.
  EXPECT_TRUE(remover->is_removing());
  EXPECT_FALSE(completion_observer.called());

  // Now run the removal process until completion, and verify that observers are
  // now notified, and the notifications is sent out.
  completion_inhibitor.ContinueToCompletion();
  completion_observer.BlockUntilCompletion();

  EXPECT_FALSE(remover->is_removing());
  EXPECT_TRUE(completion_observer.called());
}

TEST_F(BrowsingDataRemoverTest, EarlyShutdown) {
  BrowsingDataRemover* remover =
      BrowsingDataRemoverFactory::GetForBrowserContext(GetProfile());
  InspectableCompletionObserver completion_observer(remover);
  BrowsingDataRemoverCompletionInhibitor completion_inhibitor;
  remover->RemoveAndReply(BrowsingDataRemover::Unbounded(),
                          BrowsingDataRemover::REMOVE_HISTORY,
                          BrowsingDataHelper::UNPROTECTED_WEB,
                          &completion_observer);

  completion_inhibitor.BlockUntilNearCompletion();

  // Verify that the deletion has not yet been completed and the observer has
  // not been called.
  EXPECT_TRUE(remover->is_removing());
  EXPECT_FALSE(completion_observer.called());

  // Destroying the profile should trigger the notification.
  DestroyProfile();

  EXPECT_TRUE(completion_observer.called());

  // Finishing after shutdown shouldn't break anything.
  completion_inhibitor.ContinueToCompletion();
  completion_observer.BlockUntilCompletion();
}

TEST_F(BrowsingDataRemoverTest, ZeroSuggestCacheClear) {
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults,
                   "[\"\", [\"foo\", \"bar\"]]");
  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_COOKIES, false);

  // Expect the prefs to be cleared when cookies are removed.
  EXPECT_TRUE(prefs->GetString(omnibox::kZeroSuggestCachedResults).empty());
  EXPECT_EQ(BrowsingDataRemover::REMOVE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());
}

#if defined(OS_CHROMEOS)
TEST_F(BrowsingDataRemoverTest, ContentProtectionPlatformKeysRemoval) {
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service;
  chromeos::ScopedTestCrosSettings test_cros_settings;
  chromeos::MockUserManager* mock_user_manager =
      new testing::NiceMock<chromeos::MockUserManager>();
  mock_user_manager->SetActiveUser(
      AccountId::FromUserEmail("test@example.com"));
  chromeos::ScopedUserManagerEnabler user_manager_enabler(mock_user_manager);

  std::unique_ptr<chromeos::DBusThreadManagerSetter> dbus_setter =
      chromeos::DBusThreadManager::GetSetterForTesting();
  chromeos::MockCryptohomeClient* cryptohome_client =
      new chromeos::MockCryptohomeClient;
  dbus_setter->SetCryptohomeClient(
      std::unique_ptr<chromeos::CryptohomeClient>(cryptohome_client));

  // Expect exactly one call.  No calls means no attempt to delete keys and more
  // than one call means a significant performance problem.
  EXPECT_CALL(*cryptohome_client, TpmAttestationDeleteKeys(_, _, _, _))
      .WillOnce(WithArgs<3>(Invoke(FakeDBusCall)));

  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_MEDIA_LICENSES,
                                false);

  chromeos::DBusThreadManager::Shutdown();
}
#endif

TEST_F(BrowsingDataRemoverTest, DomainReliability_Null) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  EXPECT_EQ(0u, tester.clear_count());
}

TEST_F(BrowsingDataRemoverTest, DomainReliability_Beacons) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_HISTORY, false);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(CLEAR_BEACONS, tester.last_clear_mode());
  EXPECT_TRUE(ProbablySameFilters(
      BrowsingDataFilterBuilder::BuildNoopFilter(), tester.last_filter()));
}

TEST_F(BrowsingDataRemoverTest, DomainReliability_Beacons_WithFilter) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  RegistrableDomainFilterBuilder builder(
      RegistrableDomainFilterBuilder::WHITELIST);
  builder.AddRegisterableDomain(kTestRegisterableDomain1);

  BlockUntilOriginDataRemoved(browsing_data::ALL_TIME,
                              BrowsingDataRemover::REMOVE_HISTORY, builder);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(CLEAR_BEACONS, tester.last_clear_mode());
  EXPECT_TRUE(ProbablySameFilters(
      builder.BuildGeneralFilter(), tester.last_filter()));
}

TEST_F(BrowsingDataRemoverTest, DomainReliability_Contexts) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_COOKIES, false);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(CLEAR_CONTEXTS, tester.last_clear_mode());
  EXPECT_TRUE(ProbablySameFilters(
      BrowsingDataFilterBuilder::BuildNoopFilter(), tester.last_filter()));
}

TEST_F(BrowsingDataRemoverTest, DomainReliability_Contexts_WithFilter) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  RegistrableDomainFilterBuilder builder(
      RegistrableDomainFilterBuilder::WHITELIST);
  builder.AddRegisterableDomain(kTestRegisterableDomain1);

  BlockUntilOriginDataRemoved(browsing_data::ALL_TIME,
                              BrowsingDataRemover::REMOVE_COOKIES, builder);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(CLEAR_CONTEXTS, tester.last_clear_mode());
  EXPECT_TRUE(ProbablySameFilters(
      builder.BuildGeneralFilter(), tester.last_filter()));
}

TEST_F(BrowsingDataRemoverTest, DomainReliability_ContextsWin) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  BlockUntilBrowsingDataRemoved(
      browsing_data::ALL_TIME,
      BrowsingDataRemover::REMOVE_HISTORY | BrowsingDataRemover::REMOVE_COOKIES,
      false);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(CLEAR_CONTEXTS, tester.last_clear_mode());
}

TEST_F(BrowsingDataRemoverTest, DomainReliability_ProtectedOrigins) {
  const ClearDomainReliabilityTester& tester =
      clear_domain_reliability_tester();

  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_COOKIES, true);
  EXPECT_EQ(1u, tester.clear_count());
  EXPECT_EQ(CLEAR_CONTEXTS, tester.last_clear_mode());
}

// TODO(juliatuttle): This isn't actually testing the no-monitor case, since
// BrowsingDataRemoverTest now creates one unconditionally, since it's needed
// for some unrelated test cases. This should be fixed so it tests the no-
// monitor case again.
TEST_F(BrowsingDataRemoverTest, DISABLED_DomainReliability_NoMonitor) {
  BlockUntilBrowsingDataRemoved(
      browsing_data::ALL_TIME,
      BrowsingDataRemover::REMOVE_HISTORY | BrowsingDataRemover::REMOVE_COOKIES,
      false);
}

TEST_F(BrowsingDataRemoverTest, RemoveDownloadsByTimeOnly) {
  RemoveDownloadsTester tester(GetProfile());
  base::Callback<bool(const GURL&)> filter =
      BrowsingDataFilterBuilder::BuildNoopFilter();

  EXPECT_CALL(
      *tester.download_manager(),
      RemoveDownloadsByURLAndTime(ProbablySameFilter(filter), _, _));

  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_DOWNLOADS, false);
}

TEST_F(BrowsingDataRemoverTest, RemoveDownloadsByOrigin) {
  RemoveDownloadsTester tester(GetProfile());
  RegistrableDomainFilterBuilder builder(
      RegistrableDomainFilterBuilder::WHITELIST);
  builder.AddRegisterableDomain(kTestRegisterableDomain1);
  base::Callback<bool(const GURL&)> filter = builder.BuildGeneralFilter();

  EXPECT_CALL(
      *tester.download_manager(),
      RemoveDownloadsByURLAndTime(ProbablySameFilter(filter), _, _));

  BlockUntilOriginDataRemoved(browsing_data::ALL_TIME,
                              BrowsingDataRemover::REMOVE_DOWNLOADS, builder);
}

TEST_F(BrowsingDataRemoverTest, RemovePasswordStatistics) {
  RemovePasswordsTester tester(GetProfile());

  EXPECT_CALL(*tester.store(), RemoveStatisticsCreatedBetweenImpl(
                                   base::Time(), base::Time::Max()));
  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_HISTORY, false);
}

TEST_F(BrowsingDataRemoverTest, RemovePasswordsByTimeOnly) {
  RemovePasswordsTester tester(GetProfile());
  base::Callback<bool(const GURL&)> filter =
      BrowsingDataFilterBuilder::BuildNoopFilter();

  EXPECT_CALL(*tester.store(),
              RemoveLoginsByURLAndTimeImpl(ProbablySameFilter(filter), _, _))
      .WillOnce(Return(password_manager::PasswordStoreChangeList()));
  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_PASSWORDS, false);
}

TEST_F(BrowsingDataRemoverTest, RemovePasswordsByOrigin) {
  RemovePasswordsTester tester(GetProfile());
  RegistrableDomainFilterBuilder builder(
      RegistrableDomainFilterBuilder::WHITELIST);
  builder.AddRegisterableDomain(kTestRegisterableDomain1);
  base::Callback<bool(const GURL&)> filter = builder.BuildGeneralFilter();

  EXPECT_CALL(*tester.store(),
              RemoveLoginsByURLAndTimeImpl(ProbablySameFilter(filter), _, _))
      .WillOnce(Return(password_manager::PasswordStoreChangeList()));
  BlockUntilOriginDataRemoved(browsing_data::ALL_TIME,
                              BrowsingDataRemover::REMOVE_PASSWORDS, builder);
}

TEST_F(BrowsingDataRemoverTest, DisableAutoSignIn) {
  RemovePasswordsTester tester(GetProfile());
  base::Callback<bool(const GURL&)> empty_filter =
      BrowsingDataFilterBuilder::BuildNoopFilter();

  EXPECT_CALL(
      *tester.store(),
      DisableAutoSignInForOriginsImpl(ProbablySameFilter(empty_filter)))
      .WillOnce(Return(password_manager::PasswordStoreChangeList()));

  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_COOKIES, false);
}

TEST_F(BrowsingDataRemoverTest, DisableAutoSignInAfterRemovingPasswords) {
  RemovePasswordsTester tester(GetProfile());
  base::Callback<bool(const GURL&)> empty_filter =
      BrowsingDataFilterBuilder::BuildNoopFilter();

  EXPECT_CALL(*tester.store(), RemoveLoginsByURLAndTimeImpl(_, _, _))
      .WillOnce(Return(password_manager::PasswordStoreChangeList()));
  EXPECT_CALL(
      *tester.store(),
      DisableAutoSignInForOriginsImpl(ProbablySameFilter(empty_filter)))
      .WillOnce(Return(password_manager::PasswordStoreChangeList()));

  BlockUntilBrowsingDataRemoved(browsing_data::ALL_TIME,
                                BrowsingDataRemover::REMOVE_COOKIES |
                                    BrowsingDataRemover::REMOVE_PASSWORDS,
                                false);
}

TEST_F(BrowsingDataRemoverTest, RemoveContentSettingsWithBlacklist) {
  // Add our settings.
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      kOrigin1, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
      base::WrapUnique(new base::DictionaryValue()));
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      kOrigin2, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
      base::WrapUnique(new base::DictionaryValue()));
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      kOrigin3, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
      base::WrapUnique(new base::DictionaryValue()));
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      kOrigin4, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
      base::WrapUnique(new base::DictionaryValue()));

  // Clear all except for origin1 and origin3.
  RegistrableDomainFilterBuilder filter(
      RegistrableDomainFilterBuilder::BLACKLIST);
  filter.AddRegisterableDomain(kTestRegisterableDomain1);
  filter.AddRegisterableDomain(kTestRegisterableDomain3);
  BlockUntilOriginDataRemoved(browsing_data::LAST_HOUR,
                              BrowsingDataRemover::REMOVE_SITE_USAGE_DATA,
                              filter);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_SITE_USAGE_DATA, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginTypeMask());

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

TEST_F(BrowsingDataRemoverTest, ClearWithPredicate) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  ContentSettingsForOneType host_settings;

  // Patterns with wildcards.
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.org");
  ContentSettingsPattern pattern2 =
      ContentSettingsPattern::FromString("[*.]example.net");

  // Patterns without wildcards.
  GURL url1("https://www.google.com/");
  GURL url2("https://www.google.com/maps");
  GURL url3("http://www.google.com/maps");
  GURL url3_origin_only("http://www.google.com/");

  host_content_settings_map->SetContentSettingCustomScope(
      pattern2, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSettingCustomScope(
      pattern, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetWebsiteSettingCustomScope(
      pattern2, ContentSettingsPattern::Wildcard(),
      CONTENT_SETTINGS_TYPE_APP_BANNER, std::string(),
      base::WrapUnique(new base::DictionaryValue()));

  // First, test that we clear only COOKIES (not APP_BANNER), and pattern2.
  BrowsingDataRemover::ClearSettingsForOneTypeWithPredicate(
      host_content_settings_map, CONTENT_SETTINGS_TYPE_COOKIES,
      base::Bind(&MatchPrimaryPattern, pattern2));
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_COOKIES, std::string(), &host_settings);
  // |host_settings| contains default & block.
  EXPECT_EQ(2U, host_settings.size());
  EXPECT_EQ(pattern, host_settings[0].primary_pattern);
  EXPECT_EQ("*", host_settings[0].secondary_pattern.ToString());
  EXPECT_EQ("*", host_settings[1].primary_pattern.ToString());
  EXPECT_EQ("*", host_settings[1].secondary_pattern.ToString());

  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_APP_BANNER, std::string(), &host_settings);
  // |host_settings| contains block.
  EXPECT_EQ(1U, host_settings.size());
  EXPECT_EQ(pattern2, host_settings[0].primary_pattern);
  EXPECT_EQ("*", host_settings[0].secondary_pattern.ToString());

  // Next, test that we do correct pattern matching w/ an origin policy item.
  // We verify that we have no settings stored.
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(), &host_settings);
  EXPECT_EQ(0u, host_settings.size());
  // Add settings.
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      url1, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
      base::WrapUnique(new base::DictionaryValue()));
  // This setting should override the one above, as it's the same origin.
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      url2, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
      base::WrapUnique(new base::DictionaryValue()));
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      url3, GURL(), CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
      base::WrapUnique(new base::DictionaryValue()));
  // Verify we only have two.
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(), &host_settings);
  EXPECT_EQ(2u, host_settings.size());

  // Clear the http one, which we should be able to do w/ the origin only, as
  // the scope of CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT is
  // REQUESTING_ORIGIN_ONLY_SCOPE.
  ContentSettingsPattern http_pattern =
      ContentSettingsPattern::FromURLNoWildcard(url3_origin_only);
  BrowsingDataRemover::ClearSettingsForOneTypeWithPredicate(
      host_content_settings_map, CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT,
      base::Bind(&MatchPrimaryPattern, http_pattern));
  // Verify we only have one, and it's url1.
  host_content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(), &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(url1),
            host_settings[0].primary_pattern);
}

TEST_F(BrowsingDataRemoverTest, ClearPermissionPromptCounts) {
  RemovePermissionPromptCountsTest tester(GetProfile());

  RegistrableDomainFilterBuilder filter_builder_1(
      RegistrableDomainFilterBuilder::WHITELIST);
  filter_builder_1.AddRegisterableDomain(kTestRegisterableDomain1);

  RegistrableDomainFilterBuilder filter_builder_2(
      RegistrableDomainFilterBuilder::BLACKLIST);
  filter_builder_2.AddRegisterableDomain(kTestRegisterableDomain1);

  {
    // Test REMOVE_HISTORY.
    EXPECT_EQ(1, tester.RecordIgnore(kOrigin1,
                                     content::PermissionType::GEOLOCATION));
    EXPECT_EQ(2, tester.RecordIgnore(kOrigin1,
                                     content::PermissionType::GEOLOCATION));
    EXPECT_EQ(1, tester.RecordIgnore(kOrigin1,
                                     content::PermissionType::NOTIFICATIONS));
    tester.ShouldChangeDismissalToBlock(kOrigin1,
                                        content::PermissionType::MIDI_SYSEX);
    EXPECT_EQ(1, tester.RecordIgnore(kOrigin2,
                                     content::PermissionType::DURABLE_STORAGE));
    tester.ShouldChangeDismissalToBlock(kOrigin2,
                                        content::PermissionType::NOTIFICATIONS);

    BlockUntilOriginDataRemoved(browsing_data::LAST_HOUR,
                                BrowsingDataRemover::REMOVE_SITE_USAGE_DATA,
                                filter_builder_1);

    // kOrigin1 should be gone, but kOrigin2 remains.
    EXPECT_EQ(0, tester.GetIgnoreCount(kOrigin1,
                                       content::PermissionType::GEOLOCATION));
    EXPECT_EQ(0, tester.GetIgnoreCount(kOrigin1,
                                       content::PermissionType::NOTIFICATIONS));
    EXPECT_EQ(0, tester.GetDismissCount(kOrigin1,
                                        content::PermissionType::MIDI_SYSEX));
    EXPECT_EQ(1, tester.GetIgnoreCount(
                     kOrigin2, content::PermissionType::DURABLE_STORAGE));
    EXPECT_EQ(1, tester.GetDismissCount(
                     kOrigin2, content::PermissionType::NOTIFICATIONS));

    BlockUntilBrowsingDataRemoved(browsing_data::LAST_HOUR,
                                  BrowsingDataRemover::REMOVE_HISTORY, false);

    // Everything should be gone.
    EXPECT_EQ(0, tester.GetIgnoreCount(kOrigin1,
                                       content::PermissionType::GEOLOCATION));
    EXPECT_EQ(0, tester.GetIgnoreCount(kOrigin1,
                                       content::PermissionType::NOTIFICATIONS));
    EXPECT_EQ(0, tester.GetDismissCount(kOrigin1,
                                        content::PermissionType::MIDI_SYSEX));
    EXPECT_EQ(0, tester.GetIgnoreCount(
                     kOrigin2, content::PermissionType::DURABLE_STORAGE));
    EXPECT_EQ(0, tester.GetDismissCount(
                     kOrigin2, content::PermissionType::NOTIFICATIONS));
  }
  {
    // Test REMOVE_SITE_DATA.
    EXPECT_EQ(1, tester.RecordIgnore(kOrigin1,
                                     content::PermissionType::GEOLOCATION));
    EXPECT_EQ(2, tester.RecordIgnore(kOrigin1,
                                     content::PermissionType::GEOLOCATION));
    EXPECT_EQ(1, tester.RecordIgnore(kOrigin1,
                                     content::PermissionType::NOTIFICATIONS));
    tester.ShouldChangeDismissalToBlock(kOrigin1,
                                        content::PermissionType::MIDI_SYSEX);
    EXPECT_EQ(1, tester.RecordIgnore(kOrigin2,
                                     content::PermissionType::DURABLE_STORAGE));
    tester.ShouldChangeDismissalToBlock(kOrigin2,
                                        content::PermissionType::NOTIFICATIONS);

    BlockUntilOriginDataRemoved(browsing_data::LAST_HOUR,
                                BrowsingDataRemover::REMOVE_SITE_USAGE_DATA,
                                filter_builder_2);

    // kOrigin2 should be gone, but kOrigin1 remains.
    EXPECT_EQ(2, tester.GetIgnoreCount(kOrigin1,
                                       content::PermissionType::GEOLOCATION));
    EXPECT_EQ(1, tester.GetIgnoreCount(kOrigin1,
                                       content::PermissionType::NOTIFICATIONS));
    EXPECT_EQ(1, tester.GetDismissCount(kOrigin1,
                                        content::PermissionType::MIDI_SYSEX));
    EXPECT_EQ(0, tester.GetIgnoreCount(
                     kOrigin2, content::PermissionType::DURABLE_STORAGE));
    EXPECT_EQ(0, tester.GetDismissCount(
                     kOrigin2, content::PermissionType::NOTIFICATIONS));

    BlockUntilBrowsingDataRemoved(browsing_data::LAST_HOUR,
                                  BrowsingDataRemover::REMOVE_SITE_USAGE_DATA,
                                  false);

    // Everything should be gone.
    EXPECT_EQ(0, tester.GetIgnoreCount(kOrigin1,
                                       content::PermissionType::GEOLOCATION));
    EXPECT_EQ(0, tester.GetIgnoreCount(kOrigin1,
                                       content::PermissionType::NOTIFICATIONS));
    EXPECT_EQ(0, tester.GetDismissCount(kOrigin1,
                                        content::PermissionType::MIDI_SYSEX));
    EXPECT_EQ(0, tester.GetIgnoreCount(
                     kOrigin2, content::PermissionType::DURABLE_STORAGE));
    EXPECT_EQ(0, tester.GetDismissCount(
                     kOrigin2, content::PermissionType::NOTIFICATIONS));
  }
}

#if defined(ENABLE_PLUGINS)
TEST_F(BrowsingDataRemoverTest, RemovePluginData) {
  RemovePluginDataTester tester(GetProfile());

  tester.AddDomain(kOrigin1.host());
  tester.AddDomain(kOrigin2.host());
  tester.AddDomain(kOrigin3.host());

  std::vector<std::string> expected = {
      kOrigin1.host(), kOrigin2.host(), kOrigin3.host() };
  EXPECT_EQ(expected, tester.GetDomains());

  // Delete data with a filter for the registrable domain of |kOrigin3|.
  RegistrableDomainFilterBuilder filter_builder(
      RegistrableDomainFilterBuilder::WHITELIST);
  filter_builder.AddRegisterableDomain(kTestRegisterableDomain3);
  BlockUntilOriginDataRemoved(browsing_data::ALL_TIME,
                              BrowsingDataRemover::REMOVE_PLUGIN_DATA,
                              filter_builder);

  // Plugin data for |kOrigin3.host()| should have been removed.
  expected.pop_back();
  EXPECT_EQ(expected, tester.GetDomains());

  // TODO(msramek): Mock PluginDataRemover and test the complete deletion
  // of plugin data as well.
}
#endif

class MultipleTasksObserver {
 public:
  // A simple implementation of BrowsingDataRemover::Observer.
  // MultipleTasksObserver will use several instances of Target to test
  // that completion callbacks are returned to the correct one.
  class Target : public BrowsingDataRemover::Observer {
   public:
    Target(MultipleTasksObserver* parent, BrowsingDataRemover* remover)
        : parent_(parent),
          observer_(this) {
      observer_.Add(remover);
    }
    ~Target() override {}

    void OnBrowsingDataRemoverDone() override {
      parent_->SetLastCalledTarget(this);
    }

   private:
    MultipleTasksObserver* parent_;
    ScopedObserver<BrowsingDataRemover, BrowsingDataRemover::Observer>
        observer_;
  };

  explicit MultipleTasksObserver(BrowsingDataRemover* remover)
      : target_a_(this, remover),
        target_b_(this, remover),
        last_called_target_(nullptr) {}
  ~MultipleTasksObserver() {}

  void ClearLastCalledTarget() {
    last_called_target_ = nullptr;
  }

  Target* GetLastCalledTarget() {
    return last_called_target_;
  }

  Target* target_a() { return &target_a_; }
  Target* target_b() { return &target_b_; }

 private:
  void SetLastCalledTarget(Target* target) {
    DCHECK(!last_called_target_)
        << "Call ClearLastCalledTarget() before every removal task.";
    last_called_target_ = target;
  }

  Target target_a_;
  Target target_b_;
  Target* last_called_target_;
};

TEST_F(BrowsingDataRemoverTest, MultipleTasks) {
  BrowsingDataRemover* remover =
      BrowsingDataRemoverFactory::GetForBrowserContext(GetProfile());
  EXPECT_FALSE(remover->is_removing());

  std::unique_ptr<RegistrableDomainFilterBuilder> filter_builder_1(
      new RegistrableDomainFilterBuilder(
          RegistrableDomainFilterBuilder::WHITELIST));
  std::unique_ptr<RegistrableDomainFilterBuilder> filter_builder_2(
      new RegistrableDomainFilterBuilder(
          RegistrableDomainFilterBuilder::BLACKLIST));
  filter_builder_2->AddRegisterableDomain("example.com");

  MultipleTasksObserver observer(remover);
  BrowsingDataRemoverCompletionInhibitor completion_inhibitor;

  // Test several tasks with various configuration of masks, filters, and target
  // observers.
  std::list<BrowsingDataRemover::RemovalTask> tasks;
  tasks.emplace_back(
      BrowsingDataRemover::Unbounded(),
      BrowsingDataRemover::REMOVE_HISTORY,
      BrowsingDataHelper::UNPROTECTED_WEB,
      base::WrapUnique(new RegistrableDomainFilterBuilder(
          RegistrableDomainFilterBuilder::BLACKLIST)),
      observer.target_a());
  tasks.emplace_back(
      BrowsingDataRemover::Unbounded(),
      BrowsingDataRemover::REMOVE_COOKIES,
      BrowsingDataHelper::PROTECTED_WEB,
      base::WrapUnique(new RegistrableDomainFilterBuilder(
          RegistrableDomainFilterBuilder::BLACKLIST)),
      nullptr);
  tasks.emplace_back(
      BrowsingDataRemover::TimeRange(base::Time::Now(), base::Time::Max()),
      BrowsingDataRemover::REMOVE_PASSWORDS,
      BrowsingDataHelper::ALL,
      base::WrapUnique(new RegistrableDomainFilterBuilder(
          RegistrableDomainFilterBuilder::BLACKLIST)),
      observer.target_b());
  tasks.emplace_back(
      BrowsingDataRemover::TimeRange(base::Time(), base::Time::UnixEpoch()),
      BrowsingDataRemover::REMOVE_WEBSQL,
      BrowsingDataHelper::UNPROTECTED_WEB,
      std::move(filter_builder_1),
      observer.target_b());
  tasks.emplace_back(
      BrowsingDataRemover::TimeRange(
          base::Time::UnixEpoch(), base::Time::Now()),
      BrowsingDataRemover::REMOVE_CHANNEL_IDS,
      BrowsingDataHelper::ALL,
      std::move(filter_builder_2),
      nullptr);

  for (BrowsingDataRemover::RemovalTask& task : tasks) {
    // All tasks can be directly translated to a RemoveInternal() call. Since
    // that is a private method, we must call the four public versions of
    // Remove.* instead. This also serves as a test that those methods are all
    // correctly reduced to RemoveInternal().
    if (!task.observer && task.filter_builder->IsEmptyBlacklist()) {
      remover->Remove(task.time_range, task.remove_mask, task.origin_type_mask);
    } else if (task.filter_builder->IsEmptyBlacklist()) {
      remover->RemoveAndReply(task.time_range, task.remove_mask,
                              task.origin_type_mask, task.observer);
    } else if (!task.observer) {
      remover->RemoveWithFilter(task.time_range, task.remove_mask,
                                task.origin_type_mask,
                                std::move(task.filter_builder));
    } else {
      remover->RemoveWithFilterAndReply(task.time_range, task.remove_mask,
                                        task.origin_type_mask,
                                        std::move(task.filter_builder),
                                        task.observer);
    }
  }

  // Use the inhibitor to stop after every task and check the results.
  for (BrowsingDataRemover::RemovalTask& task : tasks) {
    EXPECT_TRUE(remover->is_removing());
    observer.ClearLastCalledTarget();

    // Finish the task execution synchronously.
    completion_inhibitor.BlockUntilNearCompletion();
    completion_inhibitor.ContinueToCompletion();

    // Observers, if any, should have been called by now (since we call
    // observers on the same thread).
    EXPECT_EQ(task.observer, observer.GetLastCalledTarget());

    // TODO(msramek): If BrowsingDataRemover took ownership of the last used
    // filter builder and exposed it, we could also test it here. Make it so.
    EXPECT_EQ(task.remove_mask, GetRemovalMask());
    EXPECT_EQ(task.origin_type_mask, GetOriginTypeMask());
    EXPECT_EQ(task.time_range.begin, GetBeginTime());
  }

  EXPECT_FALSE(remover->is_removing());
}

// The previous test, BrowsingDataRemoverTest.MultipleTasks, tests that the
// tasks are not mixed up and they are executed in a correct order. However,
// the completion inhibitor kept synchronizing the execution in order to verify
// the parameters. This test demonstrates that even running the tasks without
// inhibition is executed correctly and doesn't crash.
TEST_F(BrowsingDataRemoverTest, MultipleTasksInQuickSuccession) {
  BrowsingDataRemover* remover =
      BrowsingDataRemoverFactory::GetForBrowserContext(GetProfile());
  EXPECT_FALSE(remover->is_removing());

  int test_removal_masks[] = {
      BrowsingDataRemover::REMOVE_COOKIES,
      BrowsingDataRemover::REMOVE_PASSWORDS,
      BrowsingDataRemover::REMOVE_COOKIES,
      BrowsingDataRemover::REMOVE_COOKIES,
      BrowsingDataRemover::REMOVE_COOKIES,
      BrowsingDataRemover::REMOVE_HISTORY,
      BrowsingDataRemover::REMOVE_HISTORY,
      BrowsingDataRemover::REMOVE_HISTORY,
      BrowsingDataRemover::REMOVE_COOKIES | BrowsingDataRemover::REMOVE_HISTORY,
      BrowsingDataRemover::REMOVE_COOKIES | BrowsingDataRemover::REMOVE_HISTORY,
      BrowsingDataRemover::REMOVE_COOKIES |
          BrowsingDataRemover::REMOVE_HISTORY |
          BrowsingDataRemover::REMOVE_PASSWORDS,
      BrowsingDataRemover::REMOVE_PASSWORDS,
      BrowsingDataRemover::REMOVE_PASSWORDS,
  };

  for (int removal_mask : test_removal_masks) {
    remover->Remove(BrowsingDataRemover::Unbounded(), removal_mask,
                    BrowsingDataHelper::UNPROTECTED_WEB);
  }

  EXPECT_TRUE(remover->is_removing());

  // Add one more deletion and wait for it.
  BlockUntilBrowsingDataRemoved(
      browsing_data::ALL_TIME,
      BrowsingDataRemover::REMOVE_COOKIES,
      BrowsingDataHelper::UNPROTECTED_WEB);

  EXPECT_FALSE(remover->is_removing());
}
