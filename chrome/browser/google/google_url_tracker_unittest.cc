// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_url_tracker.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/net/test_url_fetcher_factory.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/testing_pref_service.h"
#include "chrome/test/testing_profile.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestNotificationObserver : public NotificationObserver {
 public:
  TestNotificationObserver() : notified_(false) {}
  virtual ~TestNotificationObserver() {}

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    notified_ = true;
  }
  bool notified() const { return notified_; }
  void ClearNotified() { notified_ = false; }
 private:
  bool notified_;
};

class TestInfoBar : public InfoBarDelegate {
 public:
  TestInfoBar(GoogleURLTracker* google_url_tracker,
              const GURL& new_google_url)
      : InfoBarDelegate(NULL),
        google_url_tracker_(google_url_tracker),
        new_google_url_(new_google_url) {}
  virtual ~TestInfoBar() {}
  GoogleURLTracker* google_url_tracker() const { return google_url_tracker_; }
  const GURL& new_google_url() const { return new_google_url_; }

  virtual InfoBar* CreateInfoBar() { return NULL; }

 private:
  GoogleURLTracker* google_url_tracker_;
  GURL new_google_url_;
};

class TestInfoBarDelegateFactory
    : public GoogleURLTracker::InfoBarDelegateFactory {
 public:
  virtual InfoBarDelegate* CreateInfoBar(TabContents* tab_contents,
                                         GoogleURLTracker* google_url_tracker,
                                         const GURL& new_google_url) {
    return new TestInfoBar(google_url_tracker, new_google_url);
  }
};

}  // anonymous namespace

class GoogleURLTrackerTest : public testing::Test {
 protected:
  GoogleURLTrackerTest()
      : message_loop_(NULL),
        io_thread_(NULL),
        original_default_request_context_(NULL) {
  }

  void SetUp() {
    original_default_request_context_ = Profile::GetDefaultRequestContext();
    Profile::set_default_request_context(NULL);
    message_loop_ = new MessageLoop(MessageLoop::TYPE_IO);
    io_thread_ = new ChromeThread(ChromeThread::IO, message_loop_);
    network_change_notifier_.reset(net::NetworkChangeNotifier::CreateMock());
    testing_profile_.reset(new TestingProfile);
    TestingBrowserProcess* testing_browser_process =
        static_cast<TestingBrowserProcess*>(g_browser_process);
    PrefService* pref_service = testing_profile_->GetPrefs();
    testing_browser_process->SetPrefService(pref_service);
    testing_browser_process->SetGoogleURLTracker(new GoogleURLTracker);

    URLFetcher::set_factory(&fetcher_factory_);
    observer_.reset(new TestNotificationObserver);
    g_browser_process->google_url_tracker()->infobar_factory_.reset(
        new TestInfoBarDelegateFactory);
  }

  void TearDown() {
    URLFetcher::set_factory(NULL);
    TestingBrowserProcess* testing_browser_process =
        static_cast<TestingBrowserProcess*>(g_browser_process);
    testing_browser_process->SetGoogleURLTracker(NULL);
    testing_browser_process->SetPrefService(NULL);
    testing_profile_.reset();
    network_change_notifier_.reset();
    delete io_thread_;
    delete message_loop_;
    Profile::set_default_request_context(original_default_request_context_);
    original_default_request_context_ = NULL;
  }

  void CreateRequestContext() {
    testing_profile_->CreateRequestContext();
    Profile::set_default_request_context(testing_profile_->GetRequestContext());
    NotificationService::current()->Notify(
        NotificationType::DEFAULT_REQUEST_CONTEXT_AVAILABLE,
        NotificationService::AllSources(), NotificationService::NoDetails());
  }

  TestURLFetcher* GetFetcherByID(int expected_id) {
    return fetcher_factory_.GetFetcherByID(expected_id);
  }

  void MockSearchDomainCheckResponse(
      int expected_id, const std::string& domain) {
    TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(expected_id);
    ASSERT_TRUE(fetcher);
    fetcher->delegate()->OnURLFetchComplete(
        fetcher,
        GURL(GoogleURLTracker::kSearchDomainCheckURL),
        URLRequestStatus(),
        200,
        ResponseCookies(),
        domain);
    MessageLoop::current()->RunAllPending();
  }

  void RequestServerCheck() {
    registrar_.Add(observer_.get(), NotificationType::GOOGLE_URL_UPDATED,
                   NotificationService::AllSources());
    GoogleURLTracker::RequestServerCheck();
    MessageLoop::current()->RunAllPending();
  }

  void FinishSleep() {
    g_browser_process->google_url_tracker()->FinishSleep();
    MessageLoop::current()->RunAllPending();
  }

  void NotifyIPAddressChanged() {
    net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    MessageLoop::current()->RunAllPending();
  }

