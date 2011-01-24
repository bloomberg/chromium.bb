// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_url_tracker.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/profiles/profile.h"
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

// TestNotificationObserver ---------------------------------------------------

namespace {

class TestNotificationObserver : public NotificationObserver {
 public:
  TestNotificationObserver();
  virtual ~TestNotificationObserver();

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
  bool notified() const { return notified_; }
  void clear_notified() { notified_ = false; }

 private:
  bool notified_;
};

TestNotificationObserver::TestNotificationObserver() : notified_(false) {
}

TestNotificationObserver::~TestNotificationObserver() {
}

void TestNotificationObserver::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  notified_ = true;
}


// TestInfoBarDelegate --------------------------------------------------------

class TestInfoBarDelegate : public InfoBarDelegate {
 public:
  TestInfoBarDelegate(GoogleURLTracker* google_url_tracker,
                      const GURL& new_google_url);

  GoogleURLTracker* google_url_tracker() const { return google_url_tracker_; }
  GURL new_google_url() const { return new_google_url_; }

 private:
  virtual ~TestInfoBarDelegate();

  // InfoBarDelegate:
  virtual InfoBar* CreateInfoBar();

  GoogleURLTracker* google_url_tracker_;
  GURL new_google_url_;
};

TestInfoBarDelegate::TestInfoBarDelegate(GoogleURLTracker* google_url_tracker,
                                         const GURL& new_google_url)
    : InfoBarDelegate(NULL),
      google_url_tracker_(google_url_tracker),
      new_google_url_(new_google_url) {
}

TestInfoBarDelegate::~TestInfoBarDelegate() {
}

InfoBar* TestInfoBarDelegate::CreateInfoBar() {
  return NULL;
}

InfoBarDelegate* CreateTestInfobar(
    TabContents* tab_contents,
    GoogleURLTracker* google_url_tracker,
    const GURL& new_google_url) {
  return new TestInfoBarDelegate(google_url_tracker, new_google_url);
}

}  // namespace


// GoogleURLTrackerTest -------------------------------------------------------

class GoogleURLTrackerTest : public testing::Test {
 protected:
  GoogleURLTrackerTest();
  virtual ~GoogleURLTrackerTest();

  // testing::Test
  virtual void SetUp();
  virtual void TearDown();

  void CreateRequestContext();
  TestURLFetcher* GetFetcherByID(int expected_id);
  void MockSearchDomainCheckResponse(int expected_id,
                                     const std::string& domain);
  void RequestServerCheck();
  void FinishSleep();
  void NotifyIPAddressChanged();
  GURL GetFetchedGoogleURL();
  void SetGoogleURL(const GURL& url);
  void SetLastPromptedGoogleURL(const GURL& url);
  GURL GetLastPromptedGoogleURL();
  void SearchCommitted(const GURL& search_url);
  void NavEntryCommitted();
  bool InfoBarIsShown();
  GURL GetInfoBarShowingURL();
  void AcceptGoogleURL();
  void CancelGoogleURL();
  void InfoBarClosed();
  void ExpectDefaultURLs();

  scoped_ptr<TestNotificationObserver> observer_;

 private:
  MessageLoop* message_loop_;
  BrowserThread* io_thread_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_ptr<TestingProfile> testing_profile_;

  TestURLFetcherFactory fetcher_factory_;
  NotificationRegistrar registrar_;

  URLRequestContextGetter* original_default_request_context_;
};

GoogleURLTrackerTest::GoogleURLTrackerTest()
    : observer_(new TestNotificationObserver),
      message_loop_(NULL),
      io_thread_(NULL),
      original_default_request_context_(NULL) {
}

GoogleURLTrackerTest::~GoogleURLTrackerTest() {
}

void GoogleURLTrackerTest::SetUp() {
  original_default_request_context_ = Profile::GetDefaultRequestContext();
  Profile::set_default_request_context(NULL);
  message_loop_ = new MessageLoop(MessageLoop::TYPE_IO);
  io_thread_ = new BrowserThread(BrowserThread::IO, message_loop_);
  network_change_notifier_.reset(net::NetworkChangeNotifier::CreateMock());
  testing_profile_.reset(new TestingProfile);
  TestingBrowserProcess* testing_browser_process =
      static_cast<TestingBrowserProcess*>(g_browser_process);
  PrefService* pref_service = testing_profile_->GetPrefs();
  testing_browser_process->SetPrefService(pref_service);
  GoogleURLTracker* tracker = new GoogleURLTracker;
  tracker->queue_wakeup_task_ = false;
  MessageLoop::current()->RunAllPending();
  testing_browser_process->SetGoogleURLTracker(tracker);

  URLFetcher::set_factory(&fetcher_factory_);
  g_browser_process->google_url_tracker()->infobar_creator_ =
      &CreateTestInfobar;
}

