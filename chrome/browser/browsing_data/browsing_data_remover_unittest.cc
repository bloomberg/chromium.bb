// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_remover.h"

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_observer.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/extensions/mock_extension_special_storage_policy.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/server_bound_cert_service.h"
#include "net/base/server_bound_cert_store.h"
#include "net/base/ssl_client_cert_type.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/quota/mock_quota_manager.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/quota_types.h"

using content::BrowserThread;

namespace {

const char kTestOrigin1[] = "http://host1:1/";
const char kTestOrigin2[] = "http://host2:1/";
const char kTestOrigin3[] = "http://host3:1/";
const char kTestOriginExt[] = "chrome-extension://abcdefghijklmnopqrstuvwxyz/";
const char kTestOriginDevTools[] = "chrome-devtools://abcdefghijklmnopqrstuvw/";

const GURL kOrigin1(kTestOrigin1);
const GURL kOrigin2(kTestOrigin2);
const GURL kOrigin3(kTestOrigin3);
const GURL kOriginExt(kTestOriginExt);
const GURL kOriginDevTools(kTestOriginDevTools);

const FilePath::CharType kDomStorageOrigin1[] =
    FILE_PATH_LITERAL("http_host1_1.localstorage");

const FilePath::CharType kDomStorageOrigin2[] =
    FILE_PATH_LITERAL("http_host2_1.localstorage");

const FilePath::CharType kDomStorageOrigin3[] =
    FILE_PATH_LITERAL("http_host3_1.localstorage");

const FilePath::CharType kDomStorageExt[] = FILE_PATH_LITERAL(
    "chrome-extension_abcdefghijklmnopqrstuvwxyz_0.localstorage");

const quota::StorageType kTemporary = quota::kStorageTypeTemporary;
const quota::StorageType kPersistent = quota::kStorageTypePersistent;

const quota::QuotaClient::ID kClientFile = quota::QuotaClient::kFileSystem;
const quota::QuotaClient::ID kClientDB = quota::QuotaClient::kIndexedDatabase;

void PopulateTestQuotaManagedNonBrowsingData(quota::MockQuotaManager* manager) {
  manager->AddOrigin(kOriginDevTools, kTemporary, kClientFile, base::Time());
  manager->AddOrigin(kOriginDevTools, kPersistent, kClientFile, base::Time());
  manager->AddOrigin(kOriginExt, kTemporary, kClientFile, base::Time());
  manager->AddOrigin(kOriginExt, kPersistent, kClientFile, base::Time());
}

void PopulateTestQuotaManagedPersistentData(quota::MockQuotaManager* manager) {
  manager->AddOrigin(kOrigin2, kPersistent, kClientFile, base::Time());
  manager->AddOrigin(kOrigin3, kPersistent, kClientFile,
      base::Time::Now() - base::TimeDelta::FromDays(1));

  EXPECT_FALSE(manager->OriginHasData(kOrigin1, kPersistent, kClientFile));
  EXPECT_TRUE(manager->OriginHasData(kOrigin2, kPersistent, kClientFile));
  EXPECT_TRUE(manager->OriginHasData(kOrigin3, kPersistent, kClientFile));
}

void PopulateTestQuotaManagedTemporaryData(quota::MockQuotaManager* manager) {
  manager->AddOrigin(kOrigin1, kTemporary, kClientFile, base::Time::Now());
  manager->AddOrigin(kOrigin3, kTemporary, kClientFile,
      base::Time::Now() - base::TimeDelta::FromDays(1));

  EXPECT_TRUE(manager->OriginHasData(kOrigin1, kTemporary, kClientFile));
  EXPECT_FALSE(manager->OriginHasData(kOrigin2, kTemporary, kClientFile));
  EXPECT_TRUE(manager->OriginHasData(kOrigin3, kTemporary, kClientFile));
}

void PopulateTestQuotaManagedData(quota::MockQuotaManager* manager) {
  // Set up kOrigin1 with a temporary quota, kOrigin2 with a persistent
  // quota, and kOrigin3 with both. kOrigin1 is modified now, kOrigin2
  // is modified at the beginning of time, and kOrigin3 is modified one day
  // ago.
  PopulateTestQuotaManagedPersistentData(manager);
  PopulateTestQuotaManagedTemporaryData(manager);
}

class AwaitCompletionHelper : public BrowsingDataRemover::Observer {
 public:
  AwaitCompletionHelper() : start_(false), already_quit_(false) {}
  virtual ~AwaitCompletionHelper() {}

