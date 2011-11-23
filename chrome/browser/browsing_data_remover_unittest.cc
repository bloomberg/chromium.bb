// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_remover.h"

#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "chrome/browser/extensions/mock_extension_special_storage_policy.h"
#include "chrome/browser/history/history.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "net/base/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/quota/mock_quota_manager.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/quota_types.h"

using content::BrowserThread;

namespace {

const char kTestkOrigin1[] = "http://host1:1/";
const char kTestkOrigin2[] = "http://host2:1/";
const char kTestkOrigin3[] = "http://host3:1/";

const GURL kOrigin1(kTestkOrigin1);
const GURL kOrigin2(kTestkOrigin2);
const GURL kOrigin3(kTestkOrigin3);

class BrowsingDataRemoverTester : public BrowsingDataRemover::Observer {
 public:
  BrowsingDataRemoverTester()
      : start_(false),
        already_quit_(false) {}
  virtual ~BrowsingDataRemoverTester() {}

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

 protected:
  // BrowsingDataRemover::Observer implementation.
  virtual void OnBrowsingDataRemoverDone() {
    Notify();
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

 private:
  // Helps prevent from running message_loop, if the callback invoked
  // immediately.
  bool start_;
  bool already_quit_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverTester);
};

// Testers -------------------------------------------------------------------

class RemoveCookieTester : public BrowsingDataRemoverTester {
 public:
  explicit RemoveCookieTester(TestingProfile* profile)
      : get_cookie_success_(false) {
    profile->CreateRequestContext();
    monster_ = profile->GetRequestContext()->GetURLRequestContext()->
        cookie_store()->GetCookieMonster();
  }

  // Returns true, if the given cookie exists in the cookie store.
  bool ContainsCookie() {
    get_cookie_success_ = false;
    monster_->GetCookiesWithOptionsAsync(
        kOrigin1, net::CookieOptions(),
        base::Bind(&RemoveCookieTester::GetCookieCallback,
                   base::Unretained(this)));
    BlockUntilNotified();
    return get_cookie_success_;
  }

  void AddCookie() {
    monster_->SetCookieWithOptionsAsync(
        kOrigin1, "A=1", net::CookieOptions(),
        base::Bind(&RemoveCookieTester::SetCookieCallback,
                   base::Unretained(this)));
    BlockUntilNotified();
  }

 private:
  void GetCookieCallback(const std::string& cookies) {
    if (cookies == "A=1") {
      get_cookie_success_ = true;
    } else {
      EXPECT_EQ(cookies, "");
      get_cookie_success_ = false;
    }
    Notify();
  }

  void SetCookieCallback(bool result) {
    ASSERT_TRUE(result);
    Notify();
  }

  bool get_cookie_success_;

  net::CookieStore* monster_;

  DISALLOW_COPY_AND_ASSIGN(RemoveCookieTester);
};

class RemoveHistoryTester : public BrowsingDataRemoverTester {
 public:
  explicit RemoveHistoryTester(TestingProfile* profile)
      : query_url_success_(false) {
    profile->CreateHistoryService(true, false);
    history_service_ = profile->GetHistoryService(Profile::EXPLICIT_ACCESS);
  }

  // Returns true, if the given URL exists in the history service.
  bool HistoryContainsURL(const GURL& url) {
    history_service_->QueryURL(
        url,
        true,
        &consumer_,
        base::Bind(&RemoveHistoryTester::SaveResultAndQuit,
                   base::Unretained(this)));
    BlockUntilNotified();
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
    Notify();
  }


  // For History requests.
  CancelableRequestConsumer consumer_;
  bool query_url_success_;

  // TestingProfile owns the history service; we shouldn't delete it.
  HistoryService* history_service_;

  DISALLOW_COPY_AND_ASSIGN(RemoveHistoryTester);
};

class RemoveQuotaManagedDataTester : public BrowsingDataRemoverTester {
 public:
  RemoveQuotaManagedDataTester() {}
  virtual ~RemoveQuotaManagedDataTester() {}

  void PopulateTestQuotaManagedData(quota::MockQuotaManager* manager) {
    // Set up kOrigin1 with a temporary quota, kOrigin2 with a persistent
    // quota, and kOrigin3 with both. kOrigin1 is modified now, kOrigin2
    // is modified at the beginning of time, and kOrigin3 is modified one day
    // ago.
    PopulateTestQuotaManagedPersistentData(manager);
    PopulateTestQuotaManagedTemporaryData(manager);
  }

