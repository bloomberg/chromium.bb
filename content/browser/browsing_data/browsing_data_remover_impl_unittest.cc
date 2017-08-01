// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/browsing_data/browsing_data_remover_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_storage_partition.h"
#include "content/public/test/test_utils.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/channel_id_store.h"
#include "net/ssl/ssl_client_cert_type.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ppapi/features/features.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using testing::_;
using testing::ByRef;
using testing::Eq;
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

namespace content {

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
  std::unique_ptr<net::CanonicalCookie> cookie(
      base::MakeUnique<net::CanonicalCookie>(
          "A", "1", source.host(), "/", base::Time::Now(), base::Time::Now(),
          base::Time(), false, false, net::CookieSameSite::DEFAULT_MODE,
          net::COOKIE_PRIORITY_MEDIUM));
  EXPECT_TRUE(cookie);
  return *cookie;
}

class StoragePartitionRemovalTestStoragePartition
    : public TestStoragePartition {
 public:
  StoragePartitionRemovalTestStoragePartition() {}
  ~StoragePartitionRemovalTestStoragePartition() override {}

  void ClearDataForOrigin(uint32_t remove_mask,
                          uint32_t quota_storage_remove_mask,
                          const GURL& storage_origin,
                          net::URLRequestContextGetter* rq_context,
                          const base::Closure& callback) override {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(
            &StoragePartitionRemovalTestStoragePartition::AsyncRunCallback,
            base::Unretained(this), callback));
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
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(
            &StoragePartitionRemovalTestStoragePartition::AsyncRunCallback,
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

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(
            &StoragePartitionRemovalTestStoragePartition::AsyncRunCallback,
            base::Unretained(this), callback));
  }

  StoragePartitionRemovalData GetStoragePartitionRemovalData() {
    return storage_partition_removal_data_;
  }

 private:
  void AsyncRunCallback(const base::Closure& callback) { callback.Run(); }

  StoragePartitionRemovalData storage_partition_removal_data_;

  DISALLOW_COPY_AND_ASSIGN(StoragePartitionRemovalTestStoragePartition);
};

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
      : to_match_(filter) {}

  virtual bool MatchAndExplain(const base::Callback<bool(const GURL&)>& filter,
                               MatchResultListener* listener) const {
    if (filter.is_null() && to_match_.is_null())
      return true;
    if (filter.is_null() != to_match_.is_null())
      return false;

    const GURL urls_to_test_[] = {kOrigin1, kOrigin2, kOrigin3,
                                  GURL("invalid spec")};
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

base::Time AnHourAgo() {
  return base::Time::Now() - base::TimeDelta::FromHours(1);
}

}  // namespace

// Testers -------------------------------------------------------------------

class RemoveCookieTester {
 public:
  RemoveCookieTester() {}

