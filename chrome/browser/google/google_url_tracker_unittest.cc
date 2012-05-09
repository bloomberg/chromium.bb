// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/infobars/infobar_delegate.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
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

class TestInfoBarDelegate : public GoogleURLTrackerInfoBarDelegate {
 public:
  TestInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                      const GURL& search_url,
                      GoogleURLTracker* google_url_tracker,
                      const GURL& new_google_url);
  virtual ~TestInfoBarDelegate();

  GURL search_url() const { return search_url_; }
  GURL new_google_url() const { return new_google_url_; }
  bool showing() const { return showing_; }

 private:
  // GoogleURLTrackerInfoBarDelegate:
  virtual void Show() OVERRIDE;
  virtual void Close(bool redo_search) OVERRIDE;
};

TestInfoBarDelegate::TestInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                                         const GURL& search_url,
                                         GoogleURLTracker* google_url_tracker,
                                         const GURL& new_google_url)
  : GoogleURLTrackerInfoBarDelegate(NULL, search_url, google_url_tracker,
                                    new_google_url) {
  // We set |map_key_| here instead of in the superclass constructor so that the
  // InfoBarDelegate base class will not try to dereference it, which would fail
  // since this is really a magic number and not an actual pointer.
  map_key_ = infobar_helper;
}

void TestInfoBarDelegate::Show() {
  showing_ = true;
}

void TestInfoBarDelegate::Close(bool redo_search) {
  InfoBarClosed();
}

TestInfoBarDelegate::~TestInfoBarDelegate() {
}

GoogleURLTrackerInfoBarDelegate* CreateTestInfobar(
    InfoBarTabHelper* infobar_helper,
    const GURL& search_url,
    GoogleURLTracker* google_url_tracker,
    const GURL& new_google_url) {
  return new TestInfoBarDelegate(infobar_helper, search_url, google_url_tracker,
                                 new_google_url);
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

  TestURLFetcher* GetFetcher();
  void MockSearchDomainCheckResponse(const std::string& domain);
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
  void SetSearchPending(const GURL& search_url, int unique_id);
  void CommitSearch(int unique_id);
  void CloseTab(int unique_id);
  TestInfoBarDelegate* GetInfoBar(int unique_id);
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
  TestURLFetcherFactory fetcher_factory_;
  content::NotificationRegistrar registrar_;
  TestingProfile profile_;
  scoped_ptr<GoogleURLTracker> google_url_tracker_;
};

GoogleURLTrackerTest::GoogleURLTrackerTest()
    : observer_(new TestNotificationObserver),
      message_loop_(MessageLoop::TYPE_IO),
      io_thread_(content::BrowserThread::IO, &message_loop_) {
  GoogleURLTrackerFactory::GetInstance()->RegisterUserPrefsOnProfile(&profile_);
}

GoogleURLTrackerTest::~GoogleURLTrackerTest() {
}

void GoogleURLTrackerTest::SetUp() {
  network_change_notifier_.reset(net::NetworkChangeNotifier::CreateMock());
  google_url_tracker_.reset(
      new GoogleURLTracker(&profile_, GoogleURLTracker::UNIT_TEST_MODE));
  google_url_tracker_->infobar_creator_ = &CreateTestInfobar;
}

void GoogleURLTrackerTest::TearDown() {
  google_url_tracker_.reset();
  network_change_notifier_.reset();
}

TestURLFetcher* GoogleURLTrackerTest::GetFetcher() {
  // This will return the last fetcher created.  If no fetchers have been
  // created, we'll pass GetFetcherByID() "-1", and it will return NULL.
  return fetcher_factory_.GetFetcherByID(google_url_tracker_->fetcher_id_ - 1);
}

void GoogleURLTrackerTest::MockSearchDomainCheckResponse(
    const std::string& domain) {
  TestURLFetcher* fetcher = GetFetcher();
  if (!fetcher)
    return;
  fetcher_factory_.RemoveFetcherFromMap(fetcher->id());
  fetcher->set_url(GURL(GoogleURLTracker::kSearchDomainCheckURL));
  fetcher->set_response_code(200);
  fetcher->SetResponseString(domain);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  // At this point, |fetcher| is deleted.
}