  void BlockUntilNotified() {
    if (!already_quit_) {
      DCHECK(!start_);
      start_ = true;
      MessageLoop::current()->Run();
    } else {
      DCHECK(!start_);
      already_quit_ = false;
    }
  }

  void Notify() {
    if (start_) {
      DCHECK(!already_quit_);
      MessageLoop::current()->Quit();
      start_ = false;
    } else {
      DCHECK(!already_quit_);
      already_quit_ = true;
    }
  }

 protected:
  // BrowsingDataRemover::Observer implementation.
  virtual void OnBrowsingDataRemoverDone() {
    Notify();
  }

 private:
  // Helps prevent from running message_loop, if the callback invoked
  // immediately.
  bool start_;
  bool already_quit_;

  DISALLOW_COPY_AND_ASSIGN(AwaitCompletionHelper);
};

}  // namespace

// Testers -------------------------------------------------------------------

class RemoveCookieTester {
 public:
  RemoveCookieTester() : get_cookie_success_(false) {
  }

  // Returns true, if the given cookie exists in the cookie store.
  bool ContainsCookie() {
    get_cookie_success_ = false;
    monster_->GetCookiesWithOptionsAsync(
        kOrigin1, net::CookieOptions(),
        base::Bind(&RemoveCookieTester::GetCookieCallback,
                   base::Unretained(this)));
    await_completion_.BlockUntilNotified();
    return get_cookie_success_;
  }

  void AddCookie() {
    monster_->SetCookieWithOptionsAsync(
        kOrigin1, "A=1", net::CookieOptions(),
        base::Bind(&RemoveCookieTester::SetCookieCallback,
                   base::Unretained(this)));
    await_completion_.BlockUntilNotified();
  }

 protected:
  void SetMonster(net::CookieStore* monster) {
    monster_ = monster;
  }

 private:
  void GetCookieCallback(const std::string& cookies) {
    if (cookies == "A=1") {
      get_cookie_success_ = true;
    } else {
      EXPECT_EQ("", cookies);
      get_cookie_success_ = false;
    }
    await_completion_.Notify();
  }

  void SetCookieCallback(bool result) {
    ASSERT_TRUE(result);
    await_completion_.Notify();
  }

  bool get_cookie_success_;
  AwaitCompletionHelper await_completion_;
  net::CookieStore* monster_;

  DISALLOW_COPY_AND_ASSIGN(RemoveCookieTester);
};

class RemoveProfileCookieTester : public RemoveCookieTester {
 public:
  explicit RemoveProfileCookieTester(TestingProfile* profile) {
    profile->CreateRequestContext();
    SetMonster(profile->GetRequestContext()->GetURLRequestContext()->
        cookie_store()->GetCookieMonster());
  }
};

#if defined(ENABLE_SAFE_BROWSING)
class RemoveSafeBrowsingCookieTester : public RemoveCookieTester {
 public:
  RemoveSafeBrowsingCookieTester()
      : browser_process_(
          static_cast<TestingBrowserProcess*>(g_browser_process)) {
    scoped_refptr<SafeBrowsingService> sb_service =
        SafeBrowsingService::CreateSafeBrowsingService();
    browser_process_->SetSafeBrowsingService(sb_service);
    sb_service->Initialize();
    MessageLoop::current()->RunAllPending();

    // Create a cookiemonster that does not have persistant storage, and replace
    // the SafeBrowsingService created one with it.
    net::CookieStore* monster = new net::CookieMonster(NULL, NULL);
    sb_service->url_request_context()->GetURLRequestContext()->
        set_cookie_store(monster);
    SetMonster(monster);
  }

