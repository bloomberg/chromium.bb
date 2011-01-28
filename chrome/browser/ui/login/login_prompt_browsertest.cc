// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <list>
#include <map>

#include "chrome/browser/browser_thread.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/auth.h"

namespace {

class LoginPromptBrowserTest : public InProcessBrowserTest {
 public:
  LoginPromptBrowserTest()
      : bad_password_(L"incorrect"), bad_username_(L"nouser") {
    set_show_window(true);

    auth_map_[L"foo"] = AuthInfo(L"testuser", L"foopassword");
    auth_map_[L"bar"] = AuthInfo(L"testuser", L"barpassword");
  }

 protected:
  void SetAuthFor(LoginHandler* handler);

  struct AuthInfo {
    std::wstring username_;
    std::wstring password_;

    AuthInfo() {}

    AuthInfo(const std::wstring username,
             const std::wstring password)
        : username_(username), password_(password) {}
  };

  std::map<std::wstring, AuthInfo> auth_map_;
  std::wstring bad_password_;
  std::wstring bad_username_;
};

void LoginPromptBrowserTest::SetAuthFor(LoginHandler* handler) {
  const net::AuthChallengeInfo* challenge = handler->auth_info();

  ASSERT_TRUE(challenge);
  std::map<std::wstring, AuthInfo>::iterator i =
      auth_map_.find(challenge->realm);
  EXPECT_TRUE(auth_map_.end() != i);
  if (i != auth_map_.end()) {
    const AuthInfo& info = i->second;
    handler->SetAuth(info.username_, info.password_);
  }
}

// Maintains a set of LoginHandlers that are currently active and
// keeps a count of the notifications that were observed.
class LoginPromptBrowserTestObserver : public NotificationObserver {
 public:
  LoginPromptBrowserTestObserver()
      : auth_needed_count_(0),
        auth_supplied_count_(0),
        auth_cancelled_count_(0) {}

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  void AddHandler(LoginHandler* handler);

  void RemoveHandler(LoginHandler* handler);

  void Register(const NotificationSource& source);

  std::list<LoginHandler*> handlers_;

  // The exact number of notifications we receive is depedent on the
  // number of requests that were dispatched and is subject to a
  // number of factors that we don't directly control here.  The
  // values below should only be used qualitatively.
  int auth_needed_count_;
  int auth_supplied_count_;
  int auth_cancelled_count_;

 private:
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(LoginPromptBrowserTestObserver);
};

void LoginPromptBrowserTestObserver::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if (type == NotificationType::AUTH_NEEDED) {
    LoginNotificationDetails* login_details =
        Details<LoginNotificationDetails>(details).ptr();
    AddHandler(login_details->handler());
    auth_needed_count_++;
  } else if (type == NotificationType::AUTH_SUPPLIED) {
    AuthSuppliedLoginNotificationDetails* login_details =
        Details<AuthSuppliedLoginNotificationDetails>(details).ptr();
    RemoveHandler(login_details->handler());
    auth_supplied_count_++;
  } else if (type == NotificationType::AUTH_CANCELLED) {
    LoginNotificationDetails* login_details =
        Details<LoginNotificationDetails>(details).ptr();
    RemoveHandler(login_details->handler());
    auth_cancelled_count_++;
  }
}

void LoginPromptBrowserTestObserver::AddHandler(LoginHandler* handler) {
  std::list<LoginHandler*>::iterator i = std::find(handlers_.begin(),
                                                   handlers_.end(),
                                                   handler);
  EXPECT_TRUE(i == handlers_.end());
  if (i == handlers_.end())
    handlers_.push_back(handler);
}

void LoginPromptBrowserTestObserver::RemoveHandler(LoginHandler* handler) {
  std::list<LoginHandler*>::iterator i = std::find(handlers_.begin(),
                                                   handlers_.end(),
                                                   handler);
  EXPECT_TRUE(i != handlers_.end());
  if (i != handlers_.end())
    handlers_.erase(i);
}

void LoginPromptBrowserTestObserver::Register(
    const NotificationSource& source) {
  registrar_.Add(this, NotificationType::AUTH_NEEDED, source);
  registrar_.Add(this, NotificationType::AUTH_SUPPLIED, source);
  registrar_.Add(this, NotificationType::AUTH_CANCELLED, source);
}

template <NotificationType::Type T>
class WindowedNavigationObserver
    : public ui_test_utils::WindowedNotificationObserver {
 public:
  explicit WindowedNavigationObserver(NavigationController* controller)
      : ui_test_utils::WindowedNotificationObserver(
          T, Source<NavigationController>(controller)) {}
};