void GoogleURLTrackerTest::RequestServerCheck() {
  if (!registrar_.IsRegistered(observer_.get(),
      chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
      content::Source<Profile>(&profile_))) {
    registrar_.Add(observer_.get(), chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
                   content::Source<Profile>(&profile_));
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
  profile_.GetPrefs()->SetString(prefs::kLastPromptedGoogleURL, url.spec());
}

GURL GoogleURLTrackerTest::GetLastPromptedGoogleURL() {
  return GURL(profile_.GetPrefs()->GetString(prefs::kLastPromptedGoogleURL));
}

void GoogleURLTrackerTest::SetSearchPending(const GURL& search_url,
                                            int unique_id) {
  google_url_tracker_->SearchCommitted();
  if (google_url_tracker_->registrar_.IsRegistered(google_url_tracker_.get(),
      content::NOTIFICATION_NAV_ENTRY_PENDING,
      content::NotificationService::AllBrowserContextsAndSources())) {
    google_url_tracker_->OnNavigationPending(
        content::Source<content::NavigationController>(
            reinterpret_cast<content::NavigationController*>(unique_id)),
        content::Source<content::WebContents>(
            reinterpret_cast<content::WebContents*>(unique_id)),
        reinterpret_cast<InfoBarTabHelper*>(unique_id), search_url);
  }
}

void GoogleURLTrackerTest::CommitSearch(int unique_id) {
  content::Source<content::NavigationController> source(
      reinterpret_cast<content::NavigationController*>(unique_id));
  if (google_url_tracker_->registrar_.IsRegistered(google_url_tracker_.get(),
      content::NOTIFICATION_NAV_ENTRY_COMMITTED, source)) {
    google_url_tracker_->OnNavigationCommittedOrTabClosed(source,
        content::Source<content::WebContents>(
            reinterpret_cast<content::WebContents*>(unique_id)),
        reinterpret_cast<InfoBarTabHelper*>(unique_id), true);
  }
}

void GoogleURLTrackerTest::CloseTab(int unique_id) {
  content::Source<content::WebContents> source(
      reinterpret_cast<content::WebContents*>(unique_id));
  InfoBarTabHelper* infobar_helper =
      reinterpret_cast<InfoBarTabHelper*>(unique_id);
  if (google_url_tracker_->registrar_.IsRegistered(google_url_tracker_.get(),
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED, source)) {
    google_url_tracker_->OnNavigationCommittedOrTabClosed(
        content::Source<content::NavigationController>(
            reinterpret_cast<content::NavigationController*>(unique_id)),
        source, infobar_helper, false);
  } else {
    // Normally, closing a tab with an infobar showing will close the infobar.
    // Since we don't have real tabs and are just faking things with magic
    // numbers, we have to manually close the infobar, if any.
    GoogleURLTracker::InfoBarMap::iterator i =
        google_url_tracker_->infobar_map_.find(infobar_helper);
    if (i != google_url_tracker_->infobar_map_.end()) {
      TestInfoBarDelegate* infobar =
          static_cast<TestInfoBarDelegate*>(i->second);
      DCHECK(infobar->showing());
      infobar->InfoBarClosed();
    }
  }
}

TestInfoBarDelegate* GoogleURLTrackerTest::GetInfoBar(int unique_id) {
  GoogleURLTracker::InfoBarMap::const_iterator i =
      google_url_tracker_->infobar_map_.find(
          reinterpret_cast<InfoBarTabHelper*>(unique_id));
  return (i == google_url_tracker_->infobar_map_.end()) ?
      NULL : static_cast<TestInfoBarDelegate*>(i->second);
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
  EXPECT_FALSE(GetFetcher());
  MockSearchDomainCheckResponse(".google.co.uk");
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, UpdateOnFirstRun) {
  RequestServerCheck();
  EXPECT_FALSE(GetFetcher());
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_->notified());

  FinishSleep();
  MockSearchDomainCheckResponse(".google.co.uk");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  // GoogleURL should be updated, becase there was no last prompted URL.
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_TRUE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, DontUpdateWhenUnchanged) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));

  RequestServerCheck();
  EXPECT_FALSE(GetFetcher());
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_->notified());

  FinishSleep();
  MockSearchDomainCheckResponse(".google.co.uk");
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
  MockSearchDomainCheckResponse(".google.co.uk");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, RefetchOnIPAddressChange) {
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(".google.co.uk");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_TRUE(observer_->notified());
  observer_->clear_notified();

  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse(".google.co.in");
  EXPECT_EQ(GURL("http://www.google.co.in/"), fetched_google_url());
  // Just fetching a new URL shouldn't reset things without a prompt.
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, DontRefetchWhenNoOneRequestsCheck) {
  FinishSleep();
  NotifyIPAddressChanged();
  // No one called RequestServerCheck() so nothing should have happened.
  EXPECT_FALSE(GetFetcher());
  MockSearchDomainCheckResponse(".google.co.uk");
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, FetchOnLateRequest) {
  FinishSleep();
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse(".google.co.jp");

  RequestServerCheck();
  // The first request for a check should trigger a fetch if it hasn't happened
  // already.
  MockSearchDomainCheckResponse(".google.co.uk");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_TRUE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, DontFetchTwiceOnLateRequests) {
  FinishSleep();
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse(".google.co.jp");

  RequestServerCheck();
  // The first request for a check should trigger a fetch if it hasn't happened
  // already.
  MockSearchDomainCheckResponse(".google.co.uk");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_TRUE(observer_->notified());
  observer_->clear_notified();

  RequestServerCheck();
  // The second request should be ignored.
  EXPECT_FALSE(GetFetcher());
  MockSearchDomainCheckResponse(".google.co.in");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, SearchingDoesNothingIfNoNeedToPrompt) {
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(".google.co.uk");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(observer_->notified());
  observer_->clear_notified();

  SetSearchPending(GURL("http://www.google.co.uk/search?q=test"), 1);
  CommitSearch(1);
  TestInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_TRUE(infobar == NULL);
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, TabClosedOnPendingSearch) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(".google.co.jp");
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());

  SetSearchPending(GURL("http://www.google.co.uk/search?q=test"), 1);
  TestInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);
  EXPECT_FALSE(infobar->showing());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/search?q=test"),
            infobar->search_url());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), infobar->new_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());

  CloseTab(1);
  EXPECT_TRUE(GetInfoBar(1) == NULL);
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, TabClosedOnCommittedSearch) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(".google.co.jp");

  SetSearchPending(GURL("http://www.google.co.uk/search?q=test"), 1);
  CommitSearch(1);
  TestInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);
  EXPECT_TRUE(infobar->showing());

  CloseTab(1);
  EXPECT_TRUE(GetInfoBar(1) == NULL);
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, InfobarClosed) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(".google.co.jp");

  SetSearchPending(GURL("http://www.google.co.uk/search?q=test"), 1);
  CommitSearch(1);
  TestInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);

  infobar->InfoBarClosed();
  EXPECT_TRUE(GetInfoBar(1) == NULL);
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, InfobarRefused) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(".google.co.jp");

  SetSearchPending(GURL("http://www.google.co.uk/search?q=test"), 1);
  CommitSearch(1);
  TestInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);

  infobar->Cancel();
  EXPECT_TRUE(GetInfoBar(1) == NULL);
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, InfobarAccepted) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(".google.co.jp");

  SetSearchPending(GURL("http://www.google.co.uk/search?q=test"), 1);
  CommitSearch(1);
  TestInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);

  infobar->Accept();
  EXPECT_TRUE(GetInfoBar(1) == NULL);
  EXPECT_EQ(GURL("http://www.google.co.jp/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, InfobarShownAgainOnSearchAfterPendingSearch) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(".google.co.jp");

  SetSearchPending(GURL("http://www.google.co.uk/search?q=test"), 1);
  TestInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);
  EXPECT_FALSE(infobar->showing());
  EXPECT_EQ(GURL("http://www.google.co.uk/search?q=test"),
            infobar->search_url());

  SetSearchPending(GURL("http://www.google.co.uk/search?q=test2"), 1);
  TestInfoBarDelegate* infobar2 = GetInfoBar(1);
  ASSERT_FALSE(infobar2 == NULL);
  EXPECT_FALSE(infobar2->showing());
  EXPECT_EQ(GURL("http://www.google.co.uk/search?q=test2"),
            infobar2->search_url());

  CommitSearch(1);
  EXPECT_TRUE(infobar2->showing());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());

  CloseTab(1);
}

