// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_blacklist_manager.h"

#include <stdint.h>
#include <ostream>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/url_formatter/url_fixer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

// Helper to get the disambiguated SegmentURL() function.
URLBlacklist::SegmentURLCallback GetSegmentURLCallback() {
  return url_formatter::SegmentURL;
}

bool OverrideBlacklistForURL(const GURL& url, bool* block, int* reason) {
  return false;
}

class TestingURLBlacklistManager : public URLBlacklistManager {
 public:
  explicit TestingURLBlacklistManager(PrefService* pref_service)
      : URLBlacklistManager(pref_service,
                            base::ThreadTaskRunnerHandle::Get(),
                            base::ThreadTaskRunnerHandle::Get(),
                            GetSegmentURLCallback(),
                            base::Bind(OverrideBlacklistForURL)),
        update_called_(0),
        set_blacklist_called_(false) {}

  ~TestingURLBlacklistManager() override {}

  // Make this method public for testing.
  using URLBlacklistManager::ScheduleUpdate;

  // Makes a direct call to UpdateOnIO during tests.
  void UpdateOnIOForTesting() {
    scoped_ptr<base::ListValue> block(new base::ListValue);
    block->Append(new base::StringValue("example.com"));
    scoped_ptr<base::ListValue> allow(new base::ListValue);
    URLBlacklistManager::UpdateOnIO(std::move(block), std::move(allow));
  }

  // URLBlacklistManager overrides:
  void SetBlacklist(scoped_ptr<URLBlacklist> blacklist) override {
    set_blacklist_called_ = true;
    URLBlacklistManager::SetBlacklist(std::move(blacklist));
  }

  void Update() override {
    update_called_++;
    URLBlacklistManager::Update();
  }

  int update_called() const { return update_called_; }
  bool set_blacklist_called() const { return set_blacklist_called_; }

 private:
  int update_called_;
  bool set_blacklist_called_;

  DISALLOW_COPY_AND_ASSIGN(TestingURLBlacklistManager);
};

class URLBlacklistManagerTest : public testing::Test {
 protected:
  URLBlacklistManagerTest() {}

  void SetUp() override {
    pref_service_.registry()->RegisterListPref(policy_prefs::kUrlBlacklist);
    pref_service_.registry()->RegisterListPref(policy_prefs::kUrlWhitelist);
    blacklist_manager_.reset(new TestingURLBlacklistManager(&pref_service_));
    loop_.RunUntilIdle();
  }

  void TearDown() override {
    if (blacklist_manager_.get())
      blacklist_manager_->ShutdownOnUIThread();
    loop_.RunUntilIdle();
    // Delete |blacklist_manager_| while |io_thread_| is mapping IO to
    // |loop_|.
    blacklist_manager_.reset();
  }

  base::MessageLoopForIO loop_;
  TestingPrefServiceSimple pref_service_;
  scoped_ptr<TestingURLBlacklistManager> blacklist_manager_;
};

// Parameters for the FilterToComponents test.
struct FilterTestParams {
 public:
  FilterTestParams(const std::string& filter,
                   const std::string& scheme,
                   const std::string& host,
                   bool match_subdomains,
                   uint16_t port,
                   const std::string& path)
      : filter_(filter),
        scheme_(scheme),
        host_(host),
        match_subdomains_(match_subdomains),
        port_(port),
        path_(path) {}

  FilterTestParams(const FilterTestParams& params)
      : filter_(params.filter_), scheme_(params.scheme_), host_(params.host_),
        match_subdomains_(params.match_subdomains_), port_(params.port_),
        path_(params.path_) {}

  const FilterTestParams& operator=(const FilterTestParams& params) {
    filter_ = params.filter_;
    scheme_ = params.scheme_;
    host_ = params.host_;
    match_subdomains_ = params.match_subdomains_;
    port_ = params.port_;
    path_ = params.path_;
    return *this;
  }

  const std::string& filter() const { return filter_; }
  const std::string& scheme() const { return scheme_; }
  const std::string& host() const { return host_; }
  bool match_subdomains() const { return match_subdomains_; }
  uint16_t port() const { return port_; }
  const std::string& path() const { return path_; }

 private:
  std::string filter_;
  std::string scheme_;
  std::string host_;
  bool match_subdomains_;
  uint16_t port_;
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

}  // namespace

