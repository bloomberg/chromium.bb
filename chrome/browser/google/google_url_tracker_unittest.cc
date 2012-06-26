// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_url_tracker.h"

#include <set>
#include <string>

#include "base/message_loop.h"
#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/infobars/infobar_delegate.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabContents;

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
                      GoogleURLTracker* google_url_tracker,
                      const GURL& new_google_url);
  virtual ~TestInfoBarDelegate();

  GURL search_url() const { return search_url_; }
  GURL new_google_url() const { return new_google_url_; }
  int pending_id() const { return pending_id_; }

 private:
  // GoogleURLTrackerInfoBarDelegate:
  virtual void Show(const GURL& search_url) OVERRIDE;
  virtual void Close(bool redo_search) OVERRIDE;
};

TestInfoBarDelegate::TestInfoBarDelegate(InfoBarTabHelper* infobar_helper,
                                         GoogleURLTracker* google_url_tracker,
                                         const GURL& new_google_url)
  : GoogleURLTrackerInfoBarDelegate(NULL, google_url_tracker, new_google_url) {
  // We set |map_key_| here instead of in the superclass constructor so that the
  // InfoBarDelegate base class will not try to dereference it, which would fail
  // since this is really a magic number and not an actual pointer.
  map_key_ = infobar_helper;
}

void TestInfoBarDelegate::Show(const GURL& search_url) {
  search_url_ = search_url;
  pending_id_ = 0;
  showing_ = true;
}

void TestInfoBarDelegate::Close(bool redo_search) {
  InfoBarClosed();
}

TestInfoBarDelegate::~TestInfoBarDelegate() {
}

