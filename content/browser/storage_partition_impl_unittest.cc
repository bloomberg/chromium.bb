// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/gpu/shader_disk_cache.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/test_completion_callback.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/quota/mock_quota_manager.h"
#include "webkit/browser/quota/mock_special_storage_policy.h"
#include "webkit/browser/quota/quota_manager.h"

namespace content {
namespace {

const int kDefaultClientId = 42;
const char kCacheKey[] = "key";
const char kCacheValue[] = "cached value";

const char kTestOrigin1[] = "http://host1:1/";
const char kTestOrigin2[] = "http://host2:1/";
const char kTestOrigin3[] = "http://host3:1/";
const char kTestOriginDevTools[] = "chrome-devtools://abcdefghijklmnopqrstuvw/";

const GURL kOrigin1(kTestOrigin1);
const GURL kOrigin2(kTestOrigin2);
const GURL kOrigin3(kTestOrigin3);
const GURL kOriginDevTools(kTestOriginDevTools);

const base::FilePath::CharType kDomStorageOrigin1[] =
    FILE_PATH_LITERAL("http_host1_1.localstorage");

const base::FilePath::CharType kDomStorageOrigin2[] =
    FILE_PATH_LITERAL("http_host2_1.localstorage");

const base::FilePath::CharType kDomStorageOrigin3[] =
    FILE_PATH_LITERAL("http_host3_1.localstorage");

const quota::StorageType kTemporary = quota::kStorageTypeTemporary;
const quota::StorageType kPersistent = quota::kStorageTypePersistent;

const quota::QuotaClient::ID kClientFile = quota::QuotaClient::kFileSystem;

const uint32 kAllQuotaRemoveMask =
    StoragePartition::REMOVE_DATA_MASK_INDEXEDDB |
    StoragePartition::REMOVE_DATA_MASK_WEBSQL |
    StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
    StoragePartition::REMOVE_DATA_MASK_APPCACHE;

class TestClosureCallback {
 public:
  TestClosureCallback()
      : callback_(base::Bind(
          &TestClosureCallback::StopWaiting, base::Unretained(this))) {
  }

  void WaitForResult() {
    wait_run_loop_.reset(new base::RunLoop());
    wait_run_loop_->Run();
  }

  const base::Closure& callback() { return callback_; }

 private:
  void StopWaiting() {
    wait_run_loop_->Quit();
  }

  base::Closure callback_;
  scoped_ptr<base::RunLoop> wait_run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestClosureCallback);
};

class AwaitCompletionHelper {
 public:
  AwaitCompletionHelper() : start_(false), already_quit_(false) {}
  virtual ~AwaitCompletionHelper() {}

  void BlockUntilNotified() {
    if (!already_quit_) {
      DCHECK(!start_);
      start_ = true;
      base::MessageLoop::current()->Run();
    } else {
      DCHECK(!start_);
      already_quit_ = false;
    }
  }