  // Returns true, if the given cookie exists in the cookie store.
  bool ContainsCookie() {
    scoped_refptr<MessageLoopRunner> message_loop_runner =
        new MessageLoopRunner();
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
    scoped_refptr<MessageLoopRunner> message_loop_runner =
        new MessageLoopRunner();
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

class RemoveChannelIDTester : public net::SSLConfigService::Observer {
 public:
  explicit RemoveChannelIDTester(BrowserContext* browser_context) {
    net::URLRequestContext* url_request_context =
        BrowserContext::GetDefaultStoragePartition(browser_context)
            ->GetURLRequestContext()
            ->GetURLRequestContext();
    channel_id_service_ = url_request_context->channel_id_service();
    ssl_config_service_ = url_request_context->ssl_config_service();
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
        base::MakeUnique<net::ChannelIDStore::ChannelID>(
            server_identifier, creation_time, crypto::ECPrivateKey::Create()));
  }

  // Add a server bound cert for |server|, with the current time as the
  // creation time.  The cert and key data will be filled with dummy values.
  void AddChannelID(const std::string& server_identifier) {
    base::Time now = base::Time::Now();
    AddChannelIDWithTimes(server_identifier, now);
  }

  void GetChannelIDList(net::ChannelIDStore::ChannelIDList* channel_ids) {
    GetChannelIDStore()->GetAllChannelIDs(base::Bind(
        &RemoveChannelIDTester::GetAllChannelIDsCallback, channel_ids));
  }

  net::ChannelIDStore* GetChannelIDStore() {
    return channel_id_service_->GetChannelIDStore();
  }

  int ssl_config_changed_count() const { return ssl_config_changed_count_; }

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

class RemoveLocalStorageTester {
 public:
  explicit RemoveLocalStorageTester(BrowserContext* browser_context)
      : browser_context_(browser_context) {
    dom_storage_context_ =
        BrowserContext::GetDefaultStoragePartition(browser_context_)
            ->GetDOMStorageContext();
  }

  // Returns true, if the given origin URL exists.
  bool DOMStorageExistsForOrigin(const GURL& origin) {
    scoped_refptr<MessageLoopRunner> message_loop_runner =
        new MessageLoopRunner();
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
        browser_context_->GetPath().AppendASCII("Local Storage");
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
    base::TouchFile(storage_path.Append(kDomStorageOrigin2), one_day_ago,
                    one_day_ago);

    base::Time sixty_days_ago = now - base::TimeDelta::FromDays(60);
    base::TouchFile(storage_path.Append(kDomStorageOrigin3), sixty_days_ago,
                    sixty_days_ago);

    base::TouchFile(storage_path.Append(kDomStorageExt), now, now);
  }

 private:
  void GetLocalStorageUsage() {
    dom_storage_context_->GetLocalStorageUsage(
        base::Bind(&RemoveLocalStorageTester::OnGotLocalStorageUsage,
                   base::Unretained(this)));
  }
  void OnGotLocalStorageUsage(const std::vector<LocalStorageUsageInfo>& infos) {
    infos_ = infos;
    quit_closure_.Run();
  }

  // We don't own these pointers.
  BrowserContext* browser_context_;
  DOMStorageContext* dom_storage_context_ = nullptr;

  std::vector<LocalStorageUsageInfo> infos_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(RemoveLocalStorageTester);
};

class RemoveDownloadsTester {
 public:
  explicit RemoveDownloadsTester(BrowserContext* browser_context)
      : download_manager_(new MockDownloadManager()) {
    BrowserContext::SetDownloadManagerForTesting(
        browser_context, base::WrapUnique(download_manager_));
    EXPECT_EQ(download_manager_,
              BrowserContext::GetDownloadManager(browser_context));
    EXPECT_CALL(*download_manager_, Shutdown());
  }

  ~RemoveDownloadsTester() {}

  MockDownloadManager* download_manager() { return download_manager_; }

 private:
  MockDownloadManager* download_manager_;  // Owned by browser context.

  DISALLOW_COPY_AND_ASSIGN(RemoveDownloadsTester);
};

// Test Class ----------------------------------------------------------------

class BrowsingDataRemoverImplTest : public testing::Test {
 public:
  BrowsingDataRemoverImplTest() : browser_context_(new TestBrowserContext()) {
    remover_ = static_cast<BrowsingDataRemoverImpl*>(
        BrowserContext::GetBrowsingDataRemover(browser_context_.get()));
  }

  ~BrowsingDataRemoverImplTest() override {}

  void TearDown() override {
    mock_policy_ = nullptr;

    // BrowserContext contains a DOMStorageContext. BrowserContext's
    // destructor posts a message to the WEBKIT thread to delete some of its
    // member variables. We need to ensure that the browser context is
    // destroyed, and that the message loop is cleared out, before destroying
    // the threads and loop. Otherwise we leak memory.
    browser_context_.reset();
    RunAllBlockingPoolTasksUntilIdle();
  }

  void BlockUntilBrowsingDataRemoved(const base::Time& delete_begin,
                                     const base::Time& delete_end,
                                     int remove_mask,
                                     bool include_protected_origins) {
    StoragePartitionRemovalTestStoragePartition storage_partition;
    remover_->OverrideStoragePartitionForTesting(&storage_partition);

    int origin_type_mask = BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;
    if (include_protected_origins)
      origin_type_mask |= BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;

    BrowsingDataRemoverCompletionObserver completion_observer(remover_);
    remover_->RemoveAndReply(delete_begin, delete_end, remove_mask,
                             origin_type_mask, &completion_observer);
    completion_observer.BlockUntilCompletion();

    // Save so we can verify later.
    storage_partition_removal_data_ =
        storage_partition.GetStoragePartitionRemovalData();
  }

  void BlockUntilOriginDataRemoved(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder) {
    StoragePartitionRemovalTestStoragePartition storage_partition;
    remover_->OverrideStoragePartitionForTesting(&storage_partition);

    BrowsingDataRemoverCompletionObserver completion_observer(remover_);
    remover_->RemoveWithFilterAndReply(
        delete_begin, delete_end, remove_mask,
        BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        std::move(filter_builder), &completion_observer);
    completion_observer.BlockUntilCompletion();

    // Save so we can verify later.
    storage_partition_removal_data_ =
        storage_partition.GetStoragePartitionRemovalData();
  }

  BrowserContext* GetBrowserContext() { return browser_context_.get(); }

  void DestroyBrowserContext() { browser_context_.reset(); }

  const base::Time& GetBeginTime() { return remover_->GetLastUsedBeginTime(); }

  int GetRemovalMask() { return remover_->GetLastUsedRemovalMask(); }

  int GetOriginTypeMask() { return remover_->GetLastUsedOriginTypeMask(); }

  StoragePartitionRemovalData GetStoragePartitionRemovalData() {
    return storage_partition_removal_data_;
  }

  MockSpecialStoragePolicy* CreateMockPolicy() {
    mock_policy_ = new MockSpecialStoragePolicy();
    return mock_policy_.get();
  }

  MockSpecialStoragePolicy* mock_policy() { return mock_policy_.get(); }

  bool Match(const GURL& origin,
             int mask,
             storage::SpecialStoragePolicy* policy) {
    return remover_->DoesOriginMatchMask(mask, origin, policy);
  }

 private:
  // Cached pointer to BrowsingDataRemoverImpl for access to testing methods.
  BrowsingDataRemoverImpl* remover_;

  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<BrowserContext> browser_context_;

  StoragePartitionRemovalData storage_partition_removal_data_;

  scoped_refptr<MockSpecialStoragePolicy> mock_policy_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverImplTest);
};

// Tests ---------------------------------------------------------------------

TEST_F(BrowsingDataRemoverImplTest, RemoveCookieForever) {
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                BrowsingDataRemover::DATA_TYPE_COOKIES, false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the cookies.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_COOKIES);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());
}