  GURL GetFetchedGoogleURL() {
    return g_browser_process->google_url_tracker()->fetched_google_url_;
  }

  void SetGoogleURL(const GURL& url) {
    g_browser_process->google_url_tracker()->google_url_ = url;
  }

  void SetLastPromptedGoogleURL(const GURL& url) {
    g_browser_process->local_state()->SetString(
        prefs::kLastPromptedGoogleURL, url.spec());
  }

  GURL GetLastPromptedGoogleURL() {
    return GURL(g_browser_process->local_state()->GetString(
        prefs::kLastPromptedGoogleURL));
  }

  void SearchCommitted(const GURL& search_url) {
    GoogleURLTracker* google_url_tracker =
        g_browser_process->google_url_tracker();

    google_url_tracker->SearchCommitted();
    // GoogleURLTracker wait for NAV_ENTRY_PENDING.
    // In NAV_ENTRY_PENDING, it will set search_url_.
    google_url_tracker->search_url_ = search_url;
  }

  void NavEntryCommitted() {
    GoogleURLTracker* google_url_tracker =
        g_browser_process->google_url_tracker();
    google_url_tracker->ShowGoogleURLInfoBarIfNecessary(NULL);
  }

  bool InfoBarIsShown() {
    return (g_browser_process->google_url_tracker()->infobar_ != NULL);
  }

  GURL GetInfoBarShowingURL() {
    TestInfoBar* infobar = static_cast<TestInfoBar*>(
        g_browser_process->google_url_tracker()->infobar_);
    return infobar->new_google_url();
  }

  void AcceptGoogleURL() {
    TestInfoBar* infobar = static_cast<TestInfoBar*>(
        g_browser_process->google_url_tracker()->infobar_);
    ASSERT_TRUE(infobar);
    ASSERT_TRUE(infobar->google_url_tracker());
    infobar->google_url_tracker()->AcceptGoogleURL(infobar->new_google_url());
  }

  void CancelGoogleURL() {
    TestInfoBar* infobar = static_cast<TestInfoBar*>(
        g_browser_process->google_url_tracker()->infobar_);
    ASSERT_TRUE(infobar);
    ASSERT_TRUE(infobar->google_url_tracker());
    infobar->google_url_tracker()->CancelGoogleURL(infobar->new_google_url());
  }

  void InfoBarClosed() {
    TestInfoBar* infobar = static_cast<TestInfoBar*>(
        g_browser_process->google_url_tracker()->infobar_);
    ASSERT_TRUE(infobar);
    ASSERT_TRUE(infobar->google_url_tracker());
    infobar->google_url_tracker()->InfoBarClosed();
    delete infobar;
  }

  void ExpectDefaultURLs() {
    EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage),
              GoogleURLTracker::GoogleURL());
    EXPECT_EQ(GURL(), GetFetchedGoogleURL());
  }

 private:
  MessageLoop* message_loop_;
  ChromeThread* io_thread_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_ptr<TestingProfile> testing_profile_;

  TestURLFetcherFactory fetcher_factory_;
  NotificationRegistrar registrar_;
  scoped_ptr<NotificationObserver> observer_;

  URLRequestContextGetter* original_default_request_context_;
};

TEST_F(GoogleURLTrackerTest, StartupSleepFinish) {
  CreateRequestContext();

  RequestServerCheck();
  EXPECT_FALSE(GetFetcherByID(0));
  ExpectDefaultURLs();

  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.uk");

  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetFetchedGoogleURL());
  // GoogleURL is updated, becase it was not the last prompted URL.
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GoogleURLTracker::GoogleURL());
}

TEST_F(GoogleURLTrackerTest, StartupSleepFinishWithLastPrompted) {
  CreateRequestContext();
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));

  RequestServerCheck();
  EXPECT_FALSE(GetFetcherByID(0));
  ExpectDefaultURLs();

  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.uk");

  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetFetchedGoogleURL());
  // GoogleURL should not be updated.
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage),
            GoogleURLTracker::GoogleURL());
}

TEST_F(GoogleURLTrackerTest, StartupSleepFinishNoObserver) {
  CreateRequestContext();

  ExpectDefaultURLs();

  FinishSleep();
  EXPECT_FALSE(GetFetcherByID(0));

  ExpectDefaultURLs();
}

TEST_F(GoogleURLTrackerTest, MonitorNetworkChange) {
  CreateRequestContext();

  RequestServerCheck();
  EXPECT_FALSE(GetFetcherByID(0));
  ExpectDefaultURLs();

  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.uk");

  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetFetchedGoogleURL());
  // GoogleURL is updated, becase it was not the last prompted URL.
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GoogleURLTracker::GoogleURL());

  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse(1, ".google.co.in");

  EXPECT_EQ(GURL("http://www.google.co.in/"), GetFetchedGoogleURL());
  // Don't update GoogleURL.
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GoogleURLTracker::GoogleURL());
}