  virtual ~RemoveSafeBrowsingCookieTester() {
    browser_process_->safe_browsing_service()->ShutDown();
    MessageLoop::current()->RunAllPending();
    browser_process_->SetSafeBrowsingService(NULL);
  }

 private:
  TestingBrowserProcess* browser_process_;

  DISALLOW_COPY_AND_ASSIGN(RemoveSafeBrowsingCookieTester);
};
#endif

class RemoveServerBoundCertTester {
 public:
  explicit RemoveServerBoundCertTester(TestingProfile* profile) {
    profile->CreateRequestContext();
    server_bound_cert_service_ = profile->GetRequestContext()->
        GetURLRequestContext()->server_bound_cert_service();
  }

  int ServerBoundCertCount() {
    return server_bound_cert_service_->cert_count();
  }

  // Add a server bound cert for |server| with specific creation and expiry
  // times.  The cert and key data will be filled with dummy values.
  void AddServerBoundCertWithTimes(const std::string& server_identifier,
                                   base::Time creation_time,
                                   base::Time expiration_time) {
    GetCertStore()->SetServerBoundCert(server_identifier,
                                       net::CLIENT_CERT_RSA_SIGN, creation_time,
                                       expiration_time, "a", "b");
  }

  // Add a server bound cert for |server|, with the current time as the
  // creation time.  The cert and key data will be filled with dummy values.
  void AddServerBoundCert(const std::string& server_identifier) {
    base::Time now = base::Time::Now();
    AddServerBoundCertWithTimes(server_identifier,
                                now,
                                now + base::TimeDelta::FromDays(1));
  }

  net::ServerBoundCertStore* GetCertStore() {
    return server_bound_cert_service_->GetCertStore();
  }

 private:
  net::ServerBoundCertService* server_bound_cert_service_;

  net::SSLClientCertType type_;
  std::string key_;
  std::string cert_;

  DISALLOW_COPY_AND_ASSIGN(RemoveServerBoundCertTester);
};

class RemoveHistoryTester {
 public:
  explicit RemoveHistoryTester(TestingProfile* profile)
      : query_url_success_(false) {
    profile->CreateHistoryService(true, false);
    history_service_ = HistoryServiceFactory::GetForProfile(
        profile, Profile::EXPLICIT_ACCESS);
  }

  // Returns true, if the given URL exists in the history service.
  bool HistoryContainsURL(const GURL& url) {
    history_service_->QueryURL(
        url,
        true,
        &consumer_,
        base::Bind(&RemoveHistoryTester::SaveResultAndQuit,
                   base::Unretained(this)));
    await_completion_.BlockUntilNotified();
    return query_url_success_;
  }

  void AddHistory(const GURL& url, base::Time time) {
    history_service_->AddPage(url, time, NULL, 0, GURL(),
        content::PAGE_TRANSITION_LINK, history::RedirectList(),
        history::SOURCE_BROWSED, false);
  }

 private:
  // Callback for HistoryService::QueryURL.
  void SaveResultAndQuit(HistoryService::Handle,
                         bool success,
                         const history::URLRow*,
                         history::VisitVector*) {
    query_url_success_ = success;
    await_completion_.Notify();
  }

  // For History requests.
  CancelableRequestConsumer consumer_;
  bool query_url_success_;

  // TestingProfile owns the history service; we shouldn't delete it.
  HistoryService* history_service_;

  AwaitCompletionHelper await_completion_;

  DISALLOW_COPY_AND_ASSIGN(RemoveHistoryTester);
};

class RemoveAutofillTester : public PersonalDataManagerObserver {
 public:
  explicit RemoveAutofillTester(TestingProfile* profile)
      : personal_data_manager_(
            PersonalDataManagerFactory::GetForProfile(profile)) {
    autofill_test::DisableSystemServices(profile);
    personal_data_manager_->SetObserver(this);
  }

  virtual ~RemoveAutofillTester() {
    personal_data_manager_->RemoveObserver(this);
  }

  // Returns true if there are autofill profiles.
  bool HasProfile() {
    return !personal_data_manager_->profiles().empty() &&
           !personal_data_manager_->credit_cards().empty();
  }

