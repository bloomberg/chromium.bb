// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/url_blacklist_manager.h"

#include <ostream>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;

class TestingURLBlacklistManager : public URLBlacklistManager {
 public:
  explicit TestingURLBlacklistManager(PrefService* pref_service)
      : URLBlacklistManager(pref_service) {
  }

  virtual ~TestingURLBlacklistManager() {
  }

  // Make this method public for testing.
  using URLBlacklistManager::ScheduleUpdate;

  // Post tasks without a delay during tests.
  virtual void PostUpdateTask(const base::Closure& task) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, task);
  }

  // Makes a direct call to UpdateOnIO during tests.
  void UpdateOnIO() {
    StringVector* block = new StringVector;
    block->push_back("example.com");
    StringVector* allow = new StringVector;
    URLBlacklistManager::UpdateOnIO(block, allow);
  }

  void UpdateNotMocked() {
    URLBlacklistManager::Update();
  }

  MOCK_METHOD0(Update, void());
  MOCK_METHOD1(SetBlacklist, void(URLBlacklist*));

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingURLBlacklistManager);
};

class URLBlacklistManagerTest : public testing::Test {
 protected:
  URLBlacklistManagerTest()
      : ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_),
        io_thread_(BrowserThread::IO, &loop_) {
  }

  virtual void SetUp() OVERRIDE {
    pref_service_.RegisterListPref(prefs::kUrlBlacklist);
    pref_service_.RegisterListPref(prefs::kUrlWhitelist);
    blacklist_manager_.reset(
        new TestingURLBlacklistManager(&pref_service_));
    loop_.RunAllPending();
  }

  virtual void TearDown() OVERRIDE {
    if (blacklist_manager_.get())
      blacklist_manager_->ShutdownOnUIThread();
    loop_.RunAllPending();
    // Delete |blacklist_manager_| while |io_thread_| is mapping IO to
    // |loop_|.
    blacklist_manager_.reset();
  }

  void ExpectUpdate() {
    EXPECT_CALL(*blacklist_manager_, Update())
        .WillOnce(Invoke(blacklist_manager_.get(),
                         &TestingURLBlacklistManager::UpdateNotMocked));
  }

  MessageLoop loop_;
  TestingPrefService pref_service_;
  scoped_ptr<TestingURLBlacklistManager> blacklist_manager_;

 private:
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
  BrowserThread io_thread_;

  DISALLOW_COPY_AND_ASSIGN(URLBlacklistManagerTest);
};

// Parameters for the FilterToComponents test.
struct FilterTestParams {
 public:
  FilterTestParams(const std::string& filter, const std::string& scheme,
                   const std::string& host, uint16 port,
                   const std::string& path)
      : filter_(filter), scheme_(scheme), host_(host), port_(port),
        path_(path) {}

  FilterTestParams(const FilterTestParams& params)
      : filter_(params.filter_), scheme_(params.scheme_), host_(params.host_),
        port_(params.port_), path_(params.path_) {}

  const FilterTestParams& operator=(const FilterTestParams& params) {
    filter_ = params.filter_;
    scheme_ = params.scheme_;
    host_ = params.host_;
    port_ = params.port_;
    path_ = params.path_;
    return *this;
  }

  const std::string& filter() const { return filter_; }
  const std::string& scheme() const { return scheme_; }
  const std::string& host() const { return host_; }
  uint16 port() const { return port_; }
  const std::string& path() const { return path_; }

 private:
  std::string filter_;
  std::string scheme_;
  std::string host_;
  uint16 port_;
  std::string path_;
};

// Make Valgrind happy. Without this function, a generic one will print the
// raw bytes in FilterTestParams, which due to some likely padding will access
// uninitialized memory.
void PrintTo(const FilterTestParams& params, std::ostream* os) {
  *os << params.filter();
}

class URLBlacklistFilterToComponentsTest
    : public testing::TestWithParam<FilterTestParams> {
 public:
  URLBlacklistFilterToComponentsTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(URLBlacklistFilterToComponentsTest);
};

TEST_P(URLBlacklistFilterToComponentsTest, FilterToComponents) {
  std::string scheme;
  std::string host;
  uint16 port;
  std::string path;

  URLBlacklist::FilterToComponents(GetParam().filter(), &scheme, &host, &port,
                                   &path);
  EXPECT_EQ(GetParam().scheme(), scheme);
  EXPECT_EQ(GetParam().host(), host);
  EXPECT_EQ(GetParam().port(), port);
  EXPECT_EQ(GetParam().path(), path);
}