typedef WindowedNavigationObserver<NotificationType::LOAD_STOP>
    WindowedLoadStopObserver;

typedef WindowedNavigationObserver<NotificationType::AUTH_NEEDED>
    WindowedAuthNeededObserver;

typedef WindowedNavigationObserver<NotificationType::AUTH_CANCELLED>
    WindowedAuthCancelledObserver;

typedef WindowedNavigationObserver<NotificationType::AUTH_SUPPLIED>
    WindowedAuthSuppliedObserver;

const char* kPrefetchAuthPage = "files/login/prefetch.html";

const char* kMultiRealmTestPage = "files/login/multi_realm.html";
const int   kMultiRealmTestRealmCount = 2;
const int   kMultiRealmTestResourceCount = 4;

const char* kSingleRealmTestPage = "files/login/single_realm.html";
const int   kSingleRealmTestResourceCount = 6;

// Confirm that <link rel="prefetch"> targetting an auth required
// resource does not provide a login dialog.  These types of requests
// should instead just cancel the auth.

// Unfortunately, this test doesn't assert on anything for its
// correctness.  Instead, it relies on the auth dialog blocking the
// browser, and triggering a timeout to cause failure when the
// prefetch resource requires authorization.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, PrefetchAuthCancels) {
  ASSERT_TRUE(test_server()->Start());

  GURL test_page = test_server()->GetURL(kPrefetchAuthPage);

  class SetPrefetchForTest {
   public:
    explicit SetPrefetchForTest(bool prefetch)
        : old_prefetch_state_(ResourceDispatcherHost::is_prefetch_enabled()) {
      ResourceDispatcherHost::set_is_prefetch_enabled(prefetch);
    }

    ~SetPrefetchForTest() {
      ResourceDispatcherHost::set_is_prefetch_enabled(old_prefetch_state_);
    }
   private:
    bool old_prefetch_state_;
  } set_prefetch_for_test(true);

  TabContentsWrapper* contents =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(contents);
  NavigationController* controller = &contents->controller();
  LoginPromptBrowserTestObserver observer;

  observer.Register(Source<NavigationController>(controller));

  WindowedLoadStopObserver load_stop_waiter(controller);
  browser()->OpenURL(test_page, GURL(), CURRENT_TAB, PageTransition::TYPED);

  load_stop_waiter.Wait();
  EXPECT_TRUE(observer.handlers_.empty());
  EXPECT_TRUE(test_server()->Stop());
}

// Test handling of resources that require authentication even though
// the page they are included on doesn't.  In this case we should only
// present the minimal number of prompts necessary for successfully
// displaying the page.  First we check whether cancelling works as
// expected.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, MultipleRealmCancellation) {
  ASSERT_TRUE(test_server()->Start());
  GURL test_page = test_server()->GetURL(kMultiRealmTestPage);

  TabContentsWrapper* contents =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(contents);

  NavigationController* controller = &contents->controller();
  LoginPromptBrowserTestObserver observer;

  observer.Register(Source<NavigationController>(controller));

  WindowedLoadStopObserver load_stop_waiter(controller);

  {
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(test_page, GURL(), CURRENT_TAB, PageTransition::TYPED);
    auth_needed_waiter.Wait();
  }

  int n_handlers = 0;

  while (n_handlers < kMultiRealmTestRealmCount) {
    WindowedAuthNeededObserver auth_needed_waiter(controller);

    while (!observer.handlers_.empty()) {
      WindowedAuthCancelledObserver auth_cancelled_waiter(controller);
      LoginHandler* handler = *observer.handlers_.begin();

      ASSERT_TRUE(handler);
      n_handlers++;
      handler->CancelAuth();
      auth_cancelled_waiter.Wait();
    }

    if (n_handlers < kMultiRealmTestRealmCount)
      auth_needed_waiter.Wait();
  }

  load_stop_waiter.Wait();

  EXPECT_EQ(kMultiRealmTestRealmCount, n_handlers);
  EXPECT_EQ(0, observer.auth_supplied_count_);
  EXPECT_LT(0, observer.auth_needed_count_);
  EXPECT_LT(0, observer.auth_cancelled_count_);
  EXPECT_TRUE(test_server()->Stop());
}