  void AddProfile() {
    AutofillProfile profile;
    profile.SetInfo(NAME_FIRST, ASCIIToUTF16("Bob"));
    profile.SetInfo(NAME_LAST, ASCIIToUTF16("Smith"));
    profile.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("94043"));
    profile.SetInfo(EMAIL_ADDRESS, ASCIIToUTF16("sue@example.com"));
    profile.SetInfo(COMPANY_NAME, ASCIIToUTF16("Company X"));

    std::vector<AutofillProfile> profiles;
    profiles.push_back(profile);
    personal_data_manager_->SetProfiles(&profiles);
    MessageLoop::current()->Run();

    CreditCard card;
    card.SetInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("1234-5678-9012-3456"));

    std::vector<CreditCard> cards;
    cards.push_back(card);
    personal_data_manager_->SetCreditCards(&cards);
    MessageLoop::current()->Run();
  }

 private:
  virtual void OnPersonalDataChanged() OVERRIDE {
    MessageLoop::current()->Quit();
  }

  PersonalDataManager* personal_data_manager_;
  DISALLOW_COPY_AND_ASSIGN(RemoveAutofillTester);
};

class RemoveLocalStorageTester {
 public:
  explicit RemoveLocalStorageTester(TestingProfile* profile)
      : profile_(profile), dom_storage_context_(NULL) {
    dom_storage_context_ =
        content::BrowserContext::GetDefaultStoragePartition(profile)->
            GetDOMStorageContext();
  }

  // Returns true, if the given origin URL exists.
  bool DOMStorageExistsForOrigin(const GURL& origin) {
    GetUsageInfo();
    await_completion_.BlockUntilNotified();
    for (size_t i = 0; i < infos_.size(); ++i) {
      if (origin == infos_[i].origin)
        return true;
    }
    return false;
  }

  void AddDOMStorageTestData() {
    // Note: This test depends on details of how the dom_storage library
    // stores data in the host file system.
    FilePath storage_path = profile_->GetPath().AppendASCII("Local Storage");
    file_util::CreateDirectory(storage_path);

    // Write some files.
    file_util::WriteFile(storage_path.Append(kDomStorageOrigin1), NULL, 0);
    file_util::WriteFile(storage_path.Append(kDomStorageOrigin2), NULL, 0);
    file_util::WriteFile(storage_path.Append(kDomStorageOrigin3), NULL, 0);
    file_util::WriteFile(storage_path.Append(kDomStorageExt), NULL, 0);

    // Tweak their dates.
    file_util::SetLastModifiedTime(storage_path.Append(kDomStorageOrigin1),
        base::Time::Now());
    file_util::SetLastModifiedTime(storage_path.Append(kDomStorageOrigin2),
        base::Time::Now() - base::TimeDelta::FromDays(1));
    file_util::SetLastModifiedTime(storage_path.Append(kDomStorageOrigin3),
        base::Time::Now() - base::TimeDelta::FromDays(60));
    file_util::SetLastModifiedTime(storage_path.Append(kDomStorageExt),
        base::Time::Now());
  }

 private:
  void GetUsageInfo() {
    dom_storage_context_->GetUsageInfo(
        base::Bind(&RemoveLocalStorageTester::OnGotUsageInfo,
                   base::Unretained(this)));
  }
  void OnGotUsageInfo(
      const std::vector<dom_storage::DomStorageContext::UsageInfo>& infos) {
    infos_ = infos;
    await_completion_.Notify();
  }

  // We don't own these pointers.
  TestingProfile* profile_;
  content::DOMStorageContext* dom_storage_context_;

  std::vector<dom_storage::DomStorageContext::UsageInfo> infos_;

  AwaitCompletionHelper await_completion_;

  DISALLOW_COPY_AND_ASSIGN(RemoveLocalStorageTester);
};

// Test Class ----------------------------------------------------------------

