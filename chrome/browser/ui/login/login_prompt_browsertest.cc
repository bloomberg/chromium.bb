// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <list>
#include <map>

#include "base/utf_string_conversions.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/test/test_browser_thread.h"
#include "net/base/auth.h"
#include "net/base/mock_host_resolver.h"

namespace {

class LoginPromptBrowserTest : public InProcessBrowserTest {
 public:
  LoginPromptBrowserTest()
      : bad_password_("incorrect"), bad_username_("nouser") {
    set_show_window(true);

    auth_map_["foo"] = AuthInfo("testuser", "foopassword");
    auth_map_["bar"] = AuthInfo("testuser", "barpassword");
    auth_map_["testrealm"] = AuthInfo("basicuser", "secret");
  }

 protected:
  struct AuthInfo {
    std::string username_;
    std::string password_;

    AuthInfo() {}

    AuthInfo(const std::string username,
             const std::string password)
        : username_(username), password_(password) {}
  };

  typedef std::map<std::string, AuthInfo> AuthMap;

  void SetAuthFor(LoginHandler* handler);

  AuthMap auth_map_;
  std::string bad_password_;
  std::string bad_username_;
};

void LoginPromptBrowserTest::SetAuthFor(LoginHandler* handler) {
  const net::AuthChallengeInfo* challenge = handler->auth_info();

  ASSERT_TRUE(challenge);
  AuthMap::iterator i = auth_map_.find(challenge->realm);
  EXPECT_TRUE(auth_map_.end() != i);
  if (i != auth_map_.end()) {
    const AuthInfo& info = i->second;
    handler->SetAuth(UTF8ToUTF16(info.username_),
                     UTF8ToUTF16(info.password_));
  }
}

// Maintains a set of LoginHandlers that are currently active and
// keeps a count of the notifications that were observed.
class LoginPromptBrowserTestObserver : public content::NotificationObserver {
 public:
  LoginPromptBrowserTestObserver()
      : auth_needed_count_(0),
        auth_supplied_count_(0),
        auth_cancelled_count_(0) {}

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  void AddHandler(LoginHandler* handler);

  void RemoveHandler(LoginHandler* handler);

  void Register(const content::NotificationSource& source);

  std::list<LoginHandler*> handlers_;

  // The exact number of notifications we receive is depedent on the
  // number of requests that were dispatched and is subject to a
  // number of factors that we don't directly control here.  The
  // values below should only be used qualitatively.
  int auth_needed_count_;
  int auth_supplied_count_;
  int auth_cancelled_count_;

 private:
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(LoginPromptBrowserTestObserver);
};

void LoginPromptBrowserTestObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_AUTH_NEEDED) {
    LoginNotificationDetails* login_details =
        content::Details<LoginNotificationDetails>(details).ptr();
    AddHandler(login_details->handler());
    auth_needed_count_++;
  } else if (type == chrome::NOTIFICATION_AUTH_SUPPLIED) {
    AuthSuppliedLoginNotificationDetails* login_details =
        content::Details<AuthSuppliedLoginNotificationDetails>(details).ptr();
    RemoveHandler(login_details->handler());
    auth_supplied_count_++;
  } else if (type == chrome::NOTIFICATION_AUTH_CANCELLED) {
    LoginNotificationDetails* login_details =
        content::Details<LoginNotificationDetails>(details).ptr();
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
    const content::NotificationSource& source) {
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_NEEDED, source);
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_SUPPLIED, source);
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_CANCELLED, source);
}

template <int T>
class WindowedNavigationObserver
    : public ui_test_utils::WindowedNotificationObserver {
 public:
  explicit WindowedNavigationObserver(NavigationController* controller)
      : ui_test_utils::WindowedNotificationObserver(
          T, content::Source<NavigationController>(controller)) {}
};