  void Notify() {
    if (start_) {
      DCHECK(!already_quit_);
      base::MessageLoop::current()->Quit();
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

  DISALLOW_COPY_AND_ASSIGN(AwaitCompletionHelper);
};

class RemoveCookieTester {
 public:
  explicit RemoveCookieTester(TestBrowserContext* context)
      : get_cookie_success_(false), monster_(NULL) {
    SetMonster(context->GetRequestContext()->GetURLRequestContext()->
                   cookie_store()->GetCookieMonster());
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

class RemoveLocalStorageTester {
 public:
  explicit RemoveLocalStorageTester(TestBrowserContext* profile)
      : profile_(profile), dom_storage_context_(NULL) {
    dom_storage_context_ =
        content::BrowserContext::GetDefaultStoragePartition(profile)->
            GetDOMStorageContext();
  }

  // Returns true, if the given origin URL exists.
  bool DOMStorageExistsForOrigin(const GURL& origin) {
    GetLocalStorageUsage();
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
    base::FilePath storage_path =
        profile_->GetPath().AppendASCII("Local Storage");
    base::CreateDirectory(storage_path);

    // Write some files.
    file_util::WriteFile(storage_path.Append(kDomStorageOrigin1), NULL, 0);
    file_util::WriteFile(storage_path.Append(kDomStorageOrigin2), NULL, 0);
    file_util::WriteFile(storage_path.Append(kDomStorageOrigin3), NULL, 0);

    // Tweak their dates.
    base::Time now = base::Time::Now();
    base::TouchFile(storage_path.Append(kDomStorageOrigin1), now, now);

    base::Time one_day_ago = now - base::TimeDelta::FromDays(1);
    base::TouchFile(storage_path.Append(kDomStorageOrigin2),
                    one_day_ago, one_day_ago);

    base::Time sixty_days_ago = now - base::TimeDelta::FromDays(60);
    base::TouchFile(storage_path.Append(kDomStorageOrigin3),
                    sixty_days_ago, sixty_days_ago);
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
    await_completion_.Notify();
  }

  // We don't own these pointers.
  TestBrowserContext* profile_;
  content::DOMStorageContext* dom_storage_context_;

  std::vector<content::LocalStorageUsageInfo> infos_;

  AwaitCompletionHelper await_completion_;

  DISALLOW_COPY_AND_ASSIGN(RemoveLocalStorageTester);
};

bool IsWebSafeSchemeForTest(const std::string& scheme) {
  return scheme == "http";
}

bool DoesOriginMatchForUnprotectedWeb(
    const GURL& origin,
    quota::SpecialStoragePolicy* special_storage_policy) {
  if (IsWebSafeSchemeForTest(origin.scheme()))
    return !special_storage_policy->IsStorageProtected(origin.GetOrigin());

  return false;
}

bool DoesOriginMatchForBothProtectedAndUnprotectedWeb(
    const GURL& origin,
    quota::SpecialStoragePolicy* special_storage_policy) {
  return true;
}

bool DoesOriginMatchUnprotected(
    const GURL& origin,
    quota::SpecialStoragePolicy* special_storage_policy) {
  return origin.GetOrigin().scheme() != kOriginDevTools.scheme();
}

void ClearQuotaData(content::StoragePartition* storage_partition,
                    const base::Closure& cb) {
  storage_partition->ClearData(
      kAllQuotaRemoveMask,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      NULL, StoragePartition::OriginMatcherFunction(),
      base::Time(), base::Time::Max(), cb);
}

void ClearQuotaDataWithOriginMatcher(
    content::StoragePartition* storage_partition,
    const GURL& remove_origin,
    const StoragePartition::OriginMatcherFunction& origin_matcher,
    const base::Time delete_begin,
    const base::Closure& cb) {
  storage_partition->ClearData(kAllQuotaRemoveMask,
                               StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
                               &remove_origin, origin_matcher, delete_begin,
                               base::Time::Max(), cb);
}

void ClearQuotaDataForOrigin(
    content::StoragePartition* storage_partition,
    const GURL& remove_origin,
    const base::Time delete_begin,
    const base::Closure& cb) {
  ClearQuotaDataWithOriginMatcher(
      storage_partition, remove_origin,
      StoragePartition::OriginMatcherFunction(), delete_begin, cb);
}

void ClearQuotaDataForNonPersistent(
    content::StoragePartition* storage_partition,
    const base::Time delete_begin,
    const base::Closure& cb) {
  uint32 quota_storage_remove_mask_no_persistent =
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL &
      ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT;
  storage_partition->ClearData(
      kAllQuotaRemoveMask, quota_storage_remove_mask_no_persistent,
      NULL, StoragePartition::OriginMatcherFunction(),
      delete_begin, base::Time::Max(), cb);
}

void ClearCookies(content::StoragePartition* storage_partition,
                  const base::Time delete_begin,
                  const base::Time delete_end,
                  const base::Closure& cb) {
  storage_partition->ClearData(
      StoragePartition::REMOVE_DATA_MASK_COOKIES,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      NULL, StoragePartition::OriginMatcherFunction(),
      delete_begin, delete_end, cb);
}

void ClearStuff(uint32 remove_mask,
                content::StoragePartition* storage_partition,
                const base::Time delete_begin,
                const base::Time delete_end,
                const StoragePartition::OriginMatcherFunction& origin_matcher,
                const base::Closure& cb) {
  storage_partition->ClearData(
      remove_mask, StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      NULL, origin_matcher, delete_begin, delete_end, cb);
}

}  // namespace

class StoragePartitionImplTest : public testing::Test {
 public:
  StoragePartitionImplTest()
      : browser_context_(new TestBrowserContext()),
        thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
  }
  virtual ~StoragePartitionImplTest() {}

  quota::MockQuotaManager* GetMockManager() {
    if (!quota_manager_.get()) {
      quota_manager_ = new quota::MockQuotaManager(
          browser_context_->IsOffTheRecord(),
          browser_context_->GetPath(),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO).get(),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB).get(),
          browser_context_->GetSpecialStoragePolicy());
    }
    return quota_manager_.get();
  }