void GoogleURLTrackerTest::TearDown() {
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

void GoogleURLTrackerTest::CreateRequestContext() {
  testing_profile_->CreateRequestContext();
  Profile::set_default_request_context(testing_profile_->GetRequestContext());
  NotificationService::current()->Notify(
      NotificationType::DEFAULT_REQUEST_CONTEXT_AVAILABLE,
      NotificationService::AllSources(), NotificationService::NoDetails());
}

TestURLFetcher* GoogleURLTrackerTest::GetFetcherByID(int expected_id) {
  return fetcher_factory_.GetFetcherByID(expected_id);
}

void GoogleURLTrackerTest::MockSearchDomainCheckResponse(
    int expected_id,
    const std::string& domain) {
  TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(expected_id);
  if (!fetcher)
    return;
  fetcher->delegate()->OnURLFetchComplete(fetcher,
      GURL(GoogleURLTracker::kSearchDomainCheckURL), net::URLRequestStatus(),
      200, ResponseCookies(), domain);
  // At this point, |fetcher| is deleted.
  MessageLoop::current()->RunAllPending();
}

void GoogleURLTrackerTest::RequestServerCheck() {
  if (!registrar_.IsRegistered(observer_.get(),
                               NotificationType::GOOGLE_URL_UPDATED,
                               NotificationService::AllSources())) {
    registrar_.Add(observer_.get(), NotificationType::GOOGLE_URL_UPDATED,
                   NotificationService::AllSources());
  }
  GoogleURLTracker::RequestServerCheck();
  MessageLoop::current()->RunAllPending();
}

void GoogleURLTrackerTest::FinishSleep() {
  g_browser_process->google_url_tracker()->FinishSleep();
  MessageLoop::current()->RunAllPending();
}

void GoogleURLTrackerTest::NotifyIPAddressChanged() {
  net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  MessageLoop::current()->RunAllPending();
}

GURL GoogleURLTrackerTest::GetFetchedGoogleURL() {
  return g_browser_process->google_url_tracker()->fetched_google_url_;
}

void GoogleURLTrackerTest::SetGoogleURL(const GURL& url) {
  g_browser_process->google_url_tracker()->google_url_ = url;
}

void GoogleURLTrackerTest::SetLastPromptedGoogleURL(const GURL& url) {
  g_browser_process->local_state()->SetString(
      prefs::kLastPromptedGoogleURL, url.spec());
}

GURL GoogleURLTrackerTest::GetLastPromptedGoogleURL() {
  return GURL(g_browser_process->local_state()->GetString(
      prefs::kLastPromptedGoogleURL));
}

void GoogleURLTrackerTest::SearchCommitted(const GURL& search_url) {
  GoogleURLTracker* google_url_tracker =
      g_browser_process->google_url_tracker();
  google_url_tracker->SearchCommitted();
  if (google_url_tracker->registrar_.IsRegistered(google_url_tracker,
      NotificationType::NAV_ENTRY_PENDING,
      NotificationService::AllSources()))
    google_url_tracker->search_url_ = search_url;
}

void GoogleURLTrackerTest::NavEntryCommitted() {
  GoogleURLTracker* google_url_tracker =
      g_browser_process->google_url_tracker();
  google_url_tracker->ShowGoogleURLInfoBarIfNecessary(NULL);
}

bool GoogleURLTrackerTest::InfoBarIsShown() {
  return (g_browser_process->google_url_tracker()->infobar_ != NULL);
}

GURL GoogleURLTrackerTest::GetInfoBarShowingURL() {
  TestInfoBarDelegate* infobar = static_cast<TestInfoBarDelegate*>(
      g_browser_process->google_url_tracker()->infobar_);
  return infobar->new_google_url();
}

void GoogleURLTrackerTest::AcceptGoogleURL() {
  TestInfoBarDelegate* infobar = static_cast<TestInfoBarDelegate*>(
      g_browser_process->google_url_tracker()->infobar_);
  ASSERT_TRUE(infobar);
  ASSERT_TRUE(infobar->google_url_tracker());
  infobar->google_url_tracker()->AcceptGoogleURL(infobar->new_google_url());
}

void GoogleURLTrackerTest::CancelGoogleURL() {
  TestInfoBarDelegate* infobar = static_cast<TestInfoBarDelegate*>(
      g_browser_process->google_url_tracker()->infobar_);
  ASSERT_TRUE(infobar);
  ASSERT_TRUE(infobar->google_url_tracker());
  infobar->google_url_tracker()->CancelGoogleURL(infobar->new_google_url());
}

void GoogleURLTrackerTest::InfoBarClosed() {
  InfoBarDelegate* infobar = g_browser_process->google_url_tracker()->infobar_;
  ASSERT_TRUE(infobar);
  GoogleURLTracker* url_tracker =
      static_cast<TestInfoBarDelegate*>(infobar)->google_url_tracker();
  ASSERT_TRUE(url_tracker);
  url_tracker->InfoBarClosed();
  delete infobar;
}

void GoogleURLTrackerTest::ExpectDefaultURLs() {
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage),
            GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL(), GetFetchedGoogleURL());
}