TEST_P(URLBlacklistFilterToComponentsTest, FilterToComponents) {
  std::string scheme;
  std::string host;
  bool match_subdomains = true;
  uint16_t port = 42;
  std::string path;

  URLBlacklist::FilterToComponents(GetSegmentURLCallback(),
                                   GetParam().filter(),
                                   &scheme,
                                   &host,
                                   &match_subdomains,
                                   &port,
                                   &path,
                                   NULL);
  EXPECT_EQ(GetParam().scheme(), scheme);
  EXPECT_EQ(GetParam().host(), host);
  EXPECT_EQ(GetParam().match_subdomains(), match_subdomains);
  EXPECT_EQ(GetParam().port(), port);
  EXPECT_EQ(GetParam().path(), path);
}

TEST_F(URLBlacklistManagerTest, SingleUpdateForTwoPrefChanges) {
  base::ListValue* blacklist = new base::ListValue;
  blacklist->Append(new base::StringValue("*.google.com"));
  base::ListValue* whitelist = new base::ListValue;
  whitelist->Append(new base::StringValue("mail.google.com"));
  pref_service_.SetManagedPref(policy_prefs::kUrlBlacklist, blacklist);
  pref_service_.SetManagedPref(policy_prefs::kUrlBlacklist, whitelist);
  loop_.RunUntilIdle();

  EXPECT_EQ(1, blacklist_manager_->update_called());
}

TEST_F(URLBlacklistManagerTest, ShutdownWithPendingTask0) {
  // Post an update task to the UI thread.
  blacklist_manager_->ScheduleUpdate();
  // Shutdown comes before the task is executed.
  blacklist_manager_->ShutdownOnUIThread();
  blacklist_manager_.reset();
  // Run the task after shutdown and deletion.
  loop_.RunUntilIdle();
}

TEST_F(URLBlacklistManagerTest, ShutdownWithPendingTask1) {
  // Post an update task.
  blacklist_manager_->ScheduleUpdate();
  // Shutdown comes before the task is executed.
  blacklist_manager_->ShutdownOnUIThread();
  // Run the task after shutdown, but before deletion.
  loop_.RunUntilIdle();

  EXPECT_EQ(0, blacklist_manager_->update_called());
  blacklist_manager_.reset();
  loop_.RunUntilIdle();
}

TEST_F(URLBlacklistManagerTest, ShutdownWithPendingTask2) {
  // This posts a task to the FILE thread.
  blacklist_manager_->UpdateOnIOForTesting();
  // But shutdown happens before it is done.
  blacklist_manager_->ShutdownOnUIThread();

  EXPECT_FALSE(blacklist_manager_->set_blacklist_called());
  blacklist_manager_.reset();
  loop_.RunUntilIdle();
}

INSTANTIATE_TEST_CASE_P(
    URLBlacklistFilterToComponentsTestInstance,
    URLBlacklistFilterToComponentsTest,
    testing::Values(
        FilterTestParams("google.com",
                         std::string(),
                         ".google.com",
                         true,
                         0u,
                         std::string()),
        FilterTestParams(".google.com",
                         std::string(),
                         "google.com",
                         false,
                         0u,
                         std::string()),
        FilterTestParams("http://google.com",
                         "http",
                         ".google.com",
                         true,
                         0u,
                         std::string()),
        FilterTestParams("google.com/",
                         std::string(),
                         ".google.com",
                         true,
                         0u,
                         "/"),
        FilterTestParams("http://google.com:8080/whatever",
                         "http",
                         ".google.com",
                         true,
                         8080u,
                         "/whatever"),
        FilterTestParams("http://user:pass@google.com:8080/whatever",
                         "http",
                         ".google.com",
                         true,
                         8080u,
                         "/whatever"),
        FilterTestParams("123.123.123.123",
                         std::string(),
                         "123.123.123.123",
                         false,
                         0u,
                         std::string()),
        FilterTestParams("https://123.123.123.123",
                         "https",
                         "123.123.123.123",
                         false,
                         0u,
                         std::string()),
        FilterTestParams("123.123.123.123/",
                         std::string(),
                         "123.123.123.123",
                         false,
                         0u,
                         "/"),
        FilterTestParams("http://123.123.123.123:123/whatever",
                         "http",
                         "123.123.123.123",
                         false,
                         123u,
                         "/whatever"),
        FilterTestParams("*",
                         std::string(),
                         std::string(),
                         true,
                         0u,
                         std::string()),
        FilterTestParams("ftp://*",
                         "ftp",
                         std::string(),
                         true,
                         0u,
                         std::string()),
        FilterTestParams("http://*/whatever",
                         "http",
                         std::string(),
                         true,
                         0u,
                         "/whatever")));