  TestBrowserContext* GetBrowserContext() {
    return browser_context_.get();
  }

 private:
  scoped_ptr<TestBrowserContext> browser_context_;
  scoped_refptr<quota::MockQuotaManager> quota_manager_;
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(StoragePartitionImplTest);
};

class StoragePartitionShaderClearTest : public testing::Test {
 public:
  StoragePartitionShaderClearTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
  }

  virtual ~StoragePartitionShaderClearTest() {}

  const base::FilePath& cache_path() { return temp_dir_.path(); }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ShaderCacheFactory::GetInstance()->SetCacheInfo(kDefaultClientId,
                                                    cache_path());

    cache_ = ShaderCacheFactory::GetInstance()->Get(kDefaultClientId);
    ASSERT_TRUE(cache_.get() != NULL);
  }

  void InitCache() {
    net::TestCompletionCallback available_cb;
    int rv = cache_->SetAvailableCallback(available_cb.callback());
    ASSERT_EQ(net::OK, available_cb.GetResult(rv));
    EXPECT_EQ(0, cache_->Size());

    cache_->Cache(kCacheKey, kCacheValue);

    net::TestCompletionCallback complete_cb;

    rv = cache_->SetCacheCompleteCallback(complete_cb.callback());
    ASSERT_EQ(net::OK, complete_cb.GetResult(rv));
  }

  size_t Size() { return cache_->Size(); }

 private:
  virtual void TearDown() OVERRIDE {
    cache_ = NULL;
    ShaderCacheFactory::GetInstance()->RemoveCacheInfo(kDefaultClientId);
  }

  base::ScopedTempDir temp_dir_;
  content::TestBrowserThreadBundle thread_bundle_;

  scoped_refptr<ShaderDiskCache> cache_;
};

void ClearData(content::StoragePartitionImpl* sp,
               const base::Closure& cb) {
  base::Time time;
  sp->ClearData(
      StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      NULL, StoragePartition::OriginMatcherFunction(),
      time, time, cb);
}

// Tests ---------------------------------------------------------------------

TEST_F(StoragePartitionShaderClearTest, ClearShaderCache) {
  InitCache();
  EXPECT_EQ(1u, Size());

  TestClosureCallback clear_cb;
  StoragePartitionImpl storage_partition(
      cache_path(), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ClearData, &storage_partition,
                            clear_cb.callback()));
  clear_cb.WaitForResult();
  EXPECT_EQ(0u, Size());
}