class BrowsingDataRemoverTest : public testing::Test,
                                public content::NotificationObserver {
 public:
  BrowsingDataRemoverTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB, &message_loop_),
        webkit_thread_(BrowserThread::WEBKIT_DEPRECATED, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        file_user_blocking_thread_(
            BrowserThread::FILE_USER_BLOCKING, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_),
        profile_(new TestingProfile()) {
    registrar_.Add(this, chrome::NOTIFICATION_BROWSING_DATA_REMOVED,
                   content::Source<Profile>(profile_.get()));
  }

  virtual ~BrowsingDataRemoverTest() {
  }

  void TearDown() {
    // TestingProfile contains a DOMStorageContext.  BrowserContext's destructor
    // posts a message to the WEBKIT thread to delete some of its member
    // variables. We need to ensure that the profile is destroyed, and that
    // the message loop is cleared out, before destroying the threads and loop.
    // Otherwise we leak memory.
    profile_.reset();
    message_loop_.RunAllPending();
  }

  void BlockUntilBrowsingDataRemoved(BrowsingDataRemover::TimePeriod period,
                                     int remove_mask,
                                     bool include_protected_origins) {
    BrowsingDataRemover* remover = BrowsingDataRemover::CreateForPeriod(
        profile_.get(), period);
    remover->OverrideQuotaManagerForTesting(GetMockManager());

    AwaitCompletionHelper await_completion;
    remover->AddObserver(&await_completion);

    called_with_details_.reset(new BrowsingDataRemover::NotificationDetails());

    // BrowsingDataRemover deletes itself when it completes.
    int origin_set_mask = BrowsingDataHelper::UNPROTECTED_WEB;
    if (include_protected_origins)
      origin_set_mask |= BrowsingDataHelper::PROTECTED_WEB;
    remover->Remove(remove_mask, origin_set_mask);
    await_completion.BlockUntilNotified();
  }

  void BlockUntilOriginDataRemoved(BrowsingDataRemover::TimePeriod period,
                                   int remove_mask,
                                   const GURL& remove_origin) {
    BrowsingDataRemover* remover = BrowsingDataRemover::CreateForPeriod(
        profile_.get(), period);
    remover->OverrideQuotaManagerForTesting(GetMockManager());

    AwaitCompletionHelper await_completion;
    remover->AddObserver(&await_completion);

    called_with_details_.reset(new BrowsingDataRemover::NotificationDetails());

    // BrowsingDataRemover deletes itself when it completes.
    remover->RemoveImpl(remove_mask, remove_origin,
        BrowsingDataHelper::UNPROTECTED_WEB);
    await_completion.BlockUntilNotified();
  }

  TestingProfile* GetProfile() {
    return profile_.get();
  }

  base::Time GetBeginTime() {
    return called_with_details_->removal_begin;
  }

  int GetRemovalMask() {
    return called_with_details_->removal_mask;
  }

  int GetOriginSetMask() {
    return called_with_details_->origin_set_mask;
  }

  quota::MockQuotaManager* GetMockManager() {
    if (!quota_manager_) {
      quota_manager_ = new quota::MockQuotaManager(
        profile_->IsOffTheRecord(),
        profile_->GetPath(),
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB),
        profile_->GetExtensionSpecialStoragePolicy());
    }
    return quota_manager_;
  }

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK_EQ(type, chrome::NOTIFICATION_BROWSING_DATA_REMOVED);

    // We're not taking ownership of the details object, but storing a copy of
    // it locally.
    called_with_details_.reset(new BrowsingDataRemover::NotificationDetails(
        *content::Details<BrowsingDataRemover::NotificationDetails>(
            details).ptr()));

    registrar_.RemoveAll();
  }

 private:
  scoped_ptr<BrowsingDataRemover::NotificationDetails> called_with_details_;
  content::NotificationRegistrar registrar_;

  // message_loop_, as well as all the threads associated with it must be
  // defined before profile_ to prevent explosions. Oh how I love C++.
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  content::TestBrowserThread webkit_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread file_user_blocking_thread_;
  content::TestBrowserThread io_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<quota::MockQuotaManager> quota_manager_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverTest);
};

// Tests ---------------------------------------------------------------------

TEST_F(BrowsingDataRemoverTest, RemoveCookieForever) {
  RemoveProfileCookieTester tester(GetProfile());

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_COOKIES, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_FALSE(tester.ContainsCookie());
}