// LOAD_STOP observer is special since we want to be able to wait for
// multiple LOAD_STOP events.
class WindowedLoadStopObserver
    : public WindowedNavigationObserver<content::NOTIFICATION_LOAD_STOP> {
 public:
  WindowedLoadStopObserver(NavigationController* controller,
                           int notification_count)
      : WindowedNavigationObserver<content::NOTIFICATION_LOAD_STOP>(controller),
        remaining_notification_count_(notification_count) {}
 protected:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
 private:
  int remaining_notification_count_;  // Number of notifications remaining.
};

void WindowedLoadStopObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (--remaining_notification_count_ == 0)
    WindowedNotificationObserver::Observe(type, source, details);
}

typedef WindowedNavigationObserver<chrome::NOTIFICATION_AUTH_NEEDED>
    WindowedAuthNeededObserver;

typedef WindowedNavigationObserver<chrome::NOTIFICATION_AUTH_CANCELLED>
    WindowedAuthCancelledObserver;

typedef WindowedNavigationObserver<chrome::NOTIFICATION_AUTH_SUPPLIED>
    WindowedAuthSuppliedObserver;

const char* kPrefetchAuthPage = "files/login/prefetch.html";

const char* kMultiRealmTestPage = "files/login/multi_realm.html";
const int   kMultiRealmTestRealmCount = 2;
const int   kMultiRealmTestResourceCount = 4;

const char* kSingleRealmTestPage = "files/login/single_realm.html";
const int   kSingleRealmTestResourceCount = 6;

const char* kAuthBasicPage = "auth-basic";

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
        : old_prefetch_state_(ResourceDispatcherHost::is_prefetch_enabled()),
          old_mode_(prerender::PrerenderManager::GetMode()) {
      ResourceDispatcherHost::set_is_prefetch_enabled(prefetch);
      // Disable prerender so this is just a prefetch of the top-level page.
      prerender::PrerenderManager::SetMode(
          prerender::PrerenderManager::PRERENDER_MODE_DISABLED);
    }

    ~SetPrefetchForTest() {
      ResourceDispatcherHost::set_is_prefetch_enabled(old_prefetch_state_);
      prerender::PrerenderManager::SetMode(old_mode_);
    }
   private:
    bool old_prefetch_state_;
    prerender::PrerenderManager::PrerenderManagerMode old_mode_;
  } set_prefetch_for_test(true);

  TabContentsWrapper* contents =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(contents);
  NavigationController* controller = &contents->controller();
  LoginPromptBrowserTestObserver observer;

  observer.Register(content::Source<NavigationController>(controller));

  WindowedLoadStopObserver load_stop_waiter(controller, 1);
  browser()->OpenURL(
      test_page, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);

  load_stop_waiter.Wait();
  EXPECT_TRUE(observer.handlers_.empty());
  EXPECT_TRUE(test_server()->Stop());
}