TEST_F(BrowsingDataRemoverImplTest, RemoveCookieLastHour) {
  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                BrowsingDataRemover::DATA_TYPE_COOKIES, false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the cookies.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_COOKIES);
  // Removing with time period other than all time should not clear
  // persistent storage data.
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT);
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());
}

TEST_F(BrowsingDataRemoverImplTest, RemoveCookiesDomainBlacklist) {
  std::unique_ptr<BrowsingDataFilterBuilder> filter(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST));
  filter->AddRegisterableDomain(kTestRegisterableDomain1);
  filter->AddRegisterableDomain(kTestRegisterableDomain3);
  BlockUntilOriginDataRemoved(AnHourAgo(), base::Time::Max(),
                              BrowsingDataRemover::DATA_TYPE_COOKIES,
                              std::move(filter));

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the cookies.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_COOKIES);
  // Removing with time period other than all time should not clear
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

// Test that removing cookies clears HTTP auth data.
TEST_F(BrowsingDataRemoverImplTest, ClearHttpAuthCache_RemoveCookies) {
  net::HttpNetworkSession* http_session =
      BrowserContext::GetDefaultStoragePartition(GetBrowserContext())
          ->GetURLRequestContext()
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

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                BrowsingDataRemover::DATA_TYPE_COOKIES, false);

  EXPECT_EQ(nullptr, http_auth_cache->Lookup(kOrigin1, kTestRealm,
                                             net::HttpAuth::AUTH_SCHEME_BASIC));
}