TEST_F(URLBlacklistManagerTest, SingleUpdateForTwoPrefChanges) {
  ExpectUpdate();

  ListValue* blacklist = new ListValue;
  blacklist->Append(new StringValue("*.google.com"));
  ListValue* whitelist = new ListValue;
  whitelist->Append(new StringValue("mail.google.com"));
  pref_service_.SetManagedPref(prefs::kUrlBlacklist, blacklist);
  pref_service_.SetManagedPref(prefs::kUrlBlacklist, whitelist);
  loop_.RunAllPending();

  Mock::VerifyAndClearExpectations(blacklist_manager_.get());
}

TEST_F(URLBlacklistManagerTest, ShutdownWithPendingTask0) {
  // Post an update task to the UI thread.
  blacklist_manager_->ScheduleUpdate();
  // Shutdown comes before the task is executed.
  blacklist_manager_->ShutdownOnUIThread();
  blacklist_manager_.reset();
  // Run the task after shutdown and deletion.
  loop_.RunAllPending();
}

TEST_F(URLBlacklistManagerTest, ShutdownWithPendingTask1) {
  EXPECT_CALL(*blacklist_manager_, Update()).Times(0);
  // Post an update task.
  blacklist_manager_->ScheduleUpdate();
  // Shutdown comes before the task is executed.
  blacklist_manager_->ShutdownOnUIThread();
  // Run the task after shutdown, but before deletion.
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(blacklist_manager_.get());
  blacklist_manager_.reset();
  loop_.RunAllPending();
}

TEST_F(URLBlacklistManagerTest, ShutdownWithPendingTask2) {
  EXPECT_CALL(*blacklist_manager_, SetBlacklist(_)).Times(0);
  // Update posts a BuildBlacklistTask to the FILE thread.
  blacklist_manager_->UpdateNotMocked();
  // Shutdown comes before the task is executed.
  blacklist_manager_->ShutdownOnUIThread();
  // Run the task after shutdown, but before deletion.
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(blacklist_manager_.get());
  blacklist_manager_.reset();
  loop_.RunAllPending();
}

TEST_F(URLBlacklistManagerTest, ShutdownWithPendingTask3) {
  EXPECT_CALL(*blacklist_manager_, SetBlacklist(_)).Times(0);

  // This posts a task to the FILE thread.
  blacklist_manager_->UpdateOnIO();
  // But shutdown happens before it is done.
  blacklist_manager_->ShutdownOnUIThread();
  blacklist_manager_.reset();
  loop_.RunAllPending();

  Mock::VerifyAndClearExpectations(blacklist_manager_.get());
}

TEST_F(URLBlacklistManagerTest, ShutdownWithPendingTask4) {
  EXPECT_CALL(*blacklist_manager_, SetBlacklist(_)).Times(0);

  // This posts a task to the FILE thread.
  blacklist_manager_->UpdateOnIO();
  // But shutdown happens before it is done.
  blacklist_manager_->ShutdownOnUIThread();
  // This time, shutdown on UI is done but the object is still alive.
  loop_.RunAllPending();
  blacklist_manager_.reset();
  loop_.RunAllPending();

  Mock::VerifyAndClearExpectations(blacklist_manager_.get());
}

TEST_F(URLBlacklistManagerTest, SchemeToFlag) {
  URLBlacklist::SchemeFlag flag;
  EXPECT_TRUE(URLBlacklist::SchemeToFlag("http", &flag));
  EXPECT_EQ(URLBlacklist::SCHEME_HTTP, flag);
  EXPECT_TRUE(URLBlacklist::SchemeToFlag("https", &flag));
  EXPECT_EQ(URLBlacklist::SCHEME_HTTPS, flag);
  EXPECT_TRUE(URLBlacklist::SchemeToFlag("ftp", &flag));
  EXPECT_EQ(URLBlacklist::SCHEME_FTP, flag);
  EXPECT_TRUE(URLBlacklist::SchemeToFlag("", &flag));
  EXPECT_EQ(URLBlacklist::SCHEME_ALL, flag);
  EXPECT_FALSE(URLBlacklist::SchemeToFlag("wtf", &flag));
}

