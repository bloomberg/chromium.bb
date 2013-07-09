// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_resetter.h"

#include "base/bind.h"
#include "chrome/browser/profile_resetter/profile_resetter_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

const char kCookieDefinition[] = "A=1";
const char kCookieHostname[] = "http://host1";

using content::BrowserThread;

// RemoveCookieTester provides the user with the ability to set and get a
// cookie for given profile.
class RemoveCookieTester {
 public:
  explicit RemoveCookieTester(Profile* profile);
  ~RemoveCookieTester();

  std::string GetCookie(const std::string& host);
  void AddCookie(const std::string& host, const std::string& definition);

 private:
  void GetCookieOnIOThread(net::URLRequestContextGetter* context_getter,
                           const std::string& host);
  void SetCookieOnIOThread(net::URLRequestContextGetter* context_getter,
                           const std::string& host,
                           const std::string& definition);
  void GetCookieCallback(const std::string& cookies);
  void SetCookieCallback(bool result);

  void BlockUntilNotified();
  void Notify();

  std::string last_cookies_;
  bool waiting_callback_;
  Profile* profile_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(RemoveCookieTester);
};

RemoveCookieTester::RemoveCookieTester(Profile* profile)
    : waiting_callback_(false),
      profile_(profile) {
}

RemoveCookieTester::~RemoveCookieTester() {}

// Returns true, if the given cookie exists in the cookie store.
std::string RemoveCookieTester::GetCookie(const std::string& host) {
  last_cookies_.clear();
  DCHECK(!waiting_callback_);
  waiting_callback_ = true;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RemoveCookieTester::GetCookieOnIOThread,
                 base::Unretained(this),
                 base::Unretained(profile_->GetRequestContext()),
                 host));
  BlockUntilNotified();
  return last_cookies_;
}

void RemoveCookieTester::AddCookie(const std::string& host,
                                   const std::string& definition) {
  DCHECK(!waiting_callback_);
  waiting_callback_ = true;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RemoveCookieTester::SetCookieOnIOThread,
                 base::Unretained(this),
                 base::Unretained(profile_->GetRequestContext()),
                 host,
                 definition));
  BlockUntilNotified();
}

void RemoveCookieTester::GetCookieOnIOThread(
    net::URLRequestContextGetter* context_getter,
    const std::string& host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::CookieStore* cookie_store = context_getter->
      GetURLRequestContext()->cookie_store();
  cookie_store->GetCookiesWithOptionsAsync(
      GURL(host), net::CookieOptions(),
      base::Bind(&RemoveCookieTester::GetCookieCallback,
                 base::Unretained(this)));
}

void RemoveCookieTester::SetCookieOnIOThread(
    net::URLRequestContextGetter* context_getter,
    const std::string& host,
    const std::string& definition) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::CookieStore* cookie_store = context_getter->
      GetURLRequestContext()->cookie_store();
  cookie_store->SetCookieWithOptionsAsync(
      GURL(host), definition, net::CookieOptions(),
      base::Bind(&RemoveCookieTester::SetCookieCallback,
                 base::Unretained(this)));
}

void RemoveCookieTester::GetCookieCallback(const std::string& cookies) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&RemoveCookieTester::GetCookieCallback,
                   base::Unretained(this), cookies));
    return;
  }
  last_cookies_ = cookies;
  Notify();
}

void RemoveCookieTester::SetCookieCallback(bool result) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&RemoveCookieTester::SetCookieCallback,
                   base::Unretained(this), result));
    return;
  }
  ASSERT_TRUE(result);
  Notify();
}

void RemoveCookieTester::BlockUntilNotified() {
  DCHECK(!runner_.get());
  if (waiting_callback_) {
    runner_ = new content::MessageLoopRunner;
    runner_->Run();
    runner_ = NULL;
  }
}

void RemoveCookieTester::Notify() {
  DCHECK(waiting_callback_);
  waiting_callback_ = false;
  if (runner_.get())
    runner_->Quit();
}

class ProfileResetTest : public InProcessBrowserTest,
                         public ProfileResetterTestBase {
 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    resetter_.reset(new ProfileResetter(browser()->profile()));
  }
};


IN_PROC_BROWSER_TEST_F(ProfileResetTest, ResetCookiesAndSiteData) {
  RemoveCookieTester tester(browser()->profile());
  tester.AddCookie(kCookieHostname, kCookieDefinition);
  ASSERT_EQ(kCookieDefinition, tester.GetCookie(kCookieHostname));

  ResetAndWait(ProfileResetter::COOKIES_AND_SITE_DATA);

  EXPECT_EQ("", tester.GetCookie(kCookieHostname));
}

}  // namespace