TEST_F(BrowsingDataRemoverTest, RemoveCookieLastHour) {
  RemoveProfileCookieTester tester(GetProfile());

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_HOUR,
      BrowsingDataRemover::REMOVE_COOKIES, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_FALSE(tester.ContainsCookie());
}

#if defined(ENABLE_SAFE_BROWSING)
TEST_F(BrowsingDataRemoverTest, RemoveSafeBrowsingCookieForever) {
  RemoveSafeBrowsingCookieTester tester;

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_COOKIES, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_FALSE(tester.ContainsCookie());
}

TEST_F(BrowsingDataRemoverTest, RemoveSafeBrowsingCookieLastHour) {
  RemoveSafeBrowsingCookieTester tester;

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_HOUR,
      BrowsingDataRemover::REMOVE_COOKIES, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  // Removing with time period other than EVERYTHING should not clear safe
  // browsing cookies.
  EXPECT_TRUE(tester.ContainsCookie());
}
#endif

TEST_F(BrowsingDataRemoverTest, RemoveServerBoundCertForever) {
  RemoveServerBoundCertTester tester(GetProfile());

  tester.AddServerBoundCert(kTestOrigin1);
  EXPECT_EQ(1, tester.ServerBoundCertCount());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_SERVER_BOUND_CERTS, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_SERVER_BOUND_CERTS, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_EQ(0, tester.ServerBoundCertCount());
}

TEST_F(BrowsingDataRemoverTest, RemoveServerBoundCertLastHour) {
  RemoveServerBoundCertTester tester(GetProfile());

  base::Time now = base::Time::Now();
  tester.AddServerBoundCert(kTestOrigin1);
  tester.AddServerBoundCertWithTimes(kTestOrigin2,
                                     now - base::TimeDelta::FromHours(2),
                                     now);
  EXPECT_EQ(2, tester.ServerBoundCertCount());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_HOUR,
      BrowsingDataRemover::REMOVE_SERVER_BOUND_CERTS, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_SERVER_BOUND_CERTS, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_EQ(1, tester.ServerBoundCertCount());
  net::ServerBoundCertStore::ServerBoundCertList certs;
  tester.GetCertStore()->GetAllServerBoundCerts(&certs);
  EXPECT_EQ(kTestOrigin2, certs.front().server_identifier());
}

TEST_F(BrowsingDataRemoverTest, RemoveUnprotectedLocalStorageForever) {
  // Protect kOrigin1.
  scoped_refptr<MockExtensionSpecialStoragePolicy> mock_policy =
      new MockExtensionSpecialStoragePolicy;
  mock_policy->AddProtected(kOrigin1.GetOrigin());
  GetProfile()->SetExtensionSpecialStoragePolicy(mock_policy);

  RemoveLocalStorageTester tester(GetProfile());

  tester.AddDOMStorageTestData();
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin3));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOriginExt));

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_LOCAL_STORAGE, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_LOCAL_STORAGE, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin3));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOriginExt));
}

TEST_F(BrowsingDataRemoverTest, RemoveProtectedLocalStorageForever) {
  // Protect kOrigin1.
  scoped_refptr<MockExtensionSpecialStoragePolicy> mock_policy =
      new MockExtensionSpecialStoragePolicy;
  mock_policy->AddProtected(kOrigin1.GetOrigin());
  GetProfile()->SetExtensionSpecialStoragePolicy(mock_policy);

  RemoveLocalStorageTester tester(GetProfile());

  tester.AddDOMStorageTestData();
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin3));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOriginExt));

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_LOCAL_STORAGE, true);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_LOCAL_STORAGE, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::PROTECTED_WEB |
      BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin3));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOriginExt));
}

TEST_F(BrowsingDataRemoverTest, RemoveLocalStorageForLastWeek) {
  RemoveLocalStorageTester tester(GetProfile());

  tester.AddDOMStorageTestData();
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin3));

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_WEEK,
      BrowsingDataRemover::REMOVE_LOCAL_STORAGE, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_LOCAL_STORAGE, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin3));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOriginExt));
}