  void PopulateTestQuotaManagedPersistentData(
      quota::MockQuotaManager* manager) {
    manager->AddOrigin(kOrigin2, quota::kStorageTypePersistent,
        base::Time());
    manager->AddOrigin(kOrigin3, quota::kStorageTypePersistent,
        base::Time::Now() - base::TimeDelta::FromDays(1));

    EXPECT_FALSE(manager->OriginHasData(kOrigin1,
        quota::kStorageTypePersistent));
    EXPECT_TRUE(manager->OriginHasData(kOrigin2,
        quota::kStorageTypePersistent));
    EXPECT_TRUE(manager->OriginHasData(kOrigin3,
        quota::kStorageTypePersistent));
  }

  void PopulateTestQuotaManagedTemporaryData(quota::MockQuotaManager* manager) {
    manager->AddOrigin(kOrigin1, quota::kStorageTypeTemporary,
        base::Time::Now());
    manager->AddOrigin(kOrigin3, quota::kStorageTypeTemporary,
        base::Time::Now() - base::TimeDelta::FromDays(1));

    EXPECT_TRUE(manager->OriginHasData(kOrigin1,
        quota::kStorageTypeTemporary));
    EXPECT_FALSE(manager->OriginHasData(kOrigin2,
        quota::kStorageTypeTemporary));
    EXPECT_TRUE(manager->OriginHasData(kOrigin3,
        quota::kStorageTypeTemporary));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoveQuotaManagedDataTester);
};

// Test Class ----------------------------------------------------------------

class BrowsingDataRemoverTest : public testing::Test {
 public:
  BrowsingDataRemoverTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB, &message_loop_),
        webkit_thread_(BrowserThread::WEBKIT, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_),
        profile_(new TestingProfile()) {
  }

  virtual ~BrowsingDataRemoverTest() {
  }

  void TearDown() {
    // TestingProfile contains a WebKitContext.  WebKitContext's destructor
    // posts a message to the WEBKIT thread to delete some of its member
    // variables. We need to ensure that the profile is destroyed, and that
    // the message loop is cleared out, before destroying the threads and loop.
    // Otherwise we leak memory.
    profile_.reset();
    message_loop_.RunAllPending();
  }

  void BlockUntilBrowsingDataRemoved(BrowsingDataRemover::TimePeriod period,
                                     int remove_mask,
                                     BrowsingDataRemoverTester* tester) {
    BrowsingDataRemover* remover = new BrowsingDataRemover(
        profile_.get(), period,
        base::Time::Now() + base::TimeDelta::FromMilliseconds(10));
    remover->AddObserver(tester);

    // BrowsingDataRemover deletes itself when it completes.
    remover->Remove(remove_mask);
    tester->BlockUntilNotified();
  }

  TestingProfile* GetProfile() {
    return profile_.get();
  }

  quota::MockQuotaManager* GetMockManager() {
    if (profile_->GetQuotaManager() == NULL) {
      profile_->SetQuotaManager(new quota::MockQuotaManager(
        profile_->IsOffTheRecord(),
        profile_->GetPath(),
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB),
        profile_->GetExtensionSpecialStoragePolicy()));
    }
    return (quota::MockQuotaManager*) profile_->GetQuotaManager();
  }

 private:
  // message_loop_, as well as all the threads associated with it must be
  // defined before profile_ to prevent explosions. Oh how I love C++.
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  content::TestBrowserThread webkit_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;
  scoped_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverTest);
};

// Tests ---------------------------------------------------------------------

TEST_F(BrowsingDataRemoverTest, RemoveCookieForever) {
  scoped_ptr<RemoveCookieTester> tester(
      new RemoveCookieTester(GetProfile()));

  tester->AddCookie();
  ASSERT_TRUE(tester->ContainsCookie());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_COOKIES, tester.get());

  EXPECT_FALSE(tester->ContainsCookie());
}

TEST_F(BrowsingDataRemoverTest, RemoveHistoryForever) {
  scoped_ptr<RemoveHistoryTester> tester(
      new RemoveHistoryTester(GetProfile()));

  tester->AddHistory(kOrigin1, base::Time::Now());
  ASSERT_TRUE(tester->HistoryContainsURL(kOrigin1));

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_HISTORY, tester.get());

  EXPECT_FALSE(tester->HistoryContainsURL(kOrigin1));
}

