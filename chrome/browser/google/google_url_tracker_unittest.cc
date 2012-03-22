// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/url_fetcher.h"
#include "content/test/test_browser_thread.h"
#include "content/test/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

// TestNotificationObserver ---------------------------------------------------

namespace {

class TestNotificationObserver : public content::NotificationObserver {
 public:
  TestNotificationObserver();
  virtual ~TestNotificationObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);
  bool notified() const { return notified_; }
  void clear_notified() { notified_ = false; }

 private:
  bool notified_;
};

TestNotificationObserver::TestNotificationObserver() : notified_(false) {
}

TestNotificationObserver::~TestNotificationObserver() {
}

void TestNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  notified_ = true;
}


// TestInfoBarDelegate --------------------------------------------------------

class TestInfoBarDelegate : public InfoBarDelegate {
 public:
  TestInfoBarDelegate(GoogleURLTracker* google_url_tracker,
                      const GURL& new_google_url);
  virtual ~TestInfoBarDelegate();

  GoogleURLTracker* google_url_tracker() const { return google_url_tracker_; }
  GURL new_google_url() const { return new_google_url_; }

 private:
  // InfoBarDelegate:
  virtual InfoBar* CreateInfoBar(InfoBarTabHelper* infobar_helper) OVERRIDE;

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

InfoBar* TestInfoBarDelegate::CreateInfoBar(InfoBarTabHelper* infobar_helper) {
  return NULL;
}

InfoBarDelegate* CreateTestInfobar(
    InfoBarTabHelper* infobar_helper,
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

  TestURLFetcher* GetFetcherByID(int expected_id);
  void MockSearchDomainCheckResponse(int expected_id,
                                     const std::string& domain);
  void RequestServerCheck();
  void FinishSleep();
  void NotifyIPAddressChanged();
  GURL fetched_google_url() const {
    return google_url_tracker_->fetched_google_url_;
  }
  void set_google_url(const GURL& url) {
    google_url_tracker_->google_url_ = url;
  }
  GURL google_url() const { return google_url_tracker_->google_url_; }
  void SetLastPromptedGoogleURL(const GURL& url);
  GURL GetLastPromptedGoogleURL();
  void SearchCommitted(const GURL& search_url);
  void NavEntryCommitted();
  bool infobar_showing() const {
    return (google_url_tracker_->infobar_ != NULL);
  }
  GURL infobar_url() const {
    return static_cast<TestInfoBarDelegate*>(google_url_tracker_->infobar_)->
        new_google_url();
  }
  void AcceptGoogleURL();
  void CancelGoogleURL();
  void InfoBarClosed();
  void ExpectDefaultURLs();

  scoped_ptr<TestNotificationObserver> observer_;

 private:
  // These are required by the TestURLFetchers GoogleURLTracker will create (see
  // test_url_fetcher_factory.h).
  MessageLoop message_loop_;
  content::TestBrowserThread io_thread_;
  // Creating this allows us to call
  // net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests().
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  ScopedTestingLocalState local_state_;
  TestURLFetcherFactory fetcher_factory_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<GoogleURLTracker> google_url_tracker_;
};

GoogleURLTrackerTest::GoogleURLTrackerTest()
    : observer_(new TestNotificationObserver),
      message_loop_(MessageLoop::TYPE_IO),
      io_thread_(content::BrowserThread::IO, &message_loop_),
      local_state_(static_cast<TestingBrowserProcess*>(g_browser_process)) {
}

GoogleURLTrackerTest::~GoogleURLTrackerTest() {
}

void GoogleURLTrackerTest::SetUp() {
  network_change_notifier_.reset(net::NetworkChangeNotifier::CreateMock());
  google_url_tracker_.reset(
      new GoogleURLTracker(GoogleURLTracker::UNIT_TEST_MODE));
  google_url_tracker_->infobar_creator_ = &CreateTestInfobar;
}

void GoogleURLTrackerTest::TearDown() {
  google_url_tracker_.reset();
  network_change_notifier_.reset();
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
  fetcher->set_url(GURL(GoogleURLTracker::kSearchDomainCheckURL));
  fetcher->set_response_code(200);
  fetcher->SetResponseString(domain);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  // At this point, |fetcher| is deleted.
}

void GoogleURLTrackerTest::RequestServerCheck() {
  if (!registrar_.IsRegistered(observer_.get(),
      chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
      content::NotificationService::AllBrowserContextsAndSources())) {
    registrar_.Add(observer_.get(), chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
        content::NotificationService::AllBrowserContextsAndSources());
  }
  google_url_tracker_->SetNeedToFetch();
}

void GoogleURLTrackerTest::FinishSleep() {
  google_url_tracker_->FinishSleep();
}

void GoogleURLTrackerTest::NotifyIPAddressChanged() {
  net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  // For thread safety, the NCN queues tasks to do the actual notifications, so
  // we need to spin the message loop so the tracker will actually be notified.
  MessageLoop::current()->RunAllPending();
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
  google_url_tracker_->SearchCommitted();
  if (google_url_tracker_->registrar_.IsRegistered(google_url_tracker_.get(),
      content::NOTIFICATION_NAV_ENTRY_PENDING,
      content::NotificationService::AllBrowserContextsAndSources()))
    google_url_tracker_->search_url_ = search_url;
}

void GoogleURLTrackerTest::NavEntryCommitted() {
  google_url_tracker_->ShowGoogleURLInfoBarIfNecessary(NULL);
}

void GoogleURLTrackerTest::AcceptGoogleURL() {
  TestInfoBarDelegate* infobar =
      static_cast<TestInfoBarDelegate*>(google_url_tracker_->infobar_);
  ASSERT_TRUE(infobar);
  ASSERT_TRUE(infobar->google_url_tracker());
  ASSERT_EQ(google_url_tracker_, infobar->google_url_tracker());
  google_url_tracker_->AcceptGoogleURL(infobar->new_google_url());
}

void GoogleURLTrackerTest::CancelGoogleURL() {
  TestInfoBarDelegate* infobar =
      static_cast<TestInfoBarDelegate*>(google_url_tracker_->infobar_);
  ASSERT_TRUE(infobar);
  ASSERT_TRUE(infobar->google_url_tracker());
  ASSERT_EQ(google_url_tracker_, infobar->google_url_tracker());
  google_url_tracker_->CancelGoogleURL(infobar->new_google_url());
}

void GoogleURLTrackerTest::InfoBarClosed() {
  TestInfoBarDelegate* infobar =
      static_cast<TestInfoBarDelegate*>(google_url_tracker_->infobar_);
  ASSERT_TRUE(infobar);
  ASSERT_TRUE(infobar->google_url_tracker());
  ASSERT_EQ(google_url_tracker_, infobar->google_url_tracker());
  google_url_tracker_->InfoBarClosed();
  delete infobar;
}

void GoogleURLTrackerTest::ExpectDefaultURLs() {
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL(), fetched_google_url());
}


