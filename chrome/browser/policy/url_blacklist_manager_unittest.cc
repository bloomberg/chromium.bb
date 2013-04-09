// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/url_blacklist_manager.h"

#include <ostream>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/gaia_urls.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

using content::BrowserThread;

class TestingURLBlacklistManager : public URLBlacklistManager {
 public:
  explicit TestingURLBlacklistManager(PrefService* pref_service)
      : URLBlacklistManager(pref_service),
        update_called_(0),
        set_blacklist_called_(false) {
  }

  virtual ~TestingURLBlacklistManager() {
  }

  // Make this method public for testing.
  using URLBlacklistManager::ScheduleUpdate;

  // Makes a direct call to UpdateOnIO during tests.
  void UpdateOnIOForTesting() {
    scoped_ptr<base::ListValue> block(new base::ListValue);
    block->Append(new base::StringValue("example.com"));
    scoped_ptr<base::ListValue> allow(new base::ListValue);
    URLBlacklistManager::UpdateOnIO(block.Pass(), allow.Pass());
  }

  // URLBlacklistManager overrides:
  virtual void SetBlacklist(scoped_ptr<URLBlacklist> blacklist) OVERRIDE {
    set_blacklist_called_ = true;
    URLBlacklistManager::SetBlacklist(blacklist.Pass());
  }

  virtual void Update() OVERRIDE {
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
  URLBlacklistManagerTest()
      : loop_(MessageLoop::TYPE_IO),
        ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_),
        io_thread_(BrowserThread::IO, &loop_) {
  }

  virtual void SetUp() OVERRIDE {
    pref_service_.registry()->RegisterListPref(prefs::kUrlBlacklist);
    pref_service_.registry()->RegisterListPref(prefs::kUrlWhitelist);
    blacklist_manager_.reset(
        new TestingURLBlacklistManager(&pref_service_));
    loop_.RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    if (blacklist_manager_.get())
      blacklist_manager_->ShutdownOnUIThread();
    loop_.RunUntilIdle();
    // Delete |blacklist_manager_| while |io_thread_| is mapping IO to
    // |loop_|.
    blacklist_manager_.reset();
  }

  MessageLoop loop_;
  TestingPrefServiceSimple pref_service_;
  scoped_ptr<TestingURLBlacklistManager> blacklist_manager_;

 private:
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;

  DISALLOW_COPY_AND_ASSIGN(URLBlacklistManagerTest);
};

// Parameters for the FilterToComponents test.
struct FilterTestParams {
 public:
  FilterTestParams(const std::string& filter, const std::string& scheme,
                   const std::string& host, bool match_subdomains, uint16 port,
                   const std::string& path)
      : filter_(filter), scheme_(scheme), host_(host),
        match_subdomains_(match_subdomains), port_(port), path_(path) {}

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
  uint16 port() const { return port_; }
  const std::string& path() const { return path_; }

 private:
  std::string filter_;
  std::string scheme_;
  std::string host_;
  bool match_subdomains_;
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

}  // namespace

TEST_P(URLBlacklistFilterToComponentsTest, FilterToComponents) {
  std::string scheme;
  std::string host;
  bool match_subdomains = true;
  uint16 port = 42;
  std::string path;

  URLBlacklist::FilterToComponents(GetParam().filter(), &scheme, &host,
                                   &match_subdomains, &port, &path);
  EXPECT_EQ(GetParam().scheme(), scheme);
  EXPECT_EQ(GetParam().host(), host);
  EXPECT_EQ(GetParam().match_subdomains(), match_subdomains);
  EXPECT_EQ(GetParam().port(), port);
  EXPECT_EQ(GetParam().path(), path);
}