// Similar to the MultipleRealmCancellation test above, but tests
// whether supplying credentials work as exepcted.
#if defined(OS_WIN)
// See http://crbug.com/70960
#define MAYBE_MultipleRealmConfirmation DISABLED_MultipleRealmConfirmation
#else
#define MAYBE_MultipleRealmConfirmation MultipleRealmConfirmation
#endif
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest,
                       MAYBE_MultipleRealmConfirmation) {
  ASSERT_TRUE(test_server()->Start());
  GURL test_page = test_server()->GetURL(kMultiRealmTestPage);

  TabContentsWrapper* contents =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(contents);

  NavigationController* controller = &contents->controller();
  LoginPromptBrowserTestObserver observer;

  observer.Register(Source<NavigationController>(controller));

  WindowedLoadStopObserver load_stop_waiter(controller);
  int n_handlers = 0;

  {
    WindowedAuthNeededObserver auth_needed_waiter(controller);

    browser()->OpenURL(test_page, GURL(), CURRENT_TAB, PageTransition::TYPED);
    auth_needed_waiter.Wait();
  }

  while (n_handlers < kMultiRealmTestRealmCount) {
    WindowedAuthNeededObserver auth_needed_waiter(controller);

    while (!observer.handlers_.empty()) {
      WindowedAuthSuppliedObserver auth_supplied_waiter(controller);
      LoginHandler* handler = *observer.handlers_.begin();

      ASSERT_TRUE(handler);
      n_handlers++;
      SetAuthFor(handler);
      auth_supplied_waiter.Wait();
    }

    if (n_handlers < kMultiRealmTestRealmCount)
      auth_needed_waiter.Wait();
  }

  load_stop_waiter.Wait();

  EXPECT_EQ(kMultiRealmTestRealmCount, n_handlers);
  EXPECT_LT(0, observer.auth_needed_count_);
  EXPECT_LT(0, observer.auth_supplied_count_);
  EXPECT_EQ(0, observer.auth_cancelled_count_);
  EXPECT_TRUE(test_server()->Stop());
}

// Testing for recovery from an incorrect password for the case where
// there are multiple authenticated resources.
// Marked as flaky.  See http://crbug.com/69266 and http://crbug.com/68860
// TODO(asanka): Remove logging when timeout issues are resolved.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, DISABLED_IncorrectConfirmation) {
  ASSERT_TRUE(test_server()->Start());
  GURL test_page = test_server()->GetURL(kSingleRealmTestPage);

  TabContentsWrapper* contents =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(contents);

  NavigationController* controller = &contents->controller();
  LoginPromptBrowserTestObserver observer;

  observer.Register(Source<NavigationController>(controller));

  WindowedLoadStopObserver load_stop_waiter(controller);

  LOG(INFO) <<
      "Begin test run "
      "(tracing for potential hang. crbug.com/69266)";
  {
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(test_page, GURL(), CURRENT_TAB, PageTransition::TYPED);
    LOG(INFO) << "Waiting for initial AUTH_NEEDED";
    auth_needed_waiter.Wait();
  }

  EXPECT_FALSE(observer.handlers_.empty());

  if (!observer.handlers_.empty()) {
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    WindowedAuthSuppliedObserver auth_supplied_waiter(controller);
    LoginHandler* handler = *observer.handlers_.begin();

    ASSERT_TRUE(handler);
    handler->SetAuth(bad_username_, bad_password_);
    LOG(INFO) << "Waiting for initial AUTH_SUPPLIED";
    auth_supplied_waiter.Wait();

    // The request should be retried after the incorrect password is
    // supplied.  This should result in a new AUTH_NEEDED notification
    // for the same realm.
    LOG(INFO) << "Waiting for secondary AUTH_NEEDED";
    auth_needed_waiter.Wait();
  }

  int n_handlers = 0;

  while (n_handlers < 1) {
    WindowedAuthNeededObserver auth_needed_waiter(controller);

    while (!observer.handlers_.empty()) {
      WindowedAuthSuppliedObserver auth_supplied_waiter(controller);
      LoginHandler* handler = *observer.handlers_.begin();

      ASSERT_TRUE(handler);
      n_handlers++;
      SetAuthFor(handler);
      LOG(INFO) << "Waiting for secondary AUTH_SUPPLIED";
      auth_supplied_waiter.Wait();
    }

    if (n_handlers < 1) {
      LOG(INFO) << "Waiting for additional AUTH_NEEDED";
      auth_needed_waiter.Wait();
    }
  }

  // The single realm test has only one realm, and thus only one login
  // prompt.
  EXPECT_EQ(1, n_handlers);
  EXPECT_LT(0, observer.auth_needed_count_);
  EXPECT_EQ(0, observer.auth_cancelled_count_);
  EXPECT_EQ(observer.auth_needed_count_, observer.auth_supplied_count_);
  LOG(INFO) << "Waiting for LOAD_STOP";
  load_stop_waiter.Wait();
  EXPECT_TRUE(test_server()->Stop());
  LOG(INFO) << "Done with test";
}
}  // namespace