TEST_F(BrowsingDataRemoverImplTest, RemoveChannelIDForever) {
  RemoveChannelIDTester tester(GetBrowserContext());

  tester.AddChannelID(kTestOrigin1);
  EXPECT_EQ(0, tester.ssl_config_changed_count());
  EXPECT_EQ(1, tester.ChannelIDCount());

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                BrowsingDataRemover::DATA_TYPE_CHANNEL_IDS,
                                false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_CHANNEL_IDS, GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_EQ(1, tester.ssl_config_changed_count());
  EXPECT_EQ(0, tester.ChannelIDCount());
}

TEST_F(BrowsingDataRemoverImplTest, RemoveChannelIDLastHour) {
  RemoveChannelIDTester tester(GetBrowserContext());

  base::Time now = base::Time::Now();
  tester.AddChannelID(kTestOrigin1);
  tester.AddChannelIDWithTimes(kTestOrigin2,
                               now - base::TimeDelta::FromHours(2));
  EXPECT_EQ(0, tester.ssl_config_changed_count());
  EXPECT_EQ(2, tester.ChannelIDCount());

  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                BrowsingDataRemover::DATA_TYPE_CHANNEL_IDS,
                                false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_CHANNEL_IDS, GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());
  EXPECT_EQ(1, tester.ssl_config_changed_count());
  ASSERT_EQ(1, tester.ChannelIDCount());
  net::ChannelIDStore::ChannelIDList channel_ids;
  tester.GetChannelIDList(&channel_ids);
  ASSERT_EQ(1U, channel_ids.size());
  EXPECT_EQ(kTestOrigin2, channel_ids.front().server_identifier());
}

TEST_F(BrowsingDataRemoverImplTest, RemoveChannelIDsForServerIdentifiers) {
  RemoveChannelIDTester tester(GetBrowserContext());

  tester.AddChannelID(kTestRegisterableDomain1);
  tester.AddChannelID(kTestRegisterableDomain3);
  EXPECT_EQ(2, tester.ChannelIDCount());

  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  filter_builder->AddRegisterableDomain(kTestRegisterableDomain1);

  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              BrowsingDataRemover::DATA_TYPE_CHANNEL_IDS,
                              std::move(filter_builder));

  EXPECT_EQ(1, tester.ChannelIDCount());
  net::ChannelIDStore::ChannelIDList channel_ids;
  tester.GetChannelIDList(&channel_ids);
  EXPECT_EQ(kTestRegisterableDomain3, channel_ids.front().server_identifier());
}

TEST_F(BrowsingDataRemoverImplTest, RemoveUnprotectedLocalStorageForever) {
  MockSpecialStoragePolicy* policy = CreateMockPolicy();
  // Protect kOrigin1.
  policy->AddProtected(kOrigin1.GetOrigin());

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE,
                                false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE, GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the data correctly.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());

  // Check origin matcher.
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOriginExt, mock_policy()));
}

TEST_F(BrowsingDataRemoverImplTest, RemoveProtectedLocalStorageForever) {
  // Protect kOrigin1.
  MockSpecialStoragePolicy* policy = CreateMockPolicy();
  policy->AddProtected(kOrigin1.GetOrigin());

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE,
                                true);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE, GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
                BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
            GetOriginTypeMask());

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