GoogleURLTrackerInfoBarDelegate* CreateTestInfobar(
    InfoBarTabHelper* infobar_helper,
    GoogleURLTracker* google_url_tracker,
    const GURL& new_google_url) {
  return new TestInfoBarDelegate(infobar_helper, google_url_tracker,
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

  net::TestURLFetcher* GetFetcher();
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
  void SetNonSearchPending(int unique_id);
  void SetSearchPending(int unique_id);
  void CommitNonSearch(int unique_id);
  void CommitSearch(const GURL& search_url, int unique_id);
  void CloseTab(int unique_id);
  TestInfoBarDelegate* GetInfoBar(int unique_id);
  void ExpectDefaultURLs() const;
  void ExpectListeningForCommit(int unique_id, bool listening) const;

  scoped_ptr<TestNotificationObserver> observer_;

 private:
  // These are required by the TestURLFetchers GoogleURLTracker will create (see
  // test_url_fetcher_factory.h).
  MessageLoop message_loop_;
  content::TestBrowserThread io_thread_;
  // Creating this allows us to call
  // net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests().
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  net::TestURLFetcherFactory fetcher_factory_;
  content::NotificationRegistrar registrar_;
  TestingProfile profile_;
  scoped_ptr<GoogleURLTracker> google_url_tracker_;
  // This tracks the different "tabs" a test has "opened", so we can close them
  // properly before shutting down |google_url_tracker_|, which expects that.
  std::set<int> unique_ids_seen_;
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
  while (!unique_ids_seen_.empty())
    CloseTab(*unique_ids_seen_.begin());

  google_url_tracker_.reset();
  network_change_notifier_.reset();
}

net::TestURLFetcher* GoogleURLTrackerTest::GetFetcher() {
  // This will return the last fetcher created.  If no fetchers have been
  // created, we'll pass GetFetcherByID() "-1", and it will return NULL.
  return fetcher_factory_.GetFetcherByID(google_url_tracker_->fetcher_id_ - 1);
}

void GoogleURLTrackerTest::MockSearchDomainCheckResponse(
    const std::string& domain) {
  net::TestURLFetcher* fetcher = GetFetcher();
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

void GoogleURLTrackerTest::SetNonSearchPending(int unique_id) {
  unique_ids_seen_.insert(unique_id);
  if (google_url_tracker_->registrar_.IsRegistered(google_url_tracker_.get(),
      content::NOTIFICATION_NAV_ENTRY_PENDING,
      content::NotificationService::AllBrowserContextsAndSources())) {
    google_url_tracker_->OnNavigationPending(
        content::Source<content::NavigationController>(
            reinterpret_cast<content::NavigationController*>(unique_id)),
        content::Source<TabContents>(reinterpret_cast<TabContents*>(unique_id)),
        reinterpret_cast<InfoBarTabHelper*>(unique_id), unique_id);
  }
}

void GoogleURLTrackerTest::SetSearchPending(int unique_id) {
  google_url_tracker_->SearchCommitted();
  // Note that the call above might not have actually registered a listener for
  // NOTIFICATION_NAV_ENTRY_PENDING if the searchdomaincheck response was bogus.
  SetNonSearchPending(unique_id);
}

void GoogleURLTrackerTest::CommitNonSearch(int unique_id) {
  GoogleURLTracker::InfoBarMap::iterator i =
      google_url_tracker_->infobar_map_.find(
          reinterpret_cast<InfoBarTabHelper*>(unique_id));
  if (i != google_url_tracker_->infobar_map_.end()) {
    ExpectListeningForCommit(unique_id, false);
    TestInfoBarDelegate* infobar =
        static_cast<TestInfoBarDelegate*>(i->second.infobar);
    // The infobar should be showing; otherwise the pending non-search should
    // have closed it.
    EXPECT_TRUE(infobar->showing());
    // The pending_id should have been reset to 0 when the non-search became
    // pending.
    EXPECT_EQ(0, infobar->pending_id());
    infobar->InfoBarClosed();
  }
}

void GoogleURLTrackerTest::CommitSearch(const GURL& search_url, int unique_id) {
  if (google_url_tracker_->registrar_.IsRegistered(google_url_tracker_.get(),
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<content::NavigationController>(
          reinterpret_cast<content::NavigationController*>(unique_id)))) {
    google_url_tracker_->OnNavigationCommittedOrTabClosed(
        reinterpret_cast<InfoBarTabHelper*>(unique_id),
        search_url);
  }
}

void GoogleURLTrackerTest::CloseTab(int unique_id) {
  unique_ids_seen_.erase(unique_id);
  InfoBarTabHelper* infobar_helper =
      reinterpret_cast<InfoBarTabHelper*>(unique_id);
  if (google_url_tracker_->registrar_.IsRegistered(google_url_tracker_.get(),
      chrome::NOTIFICATION_TAB_CONTENTS_DESTROYED, content::Source<TabContents>(
          reinterpret_cast<TabContents*>(unique_id)))) {
    google_url_tracker_->OnNavigationCommittedOrTabClosed(infobar_helper,
                                                          GURL());
  } else {
    // Normally, closing a tab with an infobar showing will close the infobar.
    // Since we don't have real tabs and are just faking things with magic
    // numbers, we have to manually close the infobar, if any.
    GoogleURLTracker::InfoBarMap::iterator i =
        google_url_tracker_->infobar_map_.find(infobar_helper);
    if (i != google_url_tracker_->infobar_map_.end()) {
      TestInfoBarDelegate* infobar =
          static_cast<TestInfoBarDelegate*>(i->second.infobar);
      EXPECT_TRUE(infobar->showing());
      infobar->InfoBarClosed();
    }
  }
}

TestInfoBarDelegate* GoogleURLTrackerTest::GetInfoBar(int unique_id) {
  GoogleURLTracker::InfoBarMap::const_iterator i =
      google_url_tracker_->infobar_map_.find(
          reinterpret_cast<InfoBarTabHelper*>(unique_id));
  return (i == google_url_tracker_->infobar_map_.end()) ?
      NULL : static_cast<TestInfoBarDelegate*>(i->second.infobar);
}

void GoogleURLTrackerTest::ExpectDefaultURLs() const {
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL(), fetched_google_url());
}

void GoogleURLTrackerTest::ExpectListeningForCommit(int unique_id,
                                                    bool listening) const {
  GoogleURLTracker::InfoBarMap::iterator i =
      google_url_tracker_->infobar_map_.find(
          reinterpret_cast<InfoBarTabHelper*>(unique_id));
  ASSERT_FALSE(i == google_url_tracker_->infobar_map_.end());
  EXPECT_EQ(listening, google_url_tracker_->registrar_.IsRegistered(
      google_url_tracker_.get(), content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      i->second.navigation_controller_source));
}


// Tests ----------------------------------------------------------------------

TEST_F(GoogleURLTrackerTest, DontFetchWhenNoOneRequestsCheck) {
  ExpectDefaultURLs();
  FinishSleep();
  // No one called RequestServerCheck() so nothing should have happened.
  EXPECT_FALSE(GetFetcher());
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, UpdateOnFirstRun) {
  RequestServerCheck();
  EXPECT_FALSE(GetFetcher());
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_->notified());

  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
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
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  // GoogleURL should not be updated, because the fetched and prompted URLs
  // match.
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, DontPromptOnBadReplies) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));

  RequestServerCheck();
  EXPECT_FALSE(GetFetcher());
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_->notified());

  // Old-style domain string.
  FinishSleep();
  MockSearchDomainCheckResponse(".google.co.in");
  EXPECT_EQ(GURL(), fetched_google_url());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(observer_->notified());
  SetSearchPending(1);
  CommitSearch(GURL("http://www.google.co.uk/search?q=test"), 1);
  EXPECT_TRUE(GetInfoBar(1) == NULL);

  // Bad subdomain.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://mail.google.com/");
  EXPECT_EQ(GURL(), fetched_google_url());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(observer_->notified());
  SetSearchPending(1);
  CommitSearch(GURL("http://www.google.co.uk/search?q=test"), 1);
  EXPECT_TRUE(GetInfoBar(1) == NULL);

  // Non-empty path.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.com/search");
  EXPECT_EQ(GURL(), fetched_google_url());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(observer_->notified());
  SetSearchPending(1);
  CommitSearch(GURL("http://www.google.co.uk/search?q=test"), 1);
  EXPECT_TRUE(GetInfoBar(1) == NULL);

  // Non-empty query.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.com/?q=foo");
  EXPECT_EQ(GURL(), fetched_google_url());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(observer_->notified());
  SetSearchPending(1);
  CommitSearch(GURL("http://www.google.co.uk/search?q=test"), 1);
  EXPECT_TRUE(GetInfoBar(1) == NULL);

  // Non-empty ref.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.com/#anchor");
  EXPECT_EQ(GURL(), fetched_google_url());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(observer_->notified());
  SetSearchPending(1);
  CommitSearch(GURL("http://www.google.co.uk/search?q=test"), 1);
  EXPECT_TRUE(GetInfoBar(1) == NULL);

  // Complete garbage.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("HJ)*qF)_*&@f1");
  EXPECT_EQ(GURL(), fetched_google_url());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(observer_->notified());
  SetSearchPending(1);
  CommitSearch(GURL("http://www.google.co.uk/search?q=test"), 1);
  EXPECT_TRUE(GetInfoBar(1) == NULL);
}