INSTANTIATE_TEST_CASE_P(
    URLBlacklistFilterToComponentsTestInstance,
    URLBlacklistFilterToComponentsTest,
    testing::Values(
        FilterTestParams("google.com",
                         "", "google.com", 0u, ""),
        FilterTestParams("http://google.com",
                         "http", "google.com", 0u, ""),
        FilterTestParams("google.com/",
                         "", "google.com", 0u, "/"),
        FilterTestParams("http://google.com:8080/whatever",
                         "http", "google.com", 8080u, "/whatever"),
        FilterTestParams("http://user:pass@google.com:8080/whatever",
                         "http", "google.com", 8080u, "/whatever"),
        FilterTestParams("123.123.123.123",
                         "", "123.123.123.123", 0u, ""),
        FilterTestParams("https://123.123.123.123",
                         "https", "123.123.123.123", 0u, ""),
        FilterTestParams("123.123.123.123/",
                         "", "123.123.123.123", 0u, "/"),
        FilterTestParams("http://123.123.123.123:123/whatever",
                         "http", "123.123.123.123", 123u, "/whatever"),
        FilterTestParams("*",
                         "", "", 0u, ""),
        FilterTestParams("ftp://*",
                         "ftp", "", 0u, ""),
        FilterTestParams("http://*/whatever",
                         "http", "", 0u, "/whatever")));

TEST_F(URLBlacklistManagerTest, Filtering) {
  URLBlacklist blacklist;

  // Block domain and all subdomains, for any filtered scheme.
  blacklist.Block("google.com");
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://google.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://google.com/whatever")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://google.com/")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("bogus://google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://mail.google.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://x.mail.google.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://x.mail.google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://x.y.google.com/a/b")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube.com/")));

  // Filter only http and ftp schemes.
  blacklist.Block("http://secure.com");
  blacklist.Block("ftp://secure.com");
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://secure.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://secure.com/whatever")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("ftp://secure.com/")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://secure.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.secure.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://www.secure.com")));

  // Filter only a certain path prefix.
  blacklist.Block("path.to/ruin");
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://path.to/ruin")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://path.to/ruin")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://path.to/ruins")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://path.to/ruin/signup")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.path.to/ruin")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://path.to/fortune")));

  // Filter only a certain path prefix and scheme.
  blacklist.Block("https://s.aaa.com/path");
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://s.aaa.com/path")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://s.aaa.com/path/bbb")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://s.aaa.com/path")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://aaa.com/path")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://x.aaa.com/path")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://s.aaa.com/bbb")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://s.aaa.com/")));

  // Test exceptions to path prefixes, and most specific matches.
  blacklist.Block("s.xxx.com/a");
  blacklist.Allow("s.xxx.com/a/b");
  blacklist.Block("https://s.xxx.com/a/b/c");
  blacklist.Allow("https://s.xxx.com/a/b/c/d");
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://s.xxx.com/a")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://s.xxx.com/a/x")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://s.xxx.com/a/x")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://s.xxx.com/a/b")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://s.xxx.com/a/b")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://s.xxx.com/a/b/x")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://s.xxx.com/a/b/c")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://s.xxx.com/a/b/c")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://s.xxx.com/a/b/c/x")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://s.xxx.com/a/b/c/d")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://s.xxx.com/a/b/c/d")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://s.xxx.com/a/b/c/d/x")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://s.xxx.com/a/b/c/d/x")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://xxx.com/a")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://xxx.com/a/b")));

  // Block an ip address.
  blacklist.Block("123.123.123.123");
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://123.123.123.123/")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://123.123.123.124/")));

  // Open an exception.
  blacklist.Allow("plus.google.com");
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.google.com/")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://plus.google.com/")));

  // Open an exception only when using https for mail.
  blacklist.Allow("https://mail.google.com");
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://mail.google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://www.google.com/")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://mail.google.com/")));

  // Match exactly "google.com", only for http. Subdomains without exceptions
  // are still blocked.
  blacklist.Allow("http://.google.com");
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.google.com/")));

  // A smaller path match in an exact host overrides a longer path for hosts
  // that also match subdomains.
  blacklist.Block("yyy.com/aaa");
  blacklist.Allow(".yyy.com/a");
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://yyy.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://yyy.com/aaa")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://yyy.com/aaa2")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://www.yyy.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.yyy.com/aaa")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.yyy.com/aaa2")));
}

TEST_F(URLBlacklistManagerTest, BlockAllWithExceptions) {
  URLBlacklist blacklist;

  blacklist.Block("*");
  blacklist.Allow(".www.google.com");
  blacklist.Allow("plus.google.com");
  blacklist.Allow("https://mail.google.com");
  blacklist.Allow("https://very.safe/path");
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://random.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://google.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://s.www.google.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://www.google.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://plus.google.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://s.plus.google.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://mail.google.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://mail.google.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://s.mail.google.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://very.safe/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://very.safe/path")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://very.safe/path")));
}

}  // namespace

}  // namespace policy