// Test login prompt cancellation.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, TestCancelAuth) {
  ASSERT_TRUE(test_server()->Start());
  GURL auth_page = test_server()->GetURL(kAuthBasicPage);
  GURL no_auth_page_1 = test_server()->GetURL("a");
  GURL no_auth_page_2 = test_server()->GetURL("b");
  GURL no_auth_page_3 = test_server()->GetURL("c");

  TabContentsWrapper* contents =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(contents);

  NavigationController* controller = &contents->controller();

  LoginPromptBrowserTestObserver observer;
  observer.Register(content::Source<NavigationController>(controller));

  // First navigate to an unauthenticated page so we have something to
  // go back to.
  ui_test_utils::NavigateToURL(browser(), no_auth_page_1);

  // Navigating while auth is requested is the same as cancelling.
  {
    // We need to wait for two LOAD_STOP events.  One for auth_page and one for
    // no_auth_page_2.
    WindowedLoadStopObserver load_stop_waiter(controller, 2);
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(
        auth_page, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
    auth_needed_waiter.Wait();
    WindowedAuthCancelledObserver auth_cancelled_waiter(controller);
    browser()->OpenURL(
        no_auth_page_2, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
    auth_cancelled_waiter.Wait();
    load_stop_waiter.Wait();
    EXPECT_TRUE(observer.handlers_.empty());
  }

  // Try navigating backwards.
  {
    // As above, we wait for two LOAD_STOP events; one for each navigation.
    WindowedLoadStopObserver load_stop_waiter(controller, 2);
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(
        auth_page, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
    auth_needed_waiter.Wait();
    WindowedAuthCancelledObserver auth_cancelled_waiter(controller);
    ASSERT_TRUE(browser()->CanGoBack());
    browser()->GoBack(CURRENT_TAB);
    auth_cancelled_waiter.Wait();
    load_stop_waiter.Wait();
    EXPECT_TRUE(observer.handlers_.empty());
  }

  // Now add a page and go back, so we have something to go forward to.
  ui_test_utils::NavigateToURL(browser(), no_auth_page_3);
  {
    WindowedLoadStopObserver load_stop_waiter(controller, 1);
    browser()->GoBack(CURRENT_TAB);  // Should take us to page 1
    load_stop_waiter.Wait();
  }

  {
    // We wait for two LOAD_STOP events; one for each navigation.
    WindowedLoadStopObserver load_stop_waiter(controller, 2);
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(
        auth_page, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
    auth_needed_waiter.Wait();
    WindowedAuthCancelledObserver auth_cancelled_waiter(controller);
    ASSERT_TRUE(browser()->CanGoForward());
    browser()->GoForward(CURRENT_TAB);  // Should take us to page 3
    auth_cancelled_waiter.Wait();
    load_stop_waiter.Wait();
    EXPECT_TRUE(observer.handlers_.empty());
  }

  // Now test that cancelling works as expected.
  {
    WindowedLoadStopObserver load_stop_waiter(controller, 1);
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(
        auth_page, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
    auth_needed_waiter.Wait();
    WindowedAuthCancelledObserver auth_cancelled_waiter(controller);
    LoginHandler* handler = *observer.handlers_.begin();
    ASSERT_TRUE(handler);
    handler->CancelAuth();
    auth_cancelled_waiter.Wait();
    load_stop_waiter.Wait();
    EXPECT_TRUE(observer.handlers_.empty());
  }
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

  observer.Register(content::Source<NavigationController>(controller));

  WindowedLoadStopObserver load_stop_waiter(controller, 1);

  {
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(
        test_page, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
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
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, MultipleRealmConfirmation) {
  ASSERT_TRUE(test_server()->Start());
  GURL test_page = test_server()->GetURL(kMultiRealmTestPage);

  TabContentsWrapper* contents =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(contents);

  NavigationController* controller = &contents->controller();
  LoginPromptBrowserTestObserver observer;

  observer.Register(content::Source<NavigationController>(controller));

  WindowedLoadStopObserver load_stop_waiter(controller, 1);
  int n_handlers = 0;

  {
    WindowedAuthNeededObserver auth_needed_waiter(controller);

    browser()->OpenURL(
        test_page, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
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
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, IncorrectConfirmation) {
  ASSERT_TRUE(test_server()->Start());
  GURL test_page = test_server()->GetURL(kSingleRealmTestPage);

  TabContentsWrapper* contents =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(contents);

  NavigationController* controller = &contents->controller();
  LoginPromptBrowserTestObserver observer;

  observer.Register(content::Source<NavigationController>(controller));

  {
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(
        test_page, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
    auth_needed_waiter.Wait();
  }

  EXPECT_FALSE(observer.handlers_.empty());

  if (!observer.handlers_.empty()) {
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    WindowedAuthSuppliedObserver auth_supplied_waiter(controller);
    LoginHandler* handler = *observer.handlers_.begin();

    ASSERT_TRUE(handler);
    handler->SetAuth(UTF8ToUTF16(bad_username_),
                     UTF8ToUTF16(bad_password_));
    auth_supplied_waiter.Wait();

    // The request should be retried after the incorrect password is
    // supplied.  This should result in a new AUTH_NEEDED notification
    // for the same realm.
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
      auth_supplied_waiter.Wait();
    }

    if (n_handlers < 1)
      auth_needed_waiter.Wait();
  }

  // The single realm test has only one realm, and thus only one login
  // prompt.
  EXPECT_EQ(1, n_handlers);
  EXPECT_LT(0, observer.auth_needed_count_);
  EXPECT_EQ(0, observer.auth_cancelled_count_);
  EXPECT_EQ(observer.auth_needed_count_, observer.auth_supplied_count_);
  EXPECT_TRUE(test_server()->Stop());
}

// If the favicon is an authenticated resource, we shouldn't prompt
// for credentials.  The same URL, if requested elsewhere should
// prompt for credentials.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, NoLoginPromptForFavicon) {
  const char* kFaviconTestPage = "files/login/has_favicon.html";
  const char* kFaviconResource = "auth-basic/favicon.gif";

  ASSERT_TRUE(test_server()->Start());

  TabContentsWrapper* contents =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(contents);

  NavigationController* controller = &contents->controller();
  LoginPromptBrowserTestObserver observer;

  observer.Register(content::Source<NavigationController>(controller));

  // First load a page that has a favicon that requires
  // authentication.  There should be no login prompt.
  {
    GURL test_page = test_server()->GetURL(kFaviconTestPage);
    WindowedLoadStopObserver load_stop_waiter(controller, 1);
    browser()->OpenURL(
        test_page, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
    load_stop_waiter.Wait();
  }

  // Now request the same favicon, but directly as the document.
  // There should be one login prompt.
  {
    GURL test_page = test_server()->GetURL(kFaviconResource);
    WindowedLoadStopObserver load_stop_waiter(controller, 1);
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(
        test_page, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
    auth_needed_waiter.Wait();
    ASSERT_EQ(1u, observer.handlers_.size());

    while (!observer.handlers_.empty()) {
      WindowedAuthCancelledObserver auth_cancelled_waiter(controller);
      LoginHandler* handler = *observer.handlers_.begin();

      ASSERT_TRUE(handler);
      handler->CancelAuth();
      auth_cancelled_waiter.Wait();
    }

    load_stop_waiter.Wait();
  }

  EXPECT_EQ(0, observer.auth_supplied_count_);
  EXPECT_EQ(1, observer.auth_needed_count_);
  EXPECT_EQ(1, observer.auth_cancelled_count_);
  EXPECT_TRUE(test_server()->Stop());
}

// Block crossdomain subresource login prompting as a phishing defense.
IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, BlockCrossdomainPrompt) {
  const char* kTestPage = "files/login/load_img_from_b.html";

  host_resolver()->AddRule("www.a.com", "127.0.0.1");
  host_resolver()->AddRule("www.b.com", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  TabContentsWrapper* contents = browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(contents);

  NavigationController* controller = &contents->controller();
  LoginPromptBrowserTestObserver observer;
  observer.Register(content::Source<NavigationController>(controller));

  // Load a page that has a cross-domain sub-resource authentication.
  // There should be no login prompt.
  {
    GURL test_page = test_server()->GetURL(kTestPage);
    ASSERT_EQ("127.0.0.1", test_page.host());

    // Change the host from 127.0.0.1 to www.a.com so that when the
    // page tries to load from b, it will be cross-origin.
    std::string new_host("www.a.com");
    GURL::Replacements replacements;
    replacements.SetHostStr(new_host);
    test_page = test_page.ReplaceComponents(replacements);

    WindowedLoadStopObserver load_stop_waiter(controller, 1);
    browser()->OpenURL(
        test_page, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
    load_stop_waiter.Wait();
  }

  EXPECT_EQ(0, observer.auth_needed_count_);

  // Now request the same page, but from the same origin.
  // There should be one login prompt.
  {
    GURL test_page = test_server()->GetURL(kTestPage);
    ASSERT_EQ("127.0.0.1", test_page.host());

    // Change the host from 127.0.0.1 to www.b.com so that when the
    // page tries to load from b, it will be same-origin.
    std::string new_host("www.b.com");
    GURL::Replacements replacements;
    replacements.SetHostStr(new_host);
    test_page = test_page.ReplaceComponents(replacements);

    WindowedAuthNeededObserver auth_needed_waiter(controller);
    browser()->OpenURL(
        test_page, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
    auth_needed_waiter.Wait();
    ASSERT_EQ(1u, observer.handlers_.size());

    while (!observer.handlers_.empty()) {
      WindowedAuthCancelledObserver auth_cancelled_waiter(controller);
      LoginHandler* handler = *observer.handlers_.begin();

      ASSERT_TRUE(handler);
      handler->CancelAuth();
      auth_cancelled_waiter.Wait();
    }
  }

  EXPECT_EQ(1, observer.auth_needed_count_);
  EXPECT_TRUE(test_server()->Stop());
}

IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, SupplyRedundantAuths) {
  ASSERT_TRUE(test_server()->Start());

  // Get NavigationController for tab 1.
  TabContentsWrapper* contents_1 =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(contents_1);
  NavigationController* controller_1 = &contents_1->controller();

  // Open a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("about:blank"),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // Get NavigationController for tab 2.
  TabContentsWrapper* contents_2 =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(contents_2);
  ASSERT_NE(contents_1, contents_2);
  NavigationController* controller_2 = &contents_2->controller();

  LoginPromptBrowserTestObserver observer;
  observer.Register(content::Source<NavigationController>(controller_1));
  observer.Register(content::Source<NavigationController>(controller_2));

  {
    // Open different auth urls in each tab.
    WindowedAuthNeededObserver auth_needed_waiter_1(controller_1);
    WindowedAuthNeededObserver auth_needed_waiter_2(controller_2);
    contents_1->tab_contents()->OpenURL(OpenURLParams(
        test_server()->GetURL("auth-basic/1"),
        GURL(),
        CURRENT_TAB,
        content::PAGE_TRANSITION_TYPED,
        false));
    contents_2->tab_contents()->OpenURL(OpenURLParams(
        test_server()->GetURL("auth-basic/2"),
        GURL(),
        CURRENT_TAB,
        content::PAGE_TRANSITION_TYPED,
        false));
    auth_needed_waiter_1.Wait();
    auth_needed_waiter_2.Wait();

    ASSERT_EQ(2U, observer.handlers_.size());

    // Supply auth in one of the tabs.
    WindowedAuthSuppliedObserver auth_supplied_waiter_1(controller_1);
    WindowedAuthSuppliedObserver auth_supplied_waiter_2(controller_2);
    LoginHandler* handler_1 = *observer.handlers_.begin();
    ASSERT_TRUE(handler_1);
    SetAuthFor(handler_1);

    // Both tabs should be authenticated.
    auth_supplied_waiter_1.Wait();
    auth_supplied_waiter_2.Wait();
  }

  EXPECT_EQ(2, observer.auth_needed_count_);
  EXPECT_EQ(2, observer.auth_supplied_count_);
  EXPECT_EQ(0, observer.auth_cancelled_count_);
  EXPECT_TRUE(test_server()->Stop());
}

IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest, CancelRedundantAuths) {
  ASSERT_TRUE(test_server()->Start());

  // Get NavigationController for tab 1.
  TabContentsWrapper* contents_1 =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(contents_1);
  NavigationController* controller_1 = &contents_1->controller();

  // Open a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL("about:blank"),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // Get NavigationController for tab 2.
  TabContentsWrapper* contents_2 =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(contents_2);
  ASSERT_NE(contents_1, contents_2);
  NavigationController* controller_2 = &contents_2->controller();

  LoginPromptBrowserTestObserver observer;
  observer.Register(content::Source<NavigationController>(controller_1));
  observer.Register(content::Source<NavigationController>(controller_2));

  {
    // Open different auth urls in each tab.
    WindowedAuthNeededObserver auth_needed_waiter_1(controller_1);
    WindowedAuthNeededObserver auth_needed_waiter_2(controller_2);
    contents_1->tab_contents()->OpenURL(OpenURLParams(
        test_server()->GetURL("auth-basic/1"),
        GURL(),
        CURRENT_TAB,
        content::PAGE_TRANSITION_TYPED,
        false));
    contents_2->tab_contents()->OpenURL(OpenURLParams(
        test_server()->GetURL("auth-basic/2"),
        GURL(),
        CURRENT_TAB,
        content::PAGE_TRANSITION_TYPED,
        false));
    auth_needed_waiter_1.Wait();
    auth_needed_waiter_2.Wait();

    ASSERT_EQ(2U, observer.handlers_.size());

    // Cancel auth in one of the tabs.
    WindowedAuthCancelledObserver auth_cancelled_waiter_1(controller_1);
    WindowedAuthCancelledObserver auth_cancelled_waiter_2(controller_2);
    LoginHandler* handler_1 = *observer.handlers_.begin();
    ASSERT_TRUE(handler_1);
    handler_1->CancelAuth();

    // Both tabs should cancel auth.
    auth_cancelled_waiter_1.Wait();
    auth_cancelled_waiter_2.Wait();
  }

  EXPECT_EQ(2, observer.auth_needed_count_);
  EXPECT_EQ(0, observer.auth_supplied_count_);
  EXPECT_EQ(2, observer.auth_cancelled_count_);
  EXPECT_TRUE(test_server()->Stop());
}

IN_PROC_BROWSER_TEST_F(LoginPromptBrowserTest,
                       SupplyRedundantAuthsMultiProfile) {
  ASSERT_TRUE(test_server()->Start());

  // Get NavigationController for regular tab.
  TabContentsWrapper* contents =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(contents);
  NavigationController* controller = &contents->controller();

  // Open an incognito window.
  Browser* browser_incognito = CreateIncognitoBrowser();

  // Get NavigationController for incognito tab.
  TabContentsWrapper* contents_incognito =
      browser_incognito->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(contents_incognito);
  ASSERT_NE(contents, contents_incognito);
  NavigationController* controller_incognito =
      &contents_incognito->controller();

  LoginPromptBrowserTestObserver observer;
  observer.Register(content::Source<NavigationController>(controller));
  LoginPromptBrowserTestObserver observer_incognito;
  observer_incognito.Register(
      content::Source<NavigationController>(controller_incognito));

  {
    // Open an auth url in each window.
    WindowedAuthNeededObserver auth_needed_waiter(controller);
    WindowedAuthNeededObserver auth_needed_waiter_incognito(
        controller_incognito);
    contents->tab_contents()->OpenURL(OpenURLParams(
        test_server()->GetURL("auth-basic/1"),
        GURL(),
        CURRENT_TAB,
        content::PAGE_TRANSITION_TYPED,
        false));
    contents_incognito->tab_contents()->OpenURL(OpenURLParams(
        test_server()->GetURL("auth-basic/2"),
        GURL(),
        CURRENT_TAB,
        content::PAGE_TRANSITION_TYPED,
        false));
    auth_needed_waiter.Wait();
    auth_needed_waiter_incognito.Wait();

    ASSERT_EQ(1U, observer.handlers_.size());
    ASSERT_EQ(1U, observer_incognito.handlers_.size());

    // Supply auth in regular tab.
    WindowedAuthSuppliedObserver auth_supplied_waiter(controller);
    LoginHandler* handler = *observer.handlers_.begin();
    ASSERT_TRUE(handler);
    SetAuthFor(handler);

    // Regular tab should be authenticated.
    auth_supplied_waiter.Wait();

    // There's not really a way to wait for the incognito window to "do
    // nothing".  Run anything pending in the message loop just to be sure.
    // (This shouldn't be necessary since notifications are synchronous, but
    // maybe it will help avoid flake someday in the future..)
    ui_test_utils::RunAllPendingInMessageLoop();
  }

  EXPECT_EQ(1, observer.auth_needed_count_);
  EXPECT_EQ(1, observer.auth_supplied_count_);
  EXPECT_EQ(0, observer.auth_cancelled_count_);
  EXPECT_EQ(1, observer_incognito.auth_needed_count_);
  EXPECT_EQ(0, observer_incognito.auth_supplied_count_);
  EXPECT_EQ(0, observer_incognito.auth_cancelled_count_);
  EXPECT_TRUE(test_server()->Stop());
}

}  // namespace