TEST_F(StoragePartitionImplTest, QuotaClientMaskGeneration) {
  EXPECT_EQ(quota::QuotaClient::kFileSystem,
            StoragePartitionImpl::GenerateQuotaClientMask(
                StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS));
  EXPECT_EQ(quota::QuotaClient::kDatabase,
            StoragePartitionImpl::GenerateQuotaClientMask(
                StoragePartition::REMOVE_DATA_MASK_WEBSQL));
  EXPECT_EQ(quota::QuotaClient::kAppcache,
            StoragePartitionImpl::GenerateQuotaClientMask(
                StoragePartition::REMOVE_DATA_MASK_APPCACHE));
  EXPECT_EQ(quota::QuotaClient::kIndexedDatabase,
            StoragePartitionImpl::GenerateQuotaClientMask(
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB));
  EXPECT_EQ(quota::QuotaClient::kFileSystem |
            quota::QuotaClient::kDatabase |
            quota::QuotaClient::kAppcache |
            quota::QuotaClient::kIndexedDatabase,
            StoragePartitionImpl::GenerateQuotaClientMask(
                StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB));
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

void PopulateTestQuotaManagedNonBrowsingData(quota::MockQuotaManager* manager) {
  manager->AddOrigin(kOriginDevTools, kTemporary, kClientFile, base::Time());
  manager->AddOrigin(kOriginDevTools, kPersistent, kClientFile, base::Time());
}

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverBoth) {
  PopulateTestQuotaManagedData(GetMockManager());

  TestClosureCallback clear_cb;
  StoragePartitionImpl sp(
      base::FilePath(), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  static_cast<StoragePartitionImpl*>(&sp)->OverrideQuotaManagerForTesting(
      GetMockManager());
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ClearQuotaData, &sp, clear_cb.callback()));
  clear_cb.WaitForResult();

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

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverOnlyTemporary) {
  PopulateTestQuotaManagedTemporaryData(GetMockManager());

  TestClosureCallback clear_cb;
  StoragePartitionImpl sp(
      base::FilePath(), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  static_cast<StoragePartitionImpl*>(&sp)->OverrideQuotaManagerForTesting(
      GetMockManager());
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ClearQuotaData, &sp, clear_cb.callback()));
  clear_cb.WaitForResult();

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

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverOnlyPersistent) {
  PopulateTestQuotaManagedPersistentData(GetMockManager());

  TestClosureCallback clear_cb;
  StoragePartitionImpl sp(
      base::FilePath(), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  static_cast<StoragePartitionImpl*>(&sp)->OverrideQuotaManagerForTesting(
      GetMockManager());
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ClearQuotaData, &sp, clear_cb.callback()));
  clear_cb.WaitForResult();

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

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverNeither) {
  TestClosureCallback clear_cb;
  StoragePartitionImpl sp(
      base::FilePath(), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  static_cast<StoragePartitionImpl*>(&sp)->OverrideQuotaManagerForTesting(
      GetMockManager());
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ClearQuotaData, &sp, clear_cb.callback()));
  clear_cb.WaitForResult();

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

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForeverSpecificOrigin) {
  PopulateTestQuotaManagedData(GetMockManager());

  TestClosureCallback clear_cb;
  StoragePartitionImpl sp(
      base::FilePath(), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  static_cast<StoragePartitionImpl*>(&sp)->OverrideQuotaManagerForTesting(
      GetMockManager());
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ClearQuotaDataForOrigin,
                            &sp, kOrigin1, base::Time(), clear_cb.callback()));
  clear_cb.WaitForResult();

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

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForLastHour) {
  PopulateTestQuotaManagedData(GetMockManager());

  TestClosureCallback clear_cb;
  StoragePartitionImpl sp(
      base::FilePath(), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  static_cast<StoragePartitionImpl*>(&sp)->OverrideQuotaManagerForTesting(
      GetMockManager());
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ClearQuotaDataForOrigin,
                            &sp, GURL(),
                            base::Time::Now() - base::TimeDelta::FromHours(1),
                            clear_cb.callback()));
  clear_cb.WaitForResult();

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

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedDataForLastWeek) {
  PopulateTestQuotaManagedData(GetMockManager());

  TestClosureCallback clear_cb;
  StoragePartitionImpl sp(
      base::FilePath(), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  static_cast<StoragePartitionImpl*>(&sp)->OverrideQuotaManagerForTesting(
      GetMockManager());
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ClearQuotaDataForNonPersistent,
                            &sp,
                            base::Time::Now() - base::TimeDelta::FromDays(7),
                            clear_cb.callback()));
  clear_cb.WaitForResult();

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

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedUnprotectedOrigins) {
  // Protect kOrigin1.
  scoped_refptr<quota::MockSpecialStoragePolicy> mock_policy =
      new quota::MockSpecialStoragePolicy;
  mock_policy->AddProtected(kOrigin1.GetOrigin());

  PopulateTestQuotaManagedData(GetMockManager());

  TestClosureCallback clear_cb;
  StoragePartitionImpl sp(
      base::FilePath(), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  static_cast<StoragePartitionImpl*>(&sp)->OverrideQuotaManagerForTesting(
      GetMockManager());
  static_cast<StoragePartitionImpl*>(
      &sp)->OverrideSpecialStoragePolicyForTesting(mock_policy);
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ClearQuotaDataWithOriginMatcher,
                            &sp, GURL(),
                            base::Bind(&DoesOriginMatchForUnprotectedWeb),
                            base::Time(), clear_cb.callback()));
  clear_cb.WaitForResult();

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

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedProtectedSpecificOrigin) {
  // Protect kOrigin1.
  scoped_refptr<quota::MockSpecialStoragePolicy> mock_policy =
      new quota::MockSpecialStoragePolicy;
  mock_policy->AddProtected(kOrigin1.GetOrigin());

  PopulateTestQuotaManagedData(GetMockManager());

  TestClosureCallback clear_cb;
  StoragePartitionImpl sp(
      base::FilePath(), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  static_cast<StoragePartitionImpl*>(&sp)->OverrideQuotaManagerForTesting(
      GetMockManager());
  static_cast<StoragePartitionImpl*>(
      &sp)->OverrideSpecialStoragePolicyForTesting(mock_policy);

  // Try to remove kOrigin1. Expect failure.
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ClearQuotaDataWithOriginMatcher,
                            &sp, kOrigin1,
                            base::Bind(&DoesOriginMatchForUnprotectedWeb),
                            base::Time(), clear_cb.callback()));
  clear_cb.WaitForResult();

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

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedProtectedOrigins) {
  // Protect kOrigin1.
  scoped_refptr<quota::MockSpecialStoragePolicy> mock_policy =
      new quota::MockSpecialStoragePolicy;
  mock_policy->AddProtected(kOrigin1.GetOrigin());

  PopulateTestQuotaManagedData(GetMockManager());

  // Try to remove kOrigin1. Expect success.
  TestClosureCallback clear_cb;
  StoragePartitionImpl sp(
      base::FilePath(), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  static_cast<StoragePartitionImpl*>(&sp)->OverrideQuotaManagerForTesting(
      GetMockManager());
  static_cast<StoragePartitionImpl*>(
      &sp)->OverrideSpecialStoragePolicyForTesting(mock_policy);
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ClearQuotaDataWithOriginMatcher,
                 &sp, GURL(),
                 base::Bind(&DoesOriginMatchForBothProtectedAndUnprotectedWeb),
                 base::Time(), clear_cb.callback()));
  clear_cb.WaitForResult();

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