TEST_F(BrowsingDataRemoverTest, RemoveHistoryForever) {
  RemoveHistoryTester tester(GetProfile());

  tester.AddHistory(kOrigin1, base::Time::Now());
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_HISTORY, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_FALSE(tester.HistoryContainsURL(kOrigin1));
}

TEST_F(BrowsingDataRemoverTest, RemoveHistoryForLastHour) {
  RemoveHistoryTester tester(GetProfile());

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  tester.AddHistory(kOrigin1, base::Time::Now());
  tester.AddHistory(kOrigin2, two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin2));

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_HOUR,
      BrowsingDataRemover::REMOVE_HISTORY, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_FALSE(tester.HistoryContainsURL(kOrigin1));
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin2));
}

TEST_F(BrowsingDataRemoverTest, QuotaClientMaskGeneration) {
  EXPECT_EQ(quota::QuotaClient::kFileSystem,
            BrowsingDataRemover::GenerateQuotaClientMask(
                BrowsingDataRemover::REMOVE_FILE_SYSTEMS));
  EXPECT_EQ(quota::QuotaClient::kDatabase,
            BrowsingDataRemover::GenerateQuotaClientMask(
                BrowsingDataRemover::REMOVE_WEBSQL));
  EXPECT_EQ(quota::QuotaClient::kAppcache,
            BrowsingDataRemover::GenerateQuotaClientMask(
                BrowsingDataRemover::REMOVE_APPCACHE));
  EXPECT_EQ(quota::QuotaClient::kIndexedDatabase,
            BrowsingDataRemover::GenerateQuotaClientMask(
                BrowsingDataRemover::REMOVE_INDEXEDDB));
  EXPECT_EQ(quota::QuotaClient::kFileSystem |
            quota::QuotaClient::kDatabase |
            quota::QuotaClient::kAppcache |
            quota::QuotaClient::kIndexedDatabase,
            BrowsingDataRemover::GenerateQuotaClientMask(
                BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
                BrowsingDataRemover::REMOVE_WEBSQL |
                BrowsingDataRemover::REMOVE_APPCACHE |
                BrowsingDataRemover::REMOVE_INDEXEDDB));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverBoth) {
  PopulateTestQuotaManagedData(GetMockManager());
  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_INDEXEDDB, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_INDEXEDDB, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverOnlyTemporary) {
  PopulateTestQuotaManagedTemporaryData(GetMockManager());
  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_INDEXEDDB, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_INDEXEDDB, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverOnlyPersistent) {
  PopulateTestQuotaManagedPersistentData(GetMockManager());
  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_INDEXEDDB, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_INDEXEDDB, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverNeither) {
  GetMockManager();  // Creates the QuotaManager instance.

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_INDEXEDDB, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_INDEXEDDB, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverSpecificOrigin) {
  PopulateTestQuotaManagedData(GetMockManager());

  // Remove Origin 1.
  BlockUntilOriginDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_INDEXEDDB |
      BrowsingDataRemover::REMOVE_WEBSQL, kOrigin1);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_INDEXEDDB |
      BrowsingDataRemover::REMOVE_WEBSQL, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForLastHour) {
  PopulateTestQuotaManagedData(GetMockManager());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_HOUR,
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_INDEXEDDB, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_INDEXEDDB, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForLastWeek) {
  PopulateTestQuotaManagedData(GetMockManager());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_WEEK,
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_INDEXEDDB, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_INDEXEDDB, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedUnprotectedOrigins) {
  // Protect kOrigin1.
  scoped_refptr<MockExtensionSpecialStoragePolicy> mock_policy =
      new MockExtensionSpecialStoragePolicy;
  mock_policy->AddProtected(kOrigin1.GetOrigin());
  GetProfile()->SetExtensionSpecialStoragePolicy(mock_policy);

  PopulateTestQuotaManagedData(GetMockManager());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_INDEXEDDB, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_WEBSQL |
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_INDEXEDDB, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedProtectedSpecificOrigin) {
  // Protect kOrigin1.
  scoped_refptr<MockExtensionSpecialStoragePolicy> mock_policy =
      new MockExtensionSpecialStoragePolicy;
  mock_policy->AddProtected(kOrigin1.GetOrigin());
  GetProfile()->SetExtensionSpecialStoragePolicy(mock_policy);

  PopulateTestQuotaManagedData(GetMockManager());

  // Try to remove kOrigin1. Expect failure.
  BlockUntilOriginDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_INDEXEDDB |
      BrowsingDataRemover::REMOVE_WEBSQL, kOrigin1);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_INDEXEDDB |
      BrowsingDataRemover::REMOVE_WEBSQL, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedProtectedOrigins) {
  // Protect kOrigin1.
  scoped_refptr<MockExtensionSpecialStoragePolicy> mock_policy =
      new MockExtensionSpecialStoragePolicy;
  mock_policy->AddProtected(kOrigin1.GetOrigin());
  GetProfile()->SetExtensionSpecialStoragePolicy(mock_policy);

  PopulateTestQuotaManagedData(GetMockManager());

  // Try to remove kOrigin1. Expect success.
  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_INDEXEDDB |
      BrowsingDataRemover::REMOVE_WEBSQL, true);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_INDEXEDDB |
      BrowsingDataRemover::REMOVE_WEBSQL, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::PROTECTED_WEB |
      BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kTemporary,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2, kPersistent,
      kClientFile));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3, kPersistent,
      kClientFile));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedIgnoreExtensionsAndDevTools) {
  PopulateTestQuotaManagedNonBrowsingData(GetMockManager());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_INDEXEDDB |
      BrowsingDataRemover::REMOVE_WEBSQL, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_APPCACHE |
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS |
      BrowsingDataRemover::REMOVE_INDEXEDDB |
      BrowsingDataRemover::REMOVE_WEBSQL, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());

  // Check that extension and devtools data isn't removed.
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOriginExt, kTemporary,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOriginExt, kPersistent,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOriginDevTools, kTemporary,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOriginDevTools, kPersistent,
      kClientFile));
}