TEST_F(GoogleURLTrackerTest, MonitorNetworkChangeNoObserver) {
  CreateRequestContext();

  ExpectDefaultURLs();

  FinishSleep();
  NotifyIPAddressChanged();
  EXPECT_FALSE(GetFetcherByID(0));

  ExpectDefaultURLs();
}

TEST_F(GoogleURLTrackerTest, MonitorNetworkChangeAndObserverRegister) {
  CreateRequestContext();

  ExpectDefaultURLs();

  FinishSleep();
  NotifyIPAddressChanged();
  EXPECT_FALSE(GetFetcherByID(0));

  ExpectDefaultURLs();

  RequestServerCheck();
  MockSearchDomainCheckResponse(0, ".google.co.uk");

  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetFetchedGoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GoogleURLTracker::GoogleURL());
}

TEST_F(GoogleURLTrackerTest, NoSearchCommitedAndPromptedNotChanged) {
  CreateRequestContext();
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  EXPECT_FALSE(GetFetcherByID(0));
  ExpectDefaultURLs();

  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.jp");

  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetFetchedGoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
}

TEST_F(GoogleURLTrackerTest, SearchCommitedAndUserCloseInfoBar) {
  CreateRequestContext();
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));

  ExpectDefaultURLs();

  RequestServerCheck();
  EXPECT_FALSE(GetFetcherByID(0));
  ExpectDefaultURLs();

  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.jp");

  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage),
            GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetFetchedGoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());

  SearchCommitted(GURL("http://www.google.co.uk/search?q=test"));
  NavEntryCommitted();

  EXPECT_TRUE(InfoBarIsShown());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage),
            GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetInfoBarShowingURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());

  InfoBarClosed();
  EXPECT_FALSE(InfoBarIsShown());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage),
            GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
}

TEST_F(GoogleURLTrackerTest, SearchCommitedAndUserSayNo) {
  CreateRequestContext();
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));

  ExpectDefaultURLs();

  RequestServerCheck();
  EXPECT_FALSE(GetFetcherByID(0));
  ExpectDefaultURLs();

  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.jp");

  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage),
            GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetFetchedGoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());

  SearchCommitted(GURL("http://www.google.co.uk/search?q=test"));
  NavEntryCommitted();

  EXPECT_TRUE(InfoBarIsShown());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage),
            GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetInfoBarShowingURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());

  CancelGoogleURL();
  EXPECT_TRUE(InfoBarIsShown());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetLastPromptedGoogleURL());

  InfoBarClosed();
  EXPECT_FALSE(InfoBarIsShown());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage),
            GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetLastPromptedGoogleURL());
}

TEST_F(GoogleURLTrackerTest, SearchCommitedAndUserSayYes) {
  CreateRequestContext();
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));

  ExpectDefaultURLs();

  RequestServerCheck();
  EXPECT_FALSE(GetFetcherByID(0));
  ExpectDefaultURLs();

  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.jp");

  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage),
            GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetFetchedGoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());

  SearchCommitted(GURL("http://www.google.co.uk/search?q=test"));
  NavEntryCommitted();

  EXPECT_TRUE(InfoBarIsShown());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage),
            GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetInfoBarShowingURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());

  AcceptGoogleURL();
  EXPECT_TRUE(InfoBarIsShown());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetLastPromptedGoogleURL());

  InfoBarClosed();
  EXPECT_FALSE(InfoBarIsShown());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetLastPromptedGoogleURL());
}

TEST_F(GoogleURLTrackerTest, InitialUpdate) {
  CreateRequestContext();
  ExpectDefaultURLs();
  EXPECT_EQ(GURL(), GetLastPromptedGoogleURL());

  RequestServerCheck();
  EXPECT_FALSE(GetFetcherByID(0));
  ExpectDefaultURLs();
  EXPECT_EQ(GURL(), GetLastPromptedGoogleURL());

  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.uk");

  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetFetchedGoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());

  SearchCommitted(GURL("http://www.google.co.uk/search?q=test"));
  NavEntryCommitted();

  EXPECT_FALSE(InfoBarIsShown());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetFetchedGoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
}

TEST_F(GoogleURLTrackerTest, UpdatePromptedURLWhenBack) {
  CreateRequestContext();
  SetLastPromptedGoogleURL(GURL("http://www.google.co.jp/"));
  SetGoogleURL(GURL("http://www.google.co.uk/"));

  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.uk");

  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetFetchedGoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());

  SearchCommitted(GURL("http://www.google.co.uk/search?q=test"));
  NavEntryCommitted();

  EXPECT_FALSE(InfoBarIsShown());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetFetchedGoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
}
