// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/managed_mode_url_filter.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FailClosureHelper : public base::RefCountedThreadSafe<FailClosureHelper> {
 public:
  explicit FailClosureHelper(const base::Closure& cb) : closure_runner_(cb) {}

  void Fail() {
    FAIL();
  }

 private:
  friend class base::RefCountedThreadSafe<FailClosureHelper>;

  virtual ~FailClosureHelper() {}

  base::ScopedClosureRunner closure_runner_;
};

// Returns a closure that FAILs when it is called. As soon as the closure is
// destroyed (because the last reference to it is dropped), |continuation| is
// called.
base::Closure FailClosure(const base::Closure& continuation) {
  scoped_refptr<FailClosureHelper> helper = new FailClosureHelper(continuation);
  return base::Bind(&FailClosureHelper::Fail, helper);
}

}  // namespace

class ManagedModeURLFilterTest : public ::testing::Test {
 public:
  ManagedModeURLFilterTest() {}
  virtual ~ManagedModeURLFilterTest() {}

  virtual void SetUp() OVERRIDE {
    filter_.reset(new ManagedModeURLFilter);
    filter_->SetActive(true);
  }

 protected:
  MessageLoop message_loop_;
  base::RunLoop run_loop_;
  scoped_ptr<ManagedModeURLFilter> filter_;
};

TEST_F(ManagedModeURLFilterTest, Basic) {
  std::vector<std::string> list;
  // Allow domain and all subdomains, for any filtered scheme.
  list.push_back("google.com");
  filter_->SetWhitelist(list, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("http://google.com")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("http://google.com/")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("http://google.com/whatever")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("https://google.com/")));
  EXPECT_FALSE(filter_->IsURLWhitelisted(GURL("http://notgoogle.com/")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("http://mail.google.com")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("http://x.mail.google.com")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("https://x.mail.google.com/")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("http://x.y.google.com/a/b")));
  EXPECT_FALSE(filter_->IsURLWhitelisted(GURL("http://youtube.com/")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("bogus://youtube.com/")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("chrome://youtube.com/")));
}

TEST_F(ManagedModeURLFilterTest, Inactive) {
  filter_->SetActive(false);

  std::vector<std::string> list;
  list.push_back("google.com");
  filter_->SetWhitelist(list, run_loop_.QuitClosure());
  run_loop_.Run();

  // If the filter is inactive, every URL should be whitelisted.
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("http://google.com")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("https://www.example.com")));
}

TEST_F(ManagedModeURLFilterTest, Shutdown) {
  std::vector<std::string> list;
  list.push_back("google.com");
  filter_->SetWhitelist(list, FailClosure(run_loop_.QuitClosure()));
  // Destroy the filter before we set the URLMatcher.
  filter_.reset();
  run_loop_.Run();
}

TEST_F(ManagedModeURLFilterTest, Scheme) {
  std::vector<std::string> list;
  // Filter only http, ftp and ws schemes.
  list.push_back("http://secure.com");
  list.push_back("ftp://secure.com");
  list.push_back("ws://secure.com");
  filter_->SetWhitelist(list, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("http://secure.com")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("http://secure.com/whatever")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("ftp://secure.com/")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("ws://secure.com")));
  EXPECT_FALSE(filter_->IsURLWhitelisted(GURL("https://secure.com/")));
  EXPECT_FALSE(filter_->IsURLWhitelisted(GURL("wss://secure.com")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("http://www.secure.com")));
  EXPECT_FALSE(filter_->IsURLWhitelisted(GURL("https://www.secure.com")));
  EXPECT_FALSE(filter_->IsURLWhitelisted(GURL("wss://www.secure.com")));
}

TEST_F(ManagedModeURLFilterTest, Path) {
  std::vector<std::string> list;
  // Filter only a certain path prefix.
  list.push_back("path.to/ruin");
  filter_->SetWhitelist(list, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("http://path.to/ruin")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("https://path.to/ruin")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("http://path.to/ruins")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("http://path.to/ruin/signup")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("http://www.path.to/ruin")));
  EXPECT_FALSE(filter_->IsURLWhitelisted(GURL("http://path.to/fortune")));
}

TEST_F(ManagedModeURLFilterTest, PathAndScheme) {
  std::vector<std::string> list;
  // Filter only a certain path prefix and scheme.
  list.push_back("https://s.aaa.com/path");
  filter_->SetWhitelist(list, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("https://s.aaa.com/path")));
  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("https://s.aaa.com/path/bbb")));
  EXPECT_FALSE(filter_->IsURLWhitelisted(GURL("http://s.aaa.com/path")));
  EXPECT_FALSE(filter_->IsURLWhitelisted(GURL("https://aaa.com/path")));
  EXPECT_FALSE(filter_->IsURLWhitelisted(GURL("https://x.aaa.com/path")));
  EXPECT_FALSE(filter_->IsURLWhitelisted(GURL("https://s.aaa.com/bbb")));
  EXPECT_FALSE(filter_->IsURLWhitelisted(GURL("https://s.aaa.com/")));
}

TEST_F(ManagedModeURLFilterTest, Host) {
  std::vector<std::string> list;
  // Filter only a certain hostname, without subdomains.
  list.push_back(".www.example.com");
  filter_->SetWhitelist(list, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("http://www.example.com")));
  EXPECT_FALSE(filter_->IsURLWhitelisted(GURL("http://example.com")));
  EXPECT_FALSE(filter_->IsURLWhitelisted(GURL("http://subdomain.example.com")));
}

TEST_F(ManagedModeURLFilterTest, IPAddress) {
  std::vector<std::string> list;
  // Filter an ip address.
  list.push_back("123.123.123.123");
  filter_->SetWhitelist(list, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_TRUE(filter_->IsURLWhitelisted(GURL("http://123.123.123.123/")));
  EXPECT_FALSE(filter_->IsURLWhitelisted(GURL("http://123.123.123.124/")));
}