TEST_F(BrowsingDataRemoverImplTest, RemoveLocalStorageForLastWeek) {
  CreateMockPolicy();

  BlockUntilBrowsingDataRemoved(
      base::Time::Now() - base::TimeDelta::FromDays(7), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE, false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE, GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

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

TEST_F(BrowsingDataRemoverImplTest, RemoveMultipleTypes) {
  // Downloads should be deleted through the DownloadManager, assure it would
  // be called.
  RemoveDownloadsTester downloads_tester(GetBrowserContext());
  EXPECT_CALL(*downloads_tester.download_manager(),
              RemoveDownloadsByURLAndTime(_, _, _));

  int removal_mask = BrowsingDataRemover::DATA_TYPE_DOWNLOADS |
                     BrowsingDataRemover::DATA_TYPE_COOKIES;

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(), removal_mask,
                                false);

  EXPECT_EQ(removal_mask, GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // The cookie would be deleted throught the StorageParition, check if the
  // partition was requested to remove cookie.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_COOKIES);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
}

TEST_F(BrowsingDataRemoverImplTest, RemoveQuotaManagedDataForeverBoth) {
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL |
          BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
      false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL |
                BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

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

TEST_F(BrowsingDataRemoverImplTest,
       RemoveQuotaManagedDataForeverOnlyTemporary) {
  CreateMockPolicy();

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL |
          BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
      false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL |
                BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

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

TEST_F(BrowsingDataRemoverImplTest,
       RemoveQuotaManagedDataForeverOnlyPersistent) {
  CreateMockPolicy();

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL |
          BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
      false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL |
                BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

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

TEST_F(BrowsingDataRemoverImplTest, RemoveQuotaManagedDataForeverNeither) {
  CreateMockPolicy();

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL |
          BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
      false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL |
                BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

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

TEST_F(BrowsingDataRemoverImplTest,
       RemoveQuotaManagedDataForeverSpecificOrigin) {
  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);
  // Remove Origin 1.
  BlockUntilOriginDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL,
      std::move(builder));

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

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

TEST_F(BrowsingDataRemoverImplTest, RemoveQuotaManagedDataForLastHour) {
  BlockUntilBrowsingDataRemoved(
      AnHourAgo(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL |
          BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
      false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL |
                BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

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

TEST_F(BrowsingDataRemoverImplTest, RemoveQuotaManagedDataForLastWeek) {
  BlockUntilBrowsingDataRemoved(
      base::Time::Now() - base::TimeDelta::FromDays(7), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL |
          BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
      false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL |
                BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

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

TEST_F(BrowsingDataRemoverImplTest, RemoveQuotaManagedUnprotectedOrigins) {
  MockSpecialStoragePolicy* policy = CreateMockPolicy();
  // Protect kOrigin1.
  policy->AddProtected(kOrigin1.GetOrigin());

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL |
          BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
      false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL |
                BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

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
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
}

TEST_F(BrowsingDataRemoverImplTest, RemoveQuotaManagedProtectedSpecificOrigin) {
  MockSpecialStoragePolicy* policy = CreateMockPolicy();
  // Protect kOrigin1.
  policy->AddProtected(kOrigin1.GetOrigin());

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);

  // Try to remove kOrigin1. Expect failure.
  BlockUntilOriginDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL,
      std::move(builder));

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

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
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOrigin1, mock_policy()));
  // Since we use the matcher function to validate origins now, this should
  // return false for the origins we're not trying to clear.
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOrigin2, mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(kOrigin3, mock_policy()));
}

TEST_F(BrowsingDataRemoverImplTest, RemoveQuotaManagedProtectedOrigins) {
  MockSpecialStoragePolicy* policy = CreateMockPolicy();
  // Protect kOrigin1.
  policy->AddProtected(kOrigin1.GetOrigin());

  // Try to remove kOrigin1. Expect success.
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL,
      true);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB |
                BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

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

TEST_F(BrowsingDataRemoverImplTest,
       RemoveQuotaManagedIgnoreExtensionsAndDevTools) {
  CreateMockPolicy();

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL,
      false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

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

TEST_F(BrowsingDataRemoverImplTest, CompletionInhibition) {
  BrowsingDataRemoverImpl* remover = static_cast<BrowsingDataRemoverImpl*>(
      BrowserContext::GetBrowsingDataRemover(GetBrowserContext()));

  // The |completion_inhibitor| on the stack should prevent removal sessions
  // from completing until after ContinueToCompletion() is called.
  BrowsingDataRemoverCompletionInhibitor completion_inhibitor(remover);
  InspectableCompletionObserver completion_observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(), BrowsingDataRemover::DATA_TYPE_COOKIES,
      BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &completion_observer);

  // Process messages until the inhibitor is notified, and then some, to make
  // sure we do not complete asynchronously before ContinueToCompletion() is
  // called.
  completion_inhibitor.BlockUntilNearCompletion();
  RunAllBlockingPoolTasksUntilIdle();

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

TEST_F(BrowsingDataRemoverImplTest, EarlyShutdown) {
  BrowsingDataRemoverImpl* remover = static_cast<BrowsingDataRemoverImpl*>(
      BrowserContext::GetBrowsingDataRemover(GetBrowserContext()));
  InspectableCompletionObserver completion_observer(remover);
  BrowsingDataRemoverCompletionInhibitor completion_inhibitor(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(), BrowsingDataRemover::DATA_TYPE_COOKIES,
      BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &completion_observer);

  completion_inhibitor.BlockUntilNearCompletion();
  completion_inhibitor.Reset();

  // Verify that the deletion has not yet been completed and the observer has
  // not been called.
  EXPECT_TRUE(remover->is_removing());
  EXPECT_FALSE(completion_observer.called());

  // Destroying the profile should trigger the notification.
  DestroyBrowserContext();

  EXPECT_TRUE(completion_observer.called());

  // Finishing after shutdown shouldn't break anything.
  completion_inhibitor.ContinueToCompletion();
  completion_observer.BlockUntilCompletion();
}

TEST_F(BrowsingDataRemoverImplTest, RemoveDownloadsByTimeOnly) {
  RemoveDownloadsTester tester(GetBrowserContext());
  base::Callback<bool(const GURL&)> filter =
      BrowsingDataFilterBuilder::BuildNoopFilter();

  EXPECT_CALL(*tester.download_manager(),
              RemoveDownloadsByURLAndTime(ProbablySameFilter(filter), _, _));

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
                                false);
}

TEST_F(BrowsingDataRemoverImplTest, RemoveDownloadsByOrigin) {
  RemoveDownloadsTester tester(GetBrowserContext());
  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  builder->AddRegisterableDomain(kTestRegisterableDomain1);
  base::Callback<bool(const GURL&)> filter = builder->BuildGeneralFilter();

  EXPECT_CALL(*tester.download_manager(),
              RemoveDownloadsByURLAndTime(ProbablySameFilter(filter), _, _));

  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
                              std::move(builder));
}

class MultipleTasksObserver {
 public:
  // A simple implementation of BrowsingDataRemover::Observer.
  // MultipleTasksObserver will use several instances of Target to test
  // that completion callbacks are returned to the correct one.
  class Target : public BrowsingDataRemover::Observer {
   public:
    Target(MultipleTasksObserver* parent, BrowsingDataRemover* remover)
        : parent_(parent), observer_(this) {
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

  void ClearLastCalledTarget() { last_called_target_ = nullptr; }

  Target* GetLastCalledTarget() { return last_called_target_; }

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

TEST_F(BrowsingDataRemoverImplTest, MultipleTasks) {
  BrowsingDataRemoverImpl* remover = static_cast<BrowsingDataRemoverImpl*>(
      BrowserContext::GetBrowsingDataRemover(GetBrowserContext()));
  EXPECT_FALSE(remover->is_removing());

  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_1(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_2(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST));
  filter_builder_2->AddRegisterableDomain("example.com");

  MultipleTasksObserver observer(remover);
  BrowsingDataRemoverCompletionInhibitor completion_inhibitor(remover);

  // Test several tasks with various configuration of masks, filters, and target
  // observers.
  std::list<BrowsingDataRemoverImpl::RemovalTask> tasks;
  tasks.emplace_back(
      base::Time(), base::Time::Max(), BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
      BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST),
      observer.target_a());
  tasks.emplace_back(
      base::Time(), base::Time::Max(), BrowsingDataRemover::DATA_TYPE_COOKIES,
      BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST),
      nullptr);
  tasks.emplace_back(
      base::Time::Now(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
      BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
          BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST),
      observer.target_b());
  tasks.emplace_back(base::Time(), base::Time::UnixEpoch(),
                     BrowsingDataRemover::DATA_TYPE_WEB_SQL,
                     BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
                     std::move(filter_builder_1), observer.target_b());
  tasks.emplace_back(base::Time::UnixEpoch(), base::Time::Now(),
                     BrowsingDataRemover::DATA_TYPE_CHANNEL_IDS,
                     BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
                         BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
                     std::move(filter_builder_2), nullptr);

  for (BrowsingDataRemoverImpl::RemovalTask& task : tasks) {
    // All tasks can be directly translated to a RemoveInternal() call. Since
    // that is a private method, we must call the four public versions of
    // Remove.* instead. This also serves as a test that those methods are all
    // correctly reduced to RemoveInternal().
    if (!task.observer && task.filter_builder->IsEmptyBlacklist()) {
      remover->Remove(task.delete_begin, task.delete_end, task.remove_mask,
                      task.origin_type_mask);
    } else if (task.filter_builder->IsEmptyBlacklist()) {
      remover->RemoveAndReply(task.delete_begin, task.delete_end,
                              task.remove_mask, task.origin_type_mask,
                              task.observer);
    } else if (!task.observer) {
      remover->RemoveWithFilter(task.delete_begin, task.delete_end,
                                task.remove_mask, task.origin_type_mask,
                                std::move(task.filter_builder));
    } else {
      remover->RemoveWithFilterAndReply(
          task.delete_begin, task.delete_end, task.remove_mask,
          task.origin_type_mask, std::move(task.filter_builder), task.observer);
    }
  }

  // Use the inhibitor to stop after every task and check the results.
  for (BrowsingDataRemoverImpl::RemovalTask& task : tasks) {
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
    EXPECT_EQ(task.delete_begin, GetBeginTime());
  }

  EXPECT_FALSE(remover->is_removing());

  // Run clean up tasks.
  RunAllBlockingPoolTasksUntilIdle();
}

// The previous test, BrowsingDataRemoverTest.MultipleTasks, tests that the
// tasks are not mixed up and they are executed in a correct order. However,
// the completion inhibitor kept synchronizing the execution in order to verify
// the parameters. This test demonstrates that even running the tasks without
// inhibition is executed correctly and doesn't crash.
TEST_F(BrowsingDataRemoverImplTest, MultipleTasksInQuickSuccession) {
  BrowsingDataRemoverImpl* remover = static_cast<BrowsingDataRemoverImpl*>(
      BrowserContext::GetBrowsingDataRemover(GetBrowserContext()));
  EXPECT_FALSE(remover->is_removing());

  int test_removal_masks[] = {
      BrowsingDataRemover::DATA_TYPE_COOKIES,
      BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE,
      BrowsingDataRemover::DATA_TYPE_COOKIES,
      BrowsingDataRemover::DATA_TYPE_COOKIES,
      BrowsingDataRemover::DATA_TYPE_COOKIES,
      BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
      BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
      BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
      BrowsingDataRemover::DATA_TYPE_COOKIES |
          BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
      BrowsingDataRemover::DATA_TYPE_COOKIES |
          BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
      BrowsingDataRemover::DATA_TYPE_COOKIES |
          BrowsingDataRemover::DATA_TYPE_DOWNLOADS |
          BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE,
      BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE,
      BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE,
  };

  for (int removal_mask : test_removal_masks) {
    remover->Remove(base::Time(), base::Time::Max(), removal_mask,
                    BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
  }

  EXPECT_TRUE(remover->is_removing());

  // Add one more deletion and wait for it.
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(), BrowsingDataRemover::DATA_TYPE_COOKIES,
      BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);

  EXPECT_FALSE(remover->is_removing());
}

}  // namespace content