TEST_F(BrowsingDataRemoverTest, RemoveHistoryForLastHour) {
  scoped_ptr<RemoveHistoryTester> tester(
      new RemoveHistoryTester(GetProfile()));

  base::Time two_hours_ago = base::Time::Now() - base::TimeDelta::FromHours(2);

  tester->AddHistory(kOrigin1, base::Time::Now());
  tester->AddHistory(kOrigin2, two_hours_ago);
  ASSERT_TRUE(tester->HistoryContainsURL(kOrigin1));
  ASSERT_TRUE(tester->HistoryContainsURL(kOrigin2));

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_HOUR,
      BrowsingDataRemover::REMOVE_HISTORY, tester.get());

  EXPECT_FALSE(tester->HistoryContainsURL(kOrigin1));
  EXPECT_TRUE(tester->HistoryContainsURL(kOrigin2));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverBoth) {
  scoped_ptr<RemoveQuotaManagedDataTester> tester(
      new RemoveQuotaManagedDataTester());

  tester->PopulateTestQuotaManagedData(GetMockManager());
  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_SITE_DATA &
      ~BrowsingDataRemover::REMOVE_LSO_DATA, tester.get());

  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1,
      quota::kStorageTypePersistent));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2,
      quota::kStorageTypePersistent));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3,
      quota::kStorageTypePersistent));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverOnlyTemporary) {
  scoped_ptr<RemoveQuotaManagedDataTester> tester(
      new RemoveQuotaManagedDataTester());

  tester->PopulateTestQuotaManagedTemporaryData(GetMockManager());
  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_SITE_DATA &
      ~BrowsingDataRemover::REMOVE_LSO_DATA, tester.get());

  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1,
      quota::kStorageTypePersistent));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2,
      quota::kStorageTypePersistent));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3,
      quota::kStorageTypePersistent));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverOnlyPersistent) {
  scoped_ptr<RemoveQuotaManagedDataTester> tester(
      new RemoveQuotaManagedDataTester());

  tester->PopulateTestQuotaManagedPersistentData(GetMockManager());
  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_SITE_DATA &
      ~BrowsingDataRemover::REMOVE_LSO_DATA, tester.get());

  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1,
      quota::kStorageTypePersistent));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2,
      quota::kStorageTypePersistent));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3,
      quota::kStorageTypePersistent));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForeverNeither) {
  scoped_ptr<RemoveQuotaManagedDataTester> tester(
      new RemoveQuotaManagedDataTester());

  GetMockManager();  // Creates the QuotaManager instance.
  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_SITE_DATA &
      ~BrowsingDataRemover::REMOVE_LSO_DATA, tester.get());

  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1,
      quota::kStorageTypePersistent));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2,
      quota::kStorageTypePersistent));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3,
      quota::kStorageTypePersistent));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForLastHour) {
  scoped_ptr<RemoveQuotaManagedDataTester> tester(
      new RemoveQuotaManagedDataTester());
  tester->PopulateTestQuotaManagedData(GetMockManager());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_HOUR,
      BrowsingDataRemover::REMOVE_SITE_DATA &
      ~BrowsingDataRemover::REMOVE_LSO_DATA, tester.get());

  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2,
      quota::kStorageTypeTemporary));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin3,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1,
      quota::kStorageTypePersistent));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin2,
      quota::kStorageTypePersistent));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin3,
      quota::kStorageTypePersistent));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedDataForLastWeek) {
  scoped_ptr<RemoveQuotaManagedDataTester> tester(
      new RemoveQuotaManagedDataTester());
  tester->PopulateTestQuotaManagedData(GetMockManager());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::LAST_WEEK,
      BrowsingDataRemover::REMOVE_SITE_DATA &
      ~BrowsingDataRemover::REMOVE_LSO_DATA, tester.get());

  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1,
      quota::kStorageTypePersistent));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin2,
      quota::kStorageTypePersistent));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin3,
      quota::kStorageTypePersistent));
}

TEST_F(BrowsingDataRemoverTest, RemoveQuotaManagedUnprotectedOrigins) {
  // Protect kOrigin1.
  scoped_refptr<MockExtensionSpecialStoragePolicy> mock_policy =
          new MockExtensionSpecialStoragePolicy;
  mock_policy->AddProtected(kOrigin1.GetOrigin());
  GetProfile()->SetExtensionSpecialStoragePolicy(mock_policy);

  scoped_ptr<RemoveQuotaManagedDataTester> tester(
      new RemoveQuotaManagedDataTester());
  tester->PopulateTestQuotaManagedData(GetMockManager());

  BlockUntilBrowsingDataRemoved(BrowsingDataRemover::EVERYTHING,
      BrowsingDataRemover::REMOVE_SITE_DATA &
      ~BrowsingDataRemover::REMOVE_LSO_DATA, tester.get());

  EXPECT_TRUE(GetMockManager()->OriginHasData(kOrigin1,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3,
      quota::kStorageTypeTemporary));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin1,
      quota::kStorageTypePersistent));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin2,
      quota::kStorageTypePersistent));
  EXPECT_FALSE(GetMockManager()->OriginHasData(kOrigin3,
      quota::kStorageTypePersistent));
}

}  // namespace