TEST_F(URLBlacklistManagerTest, Filtering) {
  URLBlacklist blacklist(GetSegmentURLCallback());

  // Block domain and all subdomains, for any filtered scheme.
  scoped_ptr<base::ListValue> blocked(new base::ListValue);
  blocked->Append(new base::StringValue("google.com"));
  blacklist.Block(blocked.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://google.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://google.com/whatever")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://google.com/")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("bogus://google.com/")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://notgoogle.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://mail.google.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://x.mail.google.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://x.mail.google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://x.y.google.com/a/b")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube.com/")));

  // Filter only http, ftp and ws schemes.
  blocked.reset(new base::ListValue);
  blocked->Append(new base::StringValue("http://secure.com"));
  blocked->Append(new base::StringValue("ftp://secure.com"));
  blocked->Append(new base::StringValue("ws://secure.com"));
  blacklist.Block(blocked.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://secure.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://secure.com/whatever")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("ftp://secure.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("ws://secure.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://secure.com/")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("wss://secure.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.secure.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://www.secure.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("wss://www.secure.com")));

  // Filter only a certain path prefix.
  blocked.reset(new base::ListValue);
  blocked->Append(new base::StringValue("path.to/ruin"));
  blacklist.Block(blocked.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://path.to/ruin")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://path.to/ruin")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://path.to/ruins")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://path.to/ruin/signup")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.path.to/ruin")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://path.to/fortune")));

  // Filter only a certain path prefix and scheme.
  blocked.reset(new base::ListValue);
  blocked->Append(new base::StringValue("https://s.aaa.com/path"));
  blacklist.Block(blocked.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://s.aaa.com/path")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://s.aaa.com/path/bbb")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://s.aaa.com/path")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://aaa.com/path")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://x.aaa.com/path")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://s.aaa.com/bbb")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://s.aaa.com/")));

  // Filter only ws and wss schemes.
  blocked.reset(new base::ListValue);
  blocked->Append(new base::StringValue("ws://ws.aaa.com"));
  blocked->Append(new base::StringValue("wss://ws.aaa.com"));
  blacklist.Block(blocked.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("ws://ws.aaa.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("wss://ws.aaa.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://ws.aaa.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://ws.aaa.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("ftp://ws.aaa.com")));

  // Test exceptions to path prefixes, and most specific matches.
  blocked.reset(new base::ListValue);
  scoped_ptr<base::ListValue> allowed(new base::ListValue);
  blocked->Append(new base::StringValue("s.xxx.com/a"));
  allowed->Append(new base::StringValue("s.xxx.com/a/b"));
  blocked->Append(new base::StringValue("https://s.xxx.com/a/b/c"));
  allowed->Append(new base::StringValue("https://s.xxx.com/a/b/c/d"));
  blacklist.Block(blocked.get());
  blacklist.Allow(allowed.get());
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
  blocked.reset(new base::ListValue);
  blocked->Append(new base::StringValue("123.123.123.123"));
  blacklist.Block(blocked.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://123.123.123.123/")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://123.123.123.124/")));

  // Open an exception.
  allowed.reset(new base::ListValue);
  allowed->Append(new base::StringValue("plus.google.com"));
  blacklist.Allow(allowed.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.google.com/")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://plus.google.com/")));

  // Open an exception only when using https for mail.
  allowed.reset(new base::ListValue);
  allowed->Append(new base::StringValue("https://mail.google.com"));
  blacklist.Allow(allowed.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://mail.google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://www.google.com/")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("https://mail.google.com/")));

  // Match exactly "google.com", only for http. Subdomains without exceptions
  // are still blocked.
  allowed.reset(new base::ListValue);
  allowed->Append(new base::StringValue("http://.google.com"));
  blacklist.Allow(allowed.get());
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("https://google.com/")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.google.com/")));

  // A smaller path match in an exact host overrides a longer path for hosts
  // that also match subdomains.
  blocked.reset(new base::ListValue);
  blocked->Append(new base::StringValue("yyy.com/aaa"));
  blacklist.Block(blocked.get());
  allowed.reset(new base::ListValue);
  allowed->Append(new base::StringValue(".yyy.com/a"));
  blacklist.Allow(allowed.get());
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://yyy.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://yyy.com/aaa")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://yyy.com/aaa2")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://www.yyy.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.yyy.com/aaa")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.yyy.com/aaa2")));

  // If the exact entry is both allowed and blocked, allowing takes precedence.
  blocked.reset(new base::ListValue);
  blocked->Append(new base::StringValue("example.com"));
  blacklist.Block(blocked.get());
  allowed.reset(new base::ListValue);
  allowed->Append(new base::StringValue("example.com"));
  blacklist.Allow(allowed.get());
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://example.com")));
}

TEST_F(URLBlacklistManagerTest, QueryParameters) {
  URLBlacklist blacklist(GetSegmentURLCallback());
  scoped_ptr<base::ListValue> blocked(new base::ListValue);
  scoped_ptr<base::ListValue> allowed(new base::ListValue);

  // Block domain and all subdomains, for any filtered scheme.
  blocked->AppendString("youtube.com");
  allowed->AppendString("youtube.com/watch?v=XYZ");
  blacklist.Block(blocked.get());
  blacklist.Allow(allowed.get());

  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube.com/watch?v=123")));
  EXPECT_TRUE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?v=123&v=XYZ")));
  EXPECT_TRUE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?v=XYZ&v=123")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube.com/watch?v=XYZ")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?v=XYZ&foo=bar")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?foo=bar&v=XYZ")));

  allowed.reset(new base::ListValue);
  allowed->AppendString("youtube.com/watch?av=XYZ&ag=123");
  blacklist.Allow(allowed.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube.com/watch?av=123")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube.com/watch?av=XYZ")));
  EXPECT_TRUE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?av=123&ag=XYZ")));
  EXPECT_TRUE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?ag=XYZ&av=123")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?av=XYZ&ag=123")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?ag=123&av=XYZ")));
  EXPECT_TRUE(blacklist.IsURLBlocked(
      GURL("http://youtube.com/watch?av=XYZ&ag=123&av=123")));
  EXPECT_TRUE(blacklist.IsURLBlocked(
      GURL("http://youtube.com/watch?av=XYZ&ag=123&ag=1234")));

  allowed.reset(new base::ListValue);
  allowed->AppendString("youtube.com/watch?foo=bar*&vid=2*");
  blacklist.Allow(allowed.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube.com/watch?vid=2")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube.com/watch?foo=bar")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?vid=2&foo=bar")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?vid=2&foo=bar1")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube.com/watch?vid=234&foo=bar")));
  EXPECT_FALSE(blacklist.IsURLBlocked(
      GURL("http://youtube.com/watch?vid=234&foo=bar23")));

  blocked.reset(new base::ListValue);
  blocked->AppendString("youtube1.com/disallow?v=44678");
  blacklist.Block(blocked.get());
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube1.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube1.com?v=123")));
  // Path does not match
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube1.com?v=44678")));
  EXPECT_TRUE(
      blacklist.IsURLBlocked(GURL("http://youtube1.com/disallow?v=44678")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube1.com/disallow?v=4467")));
  EXPECT_FALSE(blacklist.IsURLBlocked(
      GURL("http://youtube1.com/disallow?v=4467&v=123")));
  EXPECT_TRUE(blacklist.IsURLBlocked(
      GURL("http://youtube1.com/disallow?v=4467&v=123&v=44678")));

  blocked.reset(new base::ListValue);
  blocked->AppendString("youtube1.com/disallow?g=*");
  blacklist.Block(blocked.get());
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube1.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube1.com?ag=123")));
  EXPECT_TRUE(
      blacklist.IsURLBlocked(GURL("http://youtube1.com/disallow?g=123")));
  EXPECT_TRUE(
      blacklist.IsURLBlocked(GURL("http://youtube1.com/disallow?ag=13&g=123")));

  blocked.reset(new base::ListValue);
  blocked->AppendString("youtube2.com/disallow?a*");
  blacklist.Block(blocked.get());
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube2.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(
      GURL("http://youtube2.com/disallow?b=123&a21=467")));
  EXPECT_TRUE(
      blacklist.IsURLBlocked(GURL("http://youtube2.com/disallow?abba=true")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube2.com/disallow?baba=true")));

  allowed.reset(new base::ListValue);
  blocked.reset(new base::ListValue);
  blocked->AppendString("youtube3.com");
  allowed->AppendString("youtube3.com/watch?fo*");
  blacklist.Block(blocked.get());
  blacklist.Allow(allowed.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube3.com")));
  EXPECT_TRUE(
      blacklist.IsURLBlocked(GURL("http://youtube3.com/watch?b=123&a21=467")));
  EXPECT_FALSE(blacklist.IsURLBlocked(
      GURL("http://youtube3.com/watch?b=123&a21=467&foo1")));
  EXPECT_FALSE(blacklist.IsURLBlocked(
      GURL("http://youtube3.com/watch?b=123&a21=467&foo=bar")));
  EXPECT_FALSE(blacklist.IsURLBlocked(
      GURL("http://youtube3.com/watch?b=123&a21=467&fo=ba")));
  EXPECT_FALSE(
      blacklist.IsURLBlocked(GURL("http://youtube3.com/watch?foriegn=true")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube3.com/watch?fold")));

  allowed.reset(new base::ListValue);
  blocked.reset(new base::ListValue);
  blocked->AppendString("youtube4.com");
  allowed->AppendString("youtube4.com?*");
  blacklist.Block(blocked.get());
  blacklist.Allow(allowed.get());
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube4.com")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube4.com/?hello")));
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube4.com/?foo")));

  allowed.reset(new base::ListValue);
  blocked.reset(new base::ListValue);
  blocked->AppendString("youtube5.com?foo=bar");
  allowed->AppendString("youtube5.com?foo1=bar1&foo2=bar2&");
  blacklist.Block(blocked.get());
  blacklist.Allow(allowed.get());
  EXPECT_FALSE(blacklist.IsURLBlocked(GURL("http://youtube5.com")));
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://youtube5.com/?foo=bar&a=b")));
  // More specific filter is given precedence.
  EXPECT_FALSE(blacklist.IsURLBlocked(
      GURL("http://youtube5.com/?a=b&foo=bar&foo1=bar1&foo2=bar2")));
}

TEST_F(URLBlacklistManagerTest, BlockAllWithExceptions) {
  URLBlacklist blacklist(GetSegmentURLCallback());

  scoped_ptr<base::ListValue> blocked(new base::ListValue);
  scoped_ptr<base::ListValue> allowed(new base::ListValue);
  blocked->Append(new base::StringValue("*"));
  allowed->Append(new base::StringValue(".www.google.com"));
  allowed->Append(new base::StringValue("plus.google.com"));
  allowed->Append(new base::StringValue("https://mail.google.com"));
  allowed->Append(new base::StringValue("https://very.safe/path"));
  blacklist.Block(blocked.get());
  blacklist.Allow(allowed.get());
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

TEST_F(URLBlacklistManagerTest, DontBlockResources) {
  scoped_ptr<URLBlacklist> blacklist(new URLBlacklist(GetSegmentURLCallback()));
  scoped_ptr<base::ListValue> blocked(new base::ListValue);
  blocked->Append(new base::StringValue("google.com"));
  blacklist->Block(blocked.get());
  blacklist_manager_->SetBlacklist(std::move(blacklist));
  EXPECT_TRUE(blacklist_manager_->IsURLBlocked(GURL("http://google.com")));

  int reason = net::ERR_UNEXPECTED;
  EXPECT_TRUE(blacklist_manager_->ShouldBlockRequestForFrame(
      GURL("http://google.com"), &reason));
  EXPECT_EQ(net::ERR_BLOCKED_BY_ADMINISTRATOR, reason);
}

TEST_F(URLBlacklistManagerTest, DefaultBlacklistExceptions) {
  URLBlacklist blacklist(GetSegmentURLCallback());
  scoped_ptr<base::ListValue> blocked(new base::ListValue);

  // Blacklist everything:
  blocked->Append(new base::StringValue("*"));
  blacklist.Block(blocked.get());

  // Internal NTP and extension URLs are not blocked by the "*":
  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.google.com")));
  EXPECT_FALSE((blacklist.IsURLBlocked(GURL("chrome-extension://xyz"))));
  EXPECT_FALSE((blacklist.IsURLBlocked(GURL("chrome-search://local-ntp"))));
  EXPECT_FALSE((blacklist.IsURLBlocked(GURL("chrome-native://ntp"))));

  // Unless they are explicitly blacklisted:
  blocked->Append(new base::StringValue("chrome-extension://*"));
  scoped_ptr<base::ListValue> allowed(new base::ListValue);
  allowed->Append(new base::StringValue("chrome-extension://abc"));
  blacklist.Block(blocked.get());
  blacklist.Allow(allowed.get());

  EXPECT_TRUE(blacklist.IsURLBlocked(GURL("http://www.google.com")));
  EXPECT_TRUE((blacklist.IsURLBlocked(GURL("chrome-extension://xyz"))));
  EXPECT_FALSE((blacklist.IsURLBlocked(GURL("chrome-extension://abc"))));
  EXPECT_FALSE((blacklist.IsURLBlocked(GURL("chrome-search://local-ntp"))));
  EXPECT_FALSE((blacklist.IsURLBlocked(GURL("chrome-native://ntp"))));
}

}  // namespace policy