TEST_F(GoogleURLTrackerTest, InfobarShownAgainOnSearchAfterCommittedSearch) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(".google.co.jp");

  SetSearchPending(GURL("http://www.google.co.uk/search?q=test"), 1);
  CommitSearch(1);
  TestInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);
  EXPECT_TRUE(infobar->showing());

  // In real usage, the upcoming pending navigation for "test2" would close
  // |infobar|.  Since we're not actually doing navigations, we need to clean it
  // up manually to prevent leaks.
  delete infobar;

  SetSearchPending(GURL("http://www.google.co.uk/search?q=test2"), 1);
  TestInfoBarDelegate* infobar2 = GetInfoBar(1);
  ASSERT_FALSE(infobar2 == NULL);
  EXPECT_FALSE(infobar2->showing());

  CommitSearch(1);
  EXPECT_TRUE(infobar2->showing());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());

  CloseTab(1);
}

TEST_F(GoogleURLTrackerTest, MultipleInfobars) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(".google.co.jp");

  SetSearchPending(GURL("http://www.google.co.uk/search?q=test"), 1);
  TestInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);
  EXPECT_FALSE(infobar->showing());
  EXPECT_EQ(GURL("http://www.google.co.uk/search?q=test"),
            infobar->search_url());

  SetSearchPending(GURL("http://www.google.co.uk/search?q=test2"), 2);
  CommitSearch(2);
  TestInfoBarDelegate* infobar2 = GetInfoBar(2);
  ASSERT_FALSE(infobar2 == NULL);
  EXPECT_TRUE(infobar2->showing());
  EXPECT_EQ(GURL("http://www.google.co.uk/search?q=test2"),
            infobar2->search_url());

  SetSearchPending(GURL("http://www.google.co.uk/search?q=test3"), 3);
  TestInfoBarDelegate* infobar3 = GetInfoBar(3);
  ASSERT_FALSE(infobar3 == NULL);
  EXPECT_FALSE(infobar3->showing());
  EXPECT_EQ(GURL("http://www.google.co.uk/search?q=test3"),
            infobar3->search_url());

  SetSearchPending(GURL("http://www.google.co.uk/search?q=test4"), 4);
  CommitSearch(4);
  TestInfoBarDelegate* infobar4 = GetInfoBar(4);
  ASSERT_FALSE(infobar4 == NULL);
  EXPECT_TRUE(infobar4->showing());
  EXPECT_EQ(GURL("http://www.google.co.uk/search?q=test4"),
            infobar4->search_url());

  CommitSearch(1);
  EXPECT_TRUE(infobar->showing());

  infobar2->InfoBarClosed();
  EXPECT_TRUE(GetInfoBar(2) == NULL);
  EXPECT_FALSE(observer_->notified());

  infobar4->Accept();
  EXPECT_TRUE(GetInfoBar(1) == NULL);
  EXPECT_TRUE(GetInfoBar(3) == NULL);
  EXPECT_TRUE(GetInfoBar(4) == NULL);
  EXPECT_EQ(GURL("http://www.google.co.jp/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(observer_->notified());
}