TEST_F(GoogleURLTrackerTest, UpdatePromptedURLOnReturnToPreviousLocation) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.jp/"));
  set_google_url(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, SilentlyAcceptSchemeChange) {
  // We should auto-accept changes to the current Google URL that merely change
  // the scheme, regardless of what the last prompted URL was.
  SetLastPromptedGoogleURL(GURL("http://www.google.co.jp/"));
  set_google_url(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("https://www.google.co.uk/");
  EXPECT_EQ(GURL("https://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("https://www.google.co.uk/"), google_url());
  EXPECT_EQ(GURL("https://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(observer_->notified());

  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, RefetchOnIPAddressChange) {
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_TRUE(observer_->notified());
  observer_->clear_notified();

  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.in/");
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
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, FetchOnLateRequest) {
  FinishSleep();
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  RequestServerCheck();
  // The first request for a check should trigger a fetch if it hasn't happened
  // already.
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_TRUE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, DontFetchTwiceOnLateRequests) {
  FinishSleep();
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  RequestServerCheck();
  // The first request for a check should trigger a fetch if it hasn't happened
  // already.
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_TRUE(observer_->notified());
  observer_->clear_notified();

  RequestServerCheck();
  // The second request should be ignored.
  EXPECT_FALSE(GetFetcher());
  MockSearchDomainCheckResponse("http://www.google.co.in/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, SearchingDoesNothingIfNoNeedToPrompt) {
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(observer_->notified());
  observer_->clear_notified();

  SetSearchPending(1);
  CommitSearch(GURL("http://www.google.co.uk/search?q=test"), 1);
  TestInfoBarDelegate* infobar = GetInfoBar(1);
  EXPECT_TRUE(infobar == NULL);
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, TabClosedOnPendingSearch) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());

  SetSearchPending(1);
  TestInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);
  EXPECT_FALSE(infobar->showing());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
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
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  SetSearchPending(1);
  CommitSearch(GURL("http://www.google.co.uk/search?q=test"), 1);
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
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  SetSearchPending(1);
  CommitSearch(GURL("http://www.google.co.uk/search?q=test"), 1);
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
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  SetSearchPending(1);
  CommitSearch(GURL("http://www.google.co.uk/search?q=test"), 1);
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
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  SetSearchPending(1);
  CommitSearch(GURL("http://www.google.co.uk/search?q=test"), 1);
  TestInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);

  infobar->Accept();
  EXPECT_TRUE(GetInfoBar(1) == NULL);
  EXPECT_EQ(GURL("http://www.google.co.jp/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, FetchesCanAutomaticallyCloseInfobars) {
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(google_url().spec());

  // Re-fetching the accepted URL after showing an infobar for another URL
  // should close the infobar.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  SetSearchPending(1);
  CommitSearch(GURL("http://www.google.com/search?q=test"), 1);
  EXPECT_FALSE(GetInfoBar(1) == NULL);
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse(google_url().spec());
  EXPECT_EQ(google_url(), GetLastPromptedGoogleURL());
  EXPECT_TRUE(GetInfoBar(1) == NULL);

  // As should fetching a URL that differs from the accepted only by the scheme.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  SetSearchPending(1);
  CommitSearch(GURL("http://www.google.com/search?q=test"), 1);
  EXPECT_FALSE(GetInfoBar(1) == NULL);
  NotifyIPAddressChanged();
  url_canon::Replacements<char> replacements;
  const std::string& scheme("https");
  replacements.SetScheme(scheme.data(),
                         url_parse::Component(0, scheme.length()));
  GURL new_google_url(google_url().ReplaceComponents(replacements));
  MockSearchDomainCheckResponse(new_google_url.spec());
  EXPECT_EQ(new_google_url, GetLastPromptedGoogleURL());
  EXPECT_TRUE(GetInfoBar(1) == NULL);

  // As should re-fetching the last prompted URL.
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");
  SetSearchPending(1);
  CommitSearch(GURL("http://www.google.com/search?q=test"), 1);
  EXPECT_FALSE(GetInfoBar(1) == NULL);
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(new_google_url, google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(GetInfoBar(1) == NULL);

  // And one that differs from the last prompted URL only by the scheme.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");
  SetSearchPending(1);
  CommitSearch(GURL("http://www.google.com/search?q=test"), 1);
  EXPECT_FALSE(GetInfoBar(1) == NULL);
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("https://www.google.co.uk/");
  EXPECT_EQ(new_google_url, google_url());
  EXPECT_EQ(GURL("https://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(GetInfoBar(1) == NULL);

  // And fetching a different URL entirely.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");
  SetSearchPending(1);
  CommitSearch(GURL("http://www.google.com/search?q=test"), 1);
  EXPECT_FALSE(GetInfoBar(1) == NULL);
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("https://www.google.co.in/");
  EXPECT_EQ(new_google_url, google_url());
  EXPECT_EQ(GURL("https://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(GetInfoBar(1) == NULL);
}

TEST_F(GoogleURLTrackerTest, ResetInfobarGoogleURLs) {
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(google_url().spec());

  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  SetSearchPending(1);
  TestInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);
  EXPECT_EQ(GURL("http://www.google.co.uk/"), infobar->new_google_url());

  // If while an infobar is pending we fetch a new URL that differs from the
  // infobar's only by scheme, the infobar should stay pending but have its
  // Google URL reset.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("https://www.google.co.uk/");
  TestInfoBarDelegate* new_infobar = GetInfoBar(1);
  ASSERT_FALSE(new_infobar == NULL);
  EXPECT_EQ(infobar, new_infobar);
  EXPECT_EQ(GURL("https://www.google.co.uk/"), new_infobar->new_google_url());

  // Same with an infobar that is showing.
  CommitSearch(GURL("http://www.google.com/search?q=test"), 1);
  EXPECT_TRUE(infobar->showing());
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  new_infobar = GetInfoBar(1);
  ASSERT_FALSE(new_infobar == NULL);
  EXPECT_EQ(infobar, new_infobar);
  EXPECT_EQ(GURL("http://www.google.co.uk/"), infobar->new_google_url());
  EXPECT_TRUE(infobar->showing());
}

TEST_F(GoogleURLTrackerTest, NavigationsAfterPendingSearch) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  // A pending non-search after a pending search should close the infobar.
  SetSearchPending(1);
  TestInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);
  EXPECT_FALSE(infobar->showing());
  SetNonSearchPending(1);
  infobar = GetInfoBar(1);
  EXPECT_TRUE(infobar == NULL);

  // A pending search after a pending search should leave the infobar alive.
  SetSearchPending(1);
  infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);
  EXPECT_FALSE(infobar->showing());
  SetSearchPending(1);
  TestInfoBarDelegate* new_infobar = GetInfoBar(1);
  ASSERT_FALSE(new_infobar == NULL);
  EXPECT_EQ(infobar, new_infobar);
  EXPECT_FALSE(infobar->showing());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, true));

  // Committing this search should show the infobar.
  CommitSearch(GURL("http://www.google.co.uk/search?q=test2"), 1);
  EXPECT_TRUE(infobar->showing());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, false));
}

TEST_F(GoogleURLTrackerTest, NavigationsAfterCommittedSearch) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");
  SetSearchPending(1);
  CommitSearch(GURL("http://www.google.co.uk/search?q=test"), 1);
  TestInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);
  EXPECT_TRUE(infobar->showing());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, false));

  // A pending non-search on a visible infobar should basically do nothing.
  SetNonSearchPending(1);
  TestInfoBarDelegate* new_infobar = GetInfoBar(1);
  ASSERT_FALSE(new_infobar == NULL);
  EXPECT_EQ(infobar, new_infobar);
  EXPECT_TRUE(infobar->showing());
  EXPECT_EQ(0, infobar->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, false));

  // As should another pending non-search after the first.
  SetNonSearchPending(1);
  new_infobar = GetInfoBar(1);
  ASSERT_FALSE(new_infobar == NULL);
  EXPECT_EQ(infobar, new_infobar);
  EXPECT_TRUE(infobar->showing());
  EXPECT_EQ(0, infobar->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, false));

  // Committing this non-search should close the infobar.  The control flow in
  // these tests is not really comparable to in the real browser, but at least a
  // few sanity-checks will be performed.
  ASSERT_NO_FATAL_FAILURE(CommitNonSearch(1));
  new_infobar = GetInfoBar(1);
  EXPECT_TRUE(new_infobar == NULL);

  // A pending search on a visible infobar should cause the infobar to listen
  // for the search to commit.
  SetSearchPending(1);
  CommitSearch(GURL("http://www.google.co.uk/search?q=test"), 1);
  infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);
  EXPECT_TRUE(infobar->showing());
  SetSearchPending(1);
  new_infobar = GetInfoBar(1);
  ASSERT_FALSE(new_infobar == NULL);
  EXPECT_EQ(infobar, new_infobar);
  EXPECT_TRUE(infobar->showing());
  EXPECT_EQ(1, infobar->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, true));

  // But a non-search after this should cancel that state.
  SetNonSearchPending(1);
  new_infobar = GetInfoBar(1);
  ASSERT_FALSE(new_infobar == NULL);
  EXPECT_EQ(infobar, new_infobar);
  EXPECT_TRUE(infobar->showing());
  EXPECT_EQ(0, infobar->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, false));

  // Another pending search after the non-search should put us back into
  // "waiting for commit" mode.
  SetSearchPending(1);
  new_infobar = GetInfoBar(1);
  ASSERT_FALSE(new_infobar == NULL);
  EXPECT_EQ(infobar, new_infobar);
  EXPECT_TRUE(infobar->showing());
  EXPECT_EQ(1, infobar->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, true));

  // A second pending search after the first should not really change anything.
  SetSearchPending(1);
  new_infobar = GetInfoBar(1);
  ASSERT_FALSE(new_infobar == NULL);
  EXPECT_EQ(infobar, new_infobar);
  EXPECT_TRUE(infobar->showing());
  EXPECT_EQ(1, infobar->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, true));

  // Committing this search should change the visible infobar's search_url.
  CommitSearch(GURL("http://www.google.co.uk/search?q=test2"), 1);
  new_infobar = GetInfoBar(1);
  ASSERT_FALSE(new_infobar == NULL);
  EXPECT_EQ(infobar, new_infobar);
  EXPECT_TRUE(infobar->showing());
  EXPECT_EQ(GURL("http://www.google.co.uk/search?q=test2"), infobar->search_url());
  EXPECT_EQ(0, infobar->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, false));
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_->notified());
}