TEST_F(StoragePartitionImplTest, RemoveQuotaManagedIgnoreDevTools) {
  PopulateTestQuotaManagedNonBrowsingData(GetMockManager());

  TestClosureCallback clear_cb;
  StoragePartitionImpl sp(
      base::FilePath(), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  static_cast<StoragePartitionImpl*>(&sp)->OverrideQuotaManagerForTesting(
      GetMockManager());
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ClearQuotaDataWithOriginMatcher,
                            &sp, GURL(),
                            base::Bind(&DoesOriginMatchUnprotected),
                            base::Time(), clear_cb.callback()));
  clear_cb.WaitForResult();

  // Check that devtools data isn't removed.
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOriginDevTools, kTemporary,
      kClientFile));
  EXPECT_TRUE(GetMockManager()->OriginHasData(kOriginDevTools, kPersistent,
      kClientFile));
}

TEST_F(StoragePartitionImplTest, RemoveCookieForever) {
  RemoveCookieTester tester(GetBrowserContext());

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  TestClosureCallback clear_cb;
  StoragePartitionImpl sp(
      base::FilePath(), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  sp.SetURLRequestContext(GetBrowserContext()->GetRequestContext());
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ClearCookies,
                            &sp, base::Time(), base::Time::Max(),
                            clear_cb.callback()));
  clear_cb.WaitForResult();

  EXPECT_FALSE(tester.ContainsCookie());
}

TEST_F(StoragePartitionImplTest, RemoveCookieLastHour) {
  RemoveCookieTester tester(GetBrowserContext());

  tester.AddCookie();
  ASSERT_TRUE(tester.ContainsCookie());

  TestClosureCallback clear_cb;
  StoragePartitionImpl sp(
      base::FilePath(), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  base::Time an_hour_ago = base::Time::Now() - base::TimeDelta::FromHours(1);
  sp.SetURLRequestContext(GetBrowserContext()->GetRequestContext());
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&ClearCookies,
                            &sp, an_hour_ago, base::Time::Max(),
                            clear_cb.callback()));
  clear_cb.WaitForResult();

  EXPECT_FALSE(tester.ContainsCookie());
}