TEST_F(URLBlacklistManagerTest, SingleUpdateForTwoPrefChanges) {
  ListValue* blacklist = new ListValue;
  blacklist->Append(new StringValue("*.google.com"));
  ListValue* whitelist = new ListValue;
  whitelist->Append(new StringValue("mail.google.com"));
  pref_service_.SetManagedPref(prefs::kUrlBlacklist, blacklist);
  pref_service_.SetManagedPref(prefs::kUrlBlacklist, whitelist);
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

TEST_F(URLBlacklistManagerTest, HasStandardScheme) {
  EXPECT_TRUE(URLBlacklist::HasStandardScheme(GURL("http://example.com")));
  EXPECT_TRUE(URLBlacklist::HasStandardScheme(GURL("https://example.com")));
  EXPECT_TRUE(URLBlacklist::HasStandardScheme(GURL("ftp://example.com")));
  EXPECT_TRUE(URLBlacklist::HasStandardScheme(GURL("gopher://example.com")));
  EXPECT_TRUE(URLBlacklist::HasStandardScheme(GURL("ws://example.com")));
  EXPECT_TRUE(URLBlacklist::HasStandardScheme(GURL("wss://example.com")));
  EXPECT_FALSE(URLBlacklist::HasStandardScheme(GURL("wtf://example.com")));
}

INSTANTIATE_TEST_CASE_P(
    URLBlacklistFilterToComponentsTestInstance,
    URLBlacklistFilterToComponentsTest,
    testing::Values(
        FilterTestParams("google.com",
                         "", ".google.com", true, 0u, ""),
        FilterTestParams(".google.com",
                         "", "google.com", false, 0u, ""),
        FilterTestParams("http://google.com",
                         "http", ".google.com", true, 0u, ""),
        FilterTestParams("google.com/",
                         "", ".google.com", true, 0u, "/"),
        FilterTestParams("http://google.com:8080/whatever",
                         "http", ".google.com", true, 8080u, "/whatever"),
        FilterTestParams("http://user:pass@google.com:8080/whatever",
                         "http", ".google.com", true, 8080u, "/whatever"),
        FilterTestParams("123.123.123.123",
                         "", "123.123.123.123", false, 0u, ""),
        FilterTestParams("https://123.123.123.123",
                         "https", "123.123.123.123", false, 0u, ""),
        FilterTestParams("123.123.123.123/",
                         "", "123.123.123.123", false, 0u, "/"),
        FilterTestParams("http://123.123.123.123:123/whatever",
                         "http", "123.123.123.123", false, 123u, "/whatever"),
        FilterTestParams("*",
                         "", "", true, 0u, ""),
        FilterTestParams("ftp://*",
                         "ftp", "", true, 0u, ""),
        FilterTestParams("http://*/whatever",
                         "http", "", true, 0u, "/whatever")));

TEST_F(URLBlacklistManagerTest, Filtering) {
  URLBlacklist blacklist;

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

TEST_F(URLBlacklistManagerTest, BlockAllWithExceptions) {
  URLBlacklist blacklist;

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
  scoped_ptr<URLBlacklist> blacklist(new URLBlacklist());
  scoped_ptr<base::ListValue> blocked(new base::ListValue);
  blocked->Append(new base::StringValue("google.com"));
  blacklist->Block(blocked.get());
  blacklist_manager_->SetBlacklist(blacklist.Pass());
  EXPECT_TRUE(blacklist_manager_->IsURLBlocked(GURL("http://google.com")));

  net::TestURLRequestContext context;
  net::URLRequest request(GURL("http://google.com"), NULL, &context);

  // Background requests aren't filtered.
  EXPECT_FALSE(blacklist_manager_->IsRequestBlocked(request));

  // Main frames are filtered.
  request.set_load_flags(net::LOAD_MAIN_FRAME);
  EXPECT_TRUE(blacklist_manager_->IsRequestBlocked(request));

  // Sync gets a free pass.
  GURL sync_url(
      GaiaUrls::GetInstance()->service_login_url() + "?service=chromiumsync");
  net::URLRequest sync_request(sync_url, NULL, &context);
  sync_request.set_load_flags(net::LOAD_MAIN_FRAME);
  EXPECT_FALSE(blacklist_manager_->IsRequestBlocked(sync_request));
}

}  // namespace policy