TEST_F(GoogleURLTrackerTest, MultipleInfobars) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  SetSearchPending(1);
  TestInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);
  EXPECT_FALSE(infobar->showing());

  SetSearchPending(2);
  CommitSearch(GURL("http://www.google.co.uk/search?q=test2"), 2);
  TestInfoBarDelegate* infobar2 = GetInfoBar(2);
  ASSERT_FALSE(infobar2 == NULL);
  EXPECT_TRUE(infobar2->showing());
  EXPECT_EQ(GURL("http://www.google.co.uk/search?q=test2"),
            infobar2->search_url());

  SetSearchPending(3);
  TestInfoBarDelegate* infobar3 = GetInfoBar(3);
  ASSERT_FALSE(infobar3 == NULL);
  EXPECT_FALSE(infobar3->showing());

  SetSearchPending(4);
  CommitSearch(GURL("http://www.google.co.uk/search?q=test4"), 4);
  TestInfoBarDelegate* infobar4 = GetInfoBar(4);
  ASSERT_FALSE(infobar4 == NULL);
  EXPECT_TRUE(infobar4->showing());
  EXPECT_EQ(GURL("http://www.google.co.uk/search?q=test4"),
            infobar4->search_url());

  CommitSearch(GURL("http://www.google.co.uk/search?q=test"), 1);
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