TEST_F(StoragePartitionImplTest, RemoveUnprotectedLocalStorageForever) {
  // Protect kOrigin1.
  scoped_refptr<quota::MockSpecialStoragePolicy> mock_policy =
      new quota::MockSpecialStoragePolicy;
  mock_policy->AddProtected(kOrigin1.GetOrigin());

  RemoveLocalStorageTester tester(GetBrowserContext());

  tester.AddDOMStorageTestData();
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin3));

  TestClosureCallback clear_cb;
  DOMStorageContextWrapper* dom_storage_context =
      static_cast<DOMStorageContextWrapper*>(
          content::BrowserContext::GetDefaultStoragePartition(
              GetBrowserContext())->GetDOMStorageContext());
  StoragePartitionImpl sp(base::FilePath(), NULL, NULL, NULL, NULL,
                          dom_storage_context, NULL, NULL, NULL, NULL);
  static_cast<StoragePartitionImpl*>(
      &sp)->OverrideSpecialStoragePolicyForTesting(mock_policy);
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ClearStuff,
                 StoragePartitionImpl::REMOVE_DATA_MASK_LOCAL_STORAGE,
                 &sp, base::Time(), base::Time::Max(),
                 base::Bind(&DoesOriginMatchForUnprotectedWeb),
                 clear_cb.callback()));
  clear_cb.WaitForResult();

  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin3));
}

TEST_F(StoragePartitionImplTest, RemoveProtectedLocalStorageForever) {
  // Protect kOrigin1.
  scoped_refptr<quota::MockSpecialStoragePolicy> mock_policy =
      new quota::MockSpecialStoragePolicy;
  mock_policy->AddProtected(kOrigin1.GetOrigin());

  RemoveLocalStorageTester tester(GetBrowserContext());

  tester.AddDOMStorageTestData();
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin3));

  TestClosureCallback clear_cb;
  DOMStorageContextWrapper* dom_storage_context =
      static_cast<DOMStorageContextWrapper*>(
          content::BrowserContext::GetDefaultStoragePartition(
              GetBrowserContext())->GetDOMStorageContext());
  StoragePartitionImpl sp(base::FilePath(), NULL, NULL, NULL, NULL,
                          dom_storage_context, NULL, NULL, NULL, NULL);
  static_cast<StoragePartitionImpl*>(
      &sp)->OverrideSpecialStoragePolicyForTesting(mock_policy);
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ClearStuff,
                 StoragePartitionImpl::REMOVE_DATA_MASK_LOCAL_STORAGE,
                 &sp, base::Time(), base::Time::Max(),
                 base::Bind(&DoesOriginMatchForBothProtectedAndUnprotectedWeb),
                 clear_cb.callback()));
  clear_cb.WaitForResult();

  // Even if kOrigin1 is protected, it will be deleted since we specify
  // ClearData to delete protected data.
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin3));
}

TEST_F(StoragePartitionImplTest, RemoveLocalStorageForLastWeek) {
  RemoveLocalStorageTester tester(GetBrowserContext());

  tester.AddDOMStorageTestData();
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin3));

  TestClosureCallback clear_cb;
  DOMStorageContextWrapper* dom_storage_context =
      static_cast<DOMStorageContextWrapper*>(
          content::BrowserContext::GetDefaultStoragePartition(
              GetBrowserContext())->GetDOMStorageContext());
  StoragePartitionImpl sp(base::FilePath(), NULL, NULL, NULL, NULL,
                          dom_storage_context, NULL, NULL, NULL, NULL);
  base::Time a_week_ago = base::Time::Now() - base::TimeDelta::FromDays(7);
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ClearStuff,
                 StoragePartitionImpl::REMOVE_DATA_MASK_LOCAL_STORAGE,
                 &sp, a_week_ago, base::Time::Max(),
                 base::Bind(&DoesOriginMatchForBothProtectedAndUnprotectedWeb),
                 clear_cb.callback()));
  clear_cb.WaitForResult();

  // kOrigin1 and kOrigin2 do not have age more than a week.
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin1));
  EXPECT_FALSE(tester.DOMStorageExistsForOrigin(kOrigin2));
  EXPECT_TRUE(tester.DOMStorageExistsForOrigin(kOrigin3));
}

}  // namespace content