// Tests ----------------------------------------------------------------------

TEST_F(GoogleURLTrackerTest, DontFetchWhenNoOneRequestsCheck) {
  ExpectDefaultURLs();
  FinishSleep();
  // No one called RequestServerCheck() so nothing should have happened.
  EXPECT_FALSE(GetFetcherByID(0));
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, UpdateOnFirstRun) {
  RequestServerCheck();
  EXPECT_FALSE(GetFetcherByID(0));
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_->notified());

  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.uk");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  // GoogleURL should be updated, becase there was no last prompted URL.
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_TRUE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, DontUpdateWhenUnchanged) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));

  RequestServerCheck();
  EXPECT_FALSE(GetFetcherByID(0));
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_->notified());

  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.uk");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  // GoogleURL should not be updated, because the fetched and prompted URLs
  // match.
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, UpdatePromptedURLOnReturnToPreviousLocation) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.jp/"));
  set_google_url(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.uk");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, RefetchOnIPAddressChange) {
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.uk");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_TRUE(observer_->notified());
  observer_->clear_notified();

  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse(1, ".google.co.in");
  EXPECT_EQ(GURL("http://www.google.co.in/"), fetched_google_url());
  // Just fetching a new URL shouldn't reset things without a prompt.
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, DontRefetchWhenNoOneRequestsCheck) {
  FinishSleep();
  NotifyIPAddressChanged();
  // No one called RequestServerCheck() so nothing should have happened.
  EXPECT_FALSE(GetFetcherByID(0));
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, FetchOnLateRequest) {
  FinishSleep();
  NotifyIPAddressChanged();

  RequestServerCheck();
  // The first request for a check should trigger a fetch if it hasn't happened
  // already.
  MockSearchDomainCheckResponse(0, ".google.co.uk");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_TRUE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, SearchingDoesNothingIfNoNeedToPrompt) {
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.uk");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(observer_->notified());
  observer_->clear_notified();

  SearchCommitted(GURL("http://www.google.co.uk/search?q=test"));
  NavEntryCommitted();
  EXPECT_FALSE(infobar_showing());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, InfobarClosed) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.jp");

  SearchCommitted(GURL("http://www.google.co.uk/search?q=test"));
  NavEntryCommitted();
  EXPECT_TRUE(infobar_showing());

  InfoBarClosed();
  EXPECT_FALSE(infobar_showing());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, InfobarRefused) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.jp");

  SearchCommitted(GURL("http://www.google.co.uk/search?q=test"));
  NavEntryCommitted();
  EXPECT_TRUE(infobar_showing());

  CancelGoogleURL();
  InfoBarClosed();
  EXPECT_FALSE(infobar_showing());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, InfobarAccepted) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(0, ".google.co.jp");

  SearchCommitted(GURL("http://www.google.co.uk/search?q=test"));
  NavEntryCommitted();
  EXPECT_TRUE(infobar_showing());

  AcceptGoogleURL();
  InfoBarClosed();
  EXPECT_FALSE(infobar_showing());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(observer_->notified());
}