// Tests ----------------------------------------------------------------------

TEST_F(GoogleURLTrackerTest, DontFetchWhenNoOneRequestsCheck) {
  CreateRequestContext();
  ExpectDefaultURLs();
  FinishSleep();
  // No one called RequestServerCheck() so nothing should have happened.
  EXPECT_FALSE(GetFetcherByID(0));
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, UpdateOnFirstRun) {
  CreateRequestContext();
  RequestServerCheck();
  EXPECT_FALSE(GetFetcherByID(0));
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_->notified());

  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.uk");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetFetchedGoogleURL());
  // GoogleURL should be updated, becase there was no last prompted URL.
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GoogleURLTracker::GoogleURL());
  EXPECT_TRUE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, DontUpdateWhenUnchanged) {
  CreateRequestContext();
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));

  RequestServerCheck();
  EXPECT_FALSE(GetFetcherByID(0));
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_->notified());

  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.uk");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetFetchedGoogleURL());
  // GoogleURL should not be updated, because the fetched and prompted URLs
  // match.
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage),
            GoogleURLTracker::GoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, UpdatePromptedURLOnReturnToPreviousLocation) {
  CreateRequestContext();
  SetLastPromptedGoogleURL(GURL("http://www.google.co.jp/"));
  SetGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.uk");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetFetchedGoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, RefetchOnIPAddressChange) {
  CreateRequestContext();
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.uk");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetFetchedGoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GoogleURLTracker::GoogleURL());
  EXPECT_TRUE(observer_->notified());
  observer_->clear_notified();

  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse(1, ".google.co.in");
  EXPECT_EQ(GURL("http://www.google.co.in/"), GetFetchedGoogleURL());
  // Just fetching a new URL shouldn't reset things without a prompt.
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GoogleURLTracker::GoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, DontRefetchWhenNoOneRequestsCheck) {
  CreateRequestContext();
  FinishSleep();
  NotifyIPAddressChanged();
  // No one called RequestServerCheck() so nothing should have happened.
  EXPECT_FALSE(GetFetcherByID(0));
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, FetchOnLateRequest) {
  CreateRequestContext();
  FinishSleep();
  NotifyIPAddressChanged();

  RequestServerCheck();
  // The first request for a check should trigger a fetch if it hasn't happened
  // already.
  MockSearchDomainCheckResponse(0, ".google.co.uk");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetFetchedGoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GoogleURLTracker::GoogleURL());
  EXPECT_TRUE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, SearchingDoesNothingIfNoNeedToPrompt) {
  CreateRequestContext();
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.uk");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetFetchedGoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(observer_->notified());
  observer_->clear_notified();

  SearchCommitted(GURL("http://www.google.co.uk/search?q=test"));
  NavEntryCommitted();
  EXPECT_FALSE(InfoBarIsShown());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetFetchedGoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, InfobarClosed) {
  CreateRequestContext();
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.jp");

  SearchCommitted(GURL("http://www.google.co.uk/search?q=test"));
  NavEntryCommitted();
  EXPECT_TRUE(InfoBarIsShown());

  InfoBarClosed();
  EXPECT_FALSE(InfoBarIsShown());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage),
            GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, InfobarRefused) {
  CreateRequestContext();
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.jp");

  SearchCommitted(GURL("http://www.google.co.uk/search?q=test"));
  NavEntryCommitted();
  EXPECT_TRUE(InfoBarIsShown());

  CancelGoogleURL();
  InfoBarClosed();
  EXPECT_FALSE(InfoBarIsShown());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage),
            GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, InfobarAccepted) {
  CreateRequestContext();
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.jp");

  SearchCommitted(GURL("http://www.google.co.uk/search?q=test"));
  NavEntryCommitted();
  EXPECT_TRUE(InfoBarIsShown());

  AcceptGoogleURL();
  InfoBarClosed();
  EXPECT_FALSE(InfoBarIsShown());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GoogleURLTracker::GoogleURL());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(observer_->notified());
}