TEST_F(BrowsingDataRemoverTest, OriginBasedHistoryRemoval) {
  RemoveHistoryTester tester(GetProfile());

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  tester.AddHistory(kOrigin1, base::Time::Now());
  tester.AddHistory(kOrigin2, two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin2));

  BlockUntilOriginDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_HISTORY, kOrigin2);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin1));
  EXPECT_FALSE(tester.HistoryContainsURL(kOrigin2));
}

TEST_F(BrowsingDataRemoverTest, OriginAndTimeBasedHistoryRemoval) {
  RemoveHistoryTester tester(GetProfile());

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  tester.AddHistory(kOrigin1, base::Time::Now());
  tester.AddHistory(kOrigin2, two_hours_ago);
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester.HistoryContainsURL(kOrigin2));

  BlockUntilOriginDataRemoved(BrowsingDataRemover::LAST_HOUR,
      BrowsingDataRemover::REMOVE_HISTORY, kOrigin2);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_HISTORY, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin1));
  EXPECT_TRUE(tester.HistoryContainsURL(kOrigin2));
}

// Verify that clearing autofill form data works.
TEST_F(BrowsingDataRemoverTest, AutofillRemovalLastHour) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());

  ASSERT_FALSE(tester.HasProfile());
  tester.AddProfile();
  ASSERT_TRUE(tester.HasProfile());

  BlockUntilBrowsingDataRemoved(
      BrowsingDataRemover::LAST_HOUR,
      BrowsingDataRemover::REMOVE_FORM_DATA, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FORM_DATA, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  ASSERT_FALSE(tester.HasProfile());
}

TEST_F(BrowsingDataRemoverTest, AutofillRemovalEverything) {
  GetProfile()->CreateWebDataService();
  RemoveAutofillTester tester(GetProfile());

  ASSERT_FALSE(tester.HasProfile());
  tester.AddProfile();
  ASSERT_TRUE(tester.HasProfile());

  BlockUntilBrowsingDataRemoved(
      BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_FORM_DATA, false);

  EXPECT_EQ(BrowsingDataRemover::REMOVE_FORM_DATA, GetRemovalMask());
  EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  ASSERT_FALSE(tester.HasProfile());
}
