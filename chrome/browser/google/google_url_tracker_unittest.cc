// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_url_tracker.h"

#include <set>
#include <string>

#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/api/infobars/infobar_delegate.h"
#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/google/google_url_tracker_infobar_delegate.h"
#include "chrome/browser/google/google_url_tracker_navigation_helper.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "testing/gtest/include/gtest/gtest.h"

class GoogleURLTrackerTest;


namespace {

// TestInfoBarDelegate --------------------------------------------------------

class TestInfoBarDelegate : public GoogleURLTrackerInfoBarDelegate {
 public:
  // Creates a test delegate and returns it.  Unlike the parent class, this does
  // not create add the infobar to |infobar_service|, since that "pointer" is
  // really just a magic number.  Thus there is no InfoBarService ownership of
  // the returned object; and since the caller doesn't own the returned object,
  // we rely on |test_harness| cleaning this up eventually in
  // GoogleURLTrackerTest::OnInfoBarClosed() to avoid leaks.
  static GoogleURLTrackerInfoBarDelegate* Create(
      GoogleURLTrackerTest* test_harness,
      InfoBarService* infobar_service,
      GoogleURLTracker* google_url_tracker,
      const GURL& search_url);

 private:
  TestInfoBarDelegate(GoogleURLTrackerTest* test_harness,
                      InfoBarService* infobar_service,
                      GoogleURLTracker* google_url_tracker,
                      const GURL& search_url);
  virtual ~TestInfoBarDelegate();

  // GoogleURLTrackerInfoBarDelegate:
  virtual void Update(const GURL& search_url) OVERRIDE;
  virtual void Close(bool redo_search) OVERRIDE;

  GoogleURLTrackerTest* test_harness_;
  InfoBarService* infobar_service_;

  DISALLOW_COPY_AND_ASSIGN(TestInfoBarDelegate);
};

// The member function definitions come after the declaration of
// GoogleURLTrackerTest, so they can call members on it.


// TestNotificationObserver ---------------------------------------------------

class TestNotificationObserver : public content::NotificationObserver {
 public:
  TestNotificationObserver();
  virtual ~TestNotificationObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
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


// TestGoogleURLTrackerNavigationHelper -------------------------------------

class TestGoogleURLTrackerNavigationHelper
    : public GoogleURLTrackerNavigationHelper {
 public:
  TestGoogleURLTrackerNavigationHelper();
  virtual ~TestGoogleURLTrackerNavigationHelper();

  virtual void SetGoogleURLTracker(GoogleURLTracker* tracker) OVERRIDE;
  virtual void SetListeningForNavigationStart(bool listen) OVERRIDE;
  virtual bool IsListeningForNavigationStart() OVERRIDE;
  virtual void SetListeningForNavigationCommit(
      const content::NavigationController* nav_controller,
      bool listen) OVERRIDE;
  virtual bool IsListeningForNavigationCommit(
      const content::NavigationController* nav_controller) OVERRIDE;
  virtual void SetListeningForTabDestruction(
      const content::NavigationController* nav_controller,
      bool listen) OVERRIDE;
  virtual bool IsListeningForTabDestruction(
      const content::NavigationController* nav_controller) OVERRIDE;

 private:
  GoogleURLTracker* tracker_;
  bool observe_nav_start_;
  std::set<const content::NavigationController*>
      nav_controller_commit_listeners_;
  std::set<const content::NavigationController*>
      nav_controller_tab_close_listeners_;
};

TestGoogleURLTrackerNavigationHelper::TestGoogleURLTrackerNavigationHelper()
    : tracker_(NULL),
      observe_nav_start_(false) {
}

TestGoogleURLTrackerNavigationHelper::
    ~TestGoogleURLTrackerNavigationHelper() {
}

void TestGoogleURLTrackerNavigationHelper::SetGoogleURLTracker(
    GoogleURLTracker* tracker) {
  tracker_ = tracker;
}

void TestGoogleURLTrackerNavigationHelper::SetListeningForNavigationStart(
    bool listen) {
  observe_nav_start_ = listen;
}

bool TestGoogleURLTrackerNavigationHelper::IsListeningForNavigationStart() {
  return observe_nav_start_;
}

void TestGoogleURLTrackerNavigationHelper::SetListeningForNavigationCommit(
    const content::NavigationController* nav_controller,
    bool listen) {
  if (listen)
    nav_controller_commit_listeners_.insert(nav_controller);
  else
    nav_controller_commit_listeners_.erase(nav_controller);
}

bool TestGoogleURLTrackerNavigationHelper::IsListeningForNavigationCommit(
    const content::NavigationController* nav_controller) {
  return nav_controller_commit_listeners_.count(nav_controller) > 0;
}

void TestGoogleURLTrackerNavigationHelper::SetListeningForTabDestruction(
    const content::NavigationController* nav_controller,
    bool listen) {
  if (listen)
    nav_controller_tab_close_listeners_.insert(nav_controller);
  else
    nav_controller_tab_close_listeners_.erase(nav_controller);
}

bool TestGoogleURLTrackerNavigationHelper::IsListeningForTabDestruction(
    const content::NavigationController* nav_controller) {
  return nav_controller_tab_close_listeners_.count(nav_controller) > 0;
}

}  // namespace


// GoogleURLTrackerTest -------------------------------------------------------

// Ths class exercises GoogleURLTracker.  In order to avoid instantiating more
// of the Chrome infrastructure than necessary, the GoogleURLTracker functions
// are carefully written so that many of the functions which take
// NavigationController* or InfoBarService* do not actually dereference the
// objects, merely use them for comparisons and lookups, e.g. in |entry_map_|.
// This then allows the test code here to not create any of these objects, and
// instead supply "pointers" that are actually reinterpret_cast<>()ed magic
// numbers.  Then we write the necessary stubs/hooks, here and in
// TestInfoBarDelegate above, to make everything continue to work.
//
// Technically, the C++98 spec defines the result of casting
// T* -> intptr_t -> T* to be an identity, but intptr_t -> T* -> intptr_t (what
// we use here) is "implementation-defined".  Since I've never seen a compiler
// break this, though, and the result would simply be a failing test rather than
// a bug in Chrome, we'll use it anyway.
class GoogleURLTrackerTest : public testing::Test {
 public:
  // Called by TestInfoBarDelegate::Close().
  void OnInfoBarClosed(InfoBarDelegate* infobar,
                       InfoBarService* infobar_service);

 protected:
  GoogleURLTrackerTest();
  virtual ~GoogleURLTrackerTest();

  // testing::Test
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  net::TestURLFetcher* GetFetcher();
  void MockSearchDomainCheckResponse(const std::string& domain);
  void RequestServerCheck();
  void FinishSleep();
  void NotifyIPAddressChanged();
  GURL fetched_google_url() const {
    return google_url_tracker_->fetched_google_url();
  }
  void set_google_url(const GURL& url) {
    google_url_tracker_->google_url_ = url;
  }
  GURL google_url() const { return google_url_tracker_->google_url(); }
  void SetLastPromptedGoogleURL(const GURL& url);
  GURL GetLastPromptedGoogleURL();
  void SetNavigationPending(intptr_t unique_id, bool is_search);
  void CommitNonSearch(intptr_t unique_id);
  void CommitSearch(intptr_t unique_id, const GURL& search_url);
  void CloseTab(intptr_t unique_id);
  GoogleURLTrackerMapEntry* GetMapEntry(intptr_t unique_id);
  GoogleURLTrackerInfoBarDelegate* GetInfoBar(intptr_t unique_id);
  void ExpectDefaultURLs() const;
  void ExpectListeningForCommit(intptr_t unique_id, bool listening);
  bool observer_notified() const { return observer_.notified(); }
  void clear_observer_notified() { observer_.clear_notified(); }

 private:
  // Since |infobar_service| is really a magic number rather than an actual
  // object, we don't add the created infobar to it.  Instead we will simulate
  // any helper<->infobar interaction necessary.  The returned object will be
  // cleaned up in CloseTab().
  GoogleURLTrackerInfoBarDelegate* CreateTestInfoBar(
      InfoBarService* infobar_service,
      GoogleURLTracker* google_url_tracker,
      const GURL& search_url);

  // These are required by the TestURLFetchers GoogleURLTracker will create (see
  // test_url_fetcher_factory.h).
  MessageLoop message_loop_;
  content::TestBrowserThread io_thread_;
  // Creating this allows us to call
  // net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests().
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  net::TestURLFetcherFactory fetcher_factory_;
  content::NotificationRegistrar registrar_;
  TestNotificationObserver observer_;
  GoogleURLTrackerNavigationHelper* nav_helper_;
  TestingProfile profile_;
  scoped_ptr<GoogleURLTracker> google_url_tracker_;
  // This tracks the different "tabs" a test has "opened", so we can close them
  // properly before shutting down |google_url_tracker_|, which expects that.
  std::set<int> unique_ids_seen_;
};

void GoogleURLTrackerTest::OnInfoBarClosed(InfoBarDelegate* infobar,
                                           InfoBarService* infobar_service) {
  // First, simulate the InfoBarService firing INFOBAR_REMOVED.
  InfoBarRemovedDetails removed_details(infobar, false);
  GoogleURLTracker::EntryMap::const_iterator i =
      google_url_tracker_->entry_map_.find(infobar_service);
  ASSERT_FALSE(i == google_url_tracker_->entry_map_.end());
  GoogleURLTrackerMapEntry* map_entry = i->second;
  ASSERT_EQ(infobar, map_entry->infobar());
  map_entry->Observe(chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                     content::Source<InfoBarService>(infobar_service),
                     content::Details<InfoBarRemovedDetails>(&removed_details));

  // Second, simulate the infobar container closing the infobar in response.
  delete infobar;
}

GoogleURLTrackerTest::GoogleURLTrackerTest()
    : message_loop_(MessageLoop::TYPE_IO),
      io_thread_(content::BrowserThread::IO, &message_loop_) {
  GoogleURLTrackerFactory::GetInstance()->RegisterUserPrefsOnProfile(&profile_);
}

GoogleURLTrackerTest::~GoogleURLTrackerTest() {
}

void GoogleURLTrackerTest::SetUp() {
  network_change_notifier_.reset(net::NetworkChangeNotifier::CreateMock());
  // Ownership is passed to google_url_tracker_, but a weak pointer is kept;
  // this is safe since GoogleURLTracker keeps the observer for its lifetime.
  nav_helper_ = new TestGoogleURLTrackerNavigationHelper();
  scoped_ptr<GoogleURLTrackerNavigationHelper> nav_helper(nav_helper_);
  google_url_tracker_.reset(
      new GoogleURLTracker(&profile_, nav_helper.Pass(),
                           GoogleURLTracker::UNIT_TEST_MODE));
  google_url_tracker_->infobar_creator_ = base::Bind(
      &GoogleURLTrackerTest::CreateTestInfoBar, base::Unretained(this));
}

void GoogleURLTrackerTest::TearDown() {
  while (!unique_ids_seen_.empty())
    CloseTab(*unique_ids_seen_.begin());

  nav_helper_ = NULL;
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
  if (!registrar_.IsRegistered(&observer_,
                               chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
                               content::Source<Profile>(&profile_))) {
    registrar_.Add(&observer_, chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
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
  MessageLoop::current()->RunUntilIdle();
}

void GoogleURLTrackerTest::SetLastPromptedGoogleURL(const GURL& url) {
  profile_.GetPrefs()->SetString(prefs::kLastPromptedGoogleURL, url.spec());
}

GURL GoogleURLTrackerTest::GetLastPromptedGoogleURL() {
  return GURL(profile_.GetPrefs()->GetString(prefs::kLastPromptedGoogleURL));
}

void GoogleURLTrackerTest::SetNavigationPending(intptr_t unique_id,
                                                bool is_search) {
  if (is_search) {
    google_url_tracker_->SearchCommitted();
    // Note that the call above might not have actually registered a listener
    // for navigation starts if the searchdomaincheck response was bogus.
  }
  unique_ids_seen_.insert(unique_id);
  if (nav_helper_->IsListeningForNavigationStart()) {
    google_url_tracker_->OnNavigationPending(
        reinterpret_cast<content::NavigationController*>(unique_id),
        reinterpret_cast<InfoBarService*>(unique_id), unique_id);
  }
}

void GoogleURLTrackerTest::CommitNonSearch(intptr_t unique_id) {
  GoogleURLTrackerMapEntry* map_entry = GetMapEntry(unique_id);
  if (!map_entry)
    return;

  ExpectListeningForCommit(unique_id, false);

  // The infobar should be showing; otherwise the pending non-search should
  // have closed it.
  ASSERT_TRUE(map_entry->has_infobar());

  // The pending_id should have been reset to 0 when the non-search became
  // pending.
  EXPECT_EQ(0, map_entry->infobar()->pending_id());

  // Committing the navigation would close the infobar.
  map_entry->infobar()->Close(false);
}

void GoogleURLTrackerTest::CommitSearch(intptr_t unique_id,
                                        const GURL& search_url) {
  DCHECK(search_url.is_valid());
  if (nav_helper_->IsListeningForNavigationCommit(
      reinterpret_cast<content::NavigationController*>(unique_id))) {
    google_url_tracker_->OnNavigationCommitted(
        reinterpret_cast<InfoBarService*>(unique_id),
        search_url);
  }
}

void GoogleURLTrackerTest::CloseTab(intptr_t unique_id) {
  unique_ids_seen_.erase(unique_id);
  content::NavigationController* nav_controller =
      reinterpret_cast<content::NavigationController*>(unique_id);
  if (nav_helper_->IsListeningForTabDestruction(nav_controller)) {
    google_url_tracker_->OnTabClosed(nav_controller);
  } else {
    // Closing a tab with an infobar showing would close the infobar.
    GoogleURLTrackerInfoBarDelegate* infobar = GetInfoBar(unique_id);
    if (infobar)
      infobar->Close(false);
  }
}

GoogleURLTrackerMapEntry* GoogleURLTrackerTest::GetMapEntry(
    intptr_t unique_id) {
  GoogleURLTracker::EntryMap::const_iterator i =
      google_url_tracker_->entry_map_.find(
          reinterpret_cast<InfoBarService*>(unique_id));
  return (i == google_url_tracker_->entry_map_.end()) ? NULL : i->second;
}

GoogleURLTrackerInfoBarDelegate* GoogleURLTrackerTest::GetInfoBar(
    intptr_t unique_id) {
  GoogleURLTrackerMapEntry* map_entry = GetMapEntry(unique_id);
  return map_entry ? map_entry->infobar() : NULL;
}

void GoogleURLTrackerTest::ExpectDefaultURLs() const {
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL(), fetched_google_url());
}

void GoogleURLTrackerTest::ExpectListeningForCommit(intptr_t unique_id,
                                                    bool listening) {
  GoogleURLTrackerMapEntry* map_entry = GetMapEntry(unique_id);
  if (map_entry) {
    EXPECT_EQ(listening, nav_helper_->IsListeningForNavigationCommit(
        map_entry->navigation_controller()));
  } else {
    EXPECT_FALSE(listening);
  }
}

GoogleURLTrackerInfoBarDelegate* GoogleURLTrackerTest::CreateTestInfoBar(
    InfoBarService* infobar_service,
    GoogleURLTracker* google_url_tracker,
    const GURL& search_url) {
  return TestInfoBarDelegate::Create(this, infobar_service, google_url_tracker,
                                     search_url);
}


// TestInfoBarDelegate --------------------------------------------------------

namespace {

// static
GoogleURLTrackerInfoBarDelegate* TestInfoBarDelegate::Create(
    GoogleURLTrackerTest* test_harness,
    InfoBarService* infobar_service,
    GoogleURLTracker* google_url_tracker,
    const GURL& search_url) {
  return new TestInfoBarDelegate(test_harness, infobar_service,
                                 google_url_tracker, search_url);
}

TestInfoBarDelegate::TestInfoBarDelegate(GoogleURLTrackerTest* test_harness,
                                         InfoBarService* infobar_service,
                                         GoogleURLTracker* google_url_tracker,
                                         const GURL& search_url)
  : GoogleURLTrackerInfoBarDelegate(NULL, google_url_tracker, search_url),
    test_harness_(test_harness),
    infobar_service_(infobar_service) {
}

TestInfoBarDelegate::~TestInfoBarDelegate() {
}

void TestInfoBarDelegate::Update(const GURL& search_url) {
  set_search_url(search_url);
  set_pending_id(0);
}

void TestInfoBarDelegate::Close(bool redo_search) {
  test_harness_->OnInfoBarClosed(this, infobar_service_);
}

}  // namespace


// Tests ----------------------------------------------------------------------

TEST_F(GoogleURLTrackerTest, DontFetchWhenNoOneRequestsCheck) {
  ExpectDefaultURLs();
  FinishSleep();
  // No one called RequestServerCheck() so nothing should have happened.
  EXPECT_FALSE(GetFetcher());
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_notified());
}

TEST_F(GoogleURLTrackerTest, UpdateOnFirstRun) {
  RequestServerCheck();
  EXPECT_FALSE(GetFetcher());
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_notified());

  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  // GoogleURL should be updated, becase there was no last prompted URL.
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_TRUE(observer_notified());
}

TEST_F(GoogleURLTrackerTest, DontUpdateWhenUnchanged) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));

  RequestServerCheck();
  EXPECT_FALSE(GetFetcher());
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_notified());

  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  // GoogleURL should not be updated, because the fetched and prompted URLs
  // match.
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(observer_notified());
}

TEST_F(GoogleURLTrackerTest, DontPromptOnBadReplies) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));

  RequestServerCheck();
  EXPECT_FALSE(GetFetcher());
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_notified());

  // Old-style domain string.
  FinishSleep();
  MockSearchDomainCheckResponse(".google.co.in");
  EXPECT_EQ(GURL(), fetched_google_url());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(observer_notified());
  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.co.uk/search?q=test"));
  EXPECT_TRUE(GetMapEntry(1) == NULL);

  // Bad subdomain.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://mail.google.com/");
  EXPECT_EQ(GURL(), fetched_google_url());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(observer_notified());
  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.co.uk/search?q=test"));
  EXPECT_TRUE(GetMapEntry(1) == NULL);

  // Non-empty path.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.com/search");
  EXPECT_EQ(GURL(), fetched_google_url());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(observer_notified());
  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.co.uk/search?q=test"));
  EXPECT_TRUE(GetMapEntry(1) == NULL);

  // Non-empty query.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.com/?q=foo");
  EXPECT_EQ(GURL(), fetched_google_url());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(observer_notified());
  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.co.uk/search?q=test"));
  EXPECT_TRUE(GetMapEntry(1) == NULL);

  // Non-empty ref.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.com/#anchor");
  EXPECT_EQ(GURL(), fetched_google_url());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(observer_notified());
  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.co.uk/search?q=test"));
  EXPECT_TRUE(GetMapEntry(1) == NULL);

  // Complete garbage.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("HJ)*qF)_*&@f1");
  EXPECT_EQ(GURL(), fetched_google_url());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(observer_notified());
  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.co.uk/search?q=test"));
  EXPECT_TRUE(GetMapEntry(1) == NULL);
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
  EXPECT_FALSE(observer_notified());
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
  EXPECT_TRUE(observer_notified());

  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(observer_notified());
}

TEST_F(GoogleURLTrackerTest, RefetchOnIPAddressChange) {
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_TRUE(observer_notified());
  clear_observer_notified();

  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.in/");
  EXPECT_EQ(GURL("http://www.google.co.in/"), fetched_google_url());
  // Just fetching a new URL shouldn't reset things without a prompt.
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_FALSE(observer_notified());
}

TEST_F(GoogleURLTrackerTest, DontRefetchWhenNoOneRequestsCheck) {
  FinishSleep();
  NotifyIPAddressChanged();
  // No one called RequestServerCheck() so nothing should have happened.
  EXPECT_FALSE(GetFetcher());
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  ExpectDefaultURLs();
  EXPECT_FALSE(observer_notified());
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
  EXPECT_TRUE(observer_notified());
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
  EXPECT_TRUE(observer_notified());
  clear_observer_notified();

  RequestServerCheck();
  // The second request should be ignored.
  EXPECT_FALSE(GetFetcher());
  MockSearchDomainCheckResponse("http://www.google.co.in/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_FALSE(observer_notified());
}

TEST_F(GoogleURLTrackerTest, SearchingDoesNothingIfNoNeedToPrompt) {
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(observer_notified());
  clear_observer_notified();

  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.co.uk/search?q=test"));
  EXPECT_TRUE(GetMapEntry(1) == NULL);
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_notified());
}

TEST_F(GoogleURLTrackerTest, TabClosedOnPendingSearch) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_notified());

  SetNavigationPending(1, true);
  GoogleURLTrackerMapEntry* map_entry = GetMapEntry(1);
  ASSERT_FALSE(map_entry == NULL);
  EXPECT_FALSE(map_entry->has_infobar());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_notified());

  CloseTab(1);
  EXPECT_TRUE(GetMapEntry(1) == NULL);
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_notified());
}

TEST_F(GoogleURLTrackerTest, TabClosedOnCommittedSearch) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.co.uk/search?q=test"));
  EXPECT_FALSE(GetInfoBar(1) == NULL);

  CloseTab(1);
  EXPECT_TRUE(GetMapEntry(1) == NULL);
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_notified());
}

TEST_F(GoogleURLTrackerTest, InfoBarClosed) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.co.uk/search?q=test"));
  GoogleURLTrackerInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);

  infobar->Close(false);
  EXPECT_TRUE(GetMapEntry(1) == NULL);
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_notified());
}

TEST_F(GoogleURLTrackerTest, InfoBarRefused) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.co.uk/search?q=test"));
  GoogleURLTrackerInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);

  infobar->Cancel();
  EXPECT_TRUE(GetMapEntry(1) == NULL);
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_notified());
}

TEST_F(GoogleURLTrackerTest, InfoBarAccepted) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.co.uk/search?q=test"));
  GoogleURLTrackerInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);

  infobar->Accept();
  EXPECT_TRUE(GetMapEntry(1) == NULL);
  EXPECT_EQ(GURL("http://www.google.co.jp/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(observer_notified());
}

TEST_F(GoogleURLTrackerTest, FetchesCanAutomaticallyCloseInfoBars) {
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(google_url().spec());

  // Re-fetching the accepted URL after showing an infobar for another URL
  // should close the infobar.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.com/search?q=test"));
  EXPECT_FALSE(GetInfoBar(1) == NULL);
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse(google_url().spec());
  EXPECT_EQ(google_url(), GetLastPromptedGoogleURL());
  EXPECT_TRUE(GetMapEntry(1) == NULL);

  // As should fetching a URL that differs from the accepted only by the scheme.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.com/search?q=test"));
  EXPECT_FALSE(GetInfoBar(1) == NULL);
  NotifyIPAddressChanged();
  url_canon::Replacements<char> replacements;
  const std::string& scheme("https");
  replacements.SetScheme(scheme.data(),
                         url_parse::Component(0, scheme.length()));
  GURL new_google_url(google_url().ReplaceComponents(replacements));
  MockSearchDomainCheckResponse(new_google_url.spec());
  EXPECT_EQ(new_google_url, GetLastPromptedGoogleURL());
  EXPECT_TRUE(GetMapEntry(1) == NULL);

  // As should re-fetching the last prompted URL.
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");
  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.com/search?q=test"));
  EXPECT_FALSE(GetInfoBar(1) == NULL);
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(new_google_url, google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(GetMapEntry(1) == NULL);

  // And one that differs from the last prompted URL only by the scheme.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");
  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.com/search?q=test"));
  EXPECT_FALSE(GetInfoBar(1) == NULL);
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("https://www.google.co.uk/");
  EXPECT_EQ(new_google_url, google_url());
  EXPECT_EQ(GURL("https://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(GetMapEntry(1) == NULL);

  // And fetching a different URL entirely.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");
  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.com/search?q=test"));
  EXPECT_FALSE(GetInfoBar(1) == NULL);
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("https://www.google.co.in/");
  EXPECT_EQ(new_google_url, google_url());
  EXPECT_EQ(GURL("https://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(GetMapEntry(1) == NULL);
}

TEST_F(GoogleURLTrackerTest, ResetInfoBarGoogleURLs) {
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(google_url().spec());

  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.com/search?q=test"));
  GoogleURLTrackerInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());

  // If while an infobar is showing we fetch a new URL that differs from the
  // infobar's only by scheme, the infobar should stay showing.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("https://www.google.co.uk/");
  EXPECT_EQ(infobar, GetInfoBar(1));
  EXPECT_EQ(GURL("https://www.google.co.uk/"), fetched_google_url());
}

TEST_F(GoogleURLTrackerTest, NavigationsAfterPendingSearch) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  // A pending non-search after a pending search should delete the map entry.
  SetNavigationPending(1, true);
  GoogleURLTrackerMapEntry* map_entry = GetMapEntry(1);
  ASSERT_FALSE(map_entry == NULL);
  EXPECT_FALSE(map_entry->has_infobar());
  SetNavigationPending(1, false);
  EXPECT_TRUE(GetMapEntry(1) == NULL);

  // A pending search after a pending search should leave the map entry alive.
  SetNavigationPending(1, true);
  map_entry = GetMapEntry(1);
  ASSERT_FALSE(map_entry == NULL);
  EXPECT_FALSE(map_entry->has_infobar());
  SetNavigationPending(1, true);
  ASSERT_EQ(map_entry, GetMapEntry(1));
  EXPECT_FALSE(map_entry->has_infobar());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, true));

  // Committing this search should show an infobar.
  CommitSearch(1, GURL("http://www.google.co.uk/search?q=test2"));
  EXPECT_TRUE(map_entry->has_infobar());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_notified());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, false));
}

TEST_F(GoogleURLTrackerTest, NavigationsAfterCommittedSearch) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");
  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.co.uk/search?q=test"));
  GoogleURLTrackerInfoBarDelegate* infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, false));

  // A pending non-search on a visible infobar should basically do nothing.
  SetNavigationPending(1, false);
  ASSERT_EQ(infobar, GetInfoBar(1));
  EXPECT_EQ(0, infobar->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, false));

  // As should another pending non-search after the first.
  SetNavigationPending(1, false);
  ASSERT_EQ(infobar, GetInfoBar(1));
  EXPECT_EQ(0, infobar->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, false));

  // Committing this non-search should close the infobar.  The control flow in
  // these tests is not really comparable to in the real browser, but at least a
  // few sanity-checks will be performed.
  ASSERT_NO_FATAL_FAILURE(CommitNonSearch(1));
  EXPECT_TRUE(GetMapEntry(1) == NULL);

  // A pending search on a visible infobar should cause the infobar to listen
  // for the search to commit.
  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.co.uk/search?q=test"));
  infobar = GetInfoBar(1);
  ASSERT_FALSE(infobar == NULL);
  SetNavigationPending(1, true);
  ASSERT_EQ(infobar, GetInfoBar(1));
  EXPECT_EQ(1, infobar->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, true));

  // But a non-search after this should cancel that state.
  SetNavigationPending(1, false);
  ASSERT_EQ(infobar, GetInfoBar(1));
  EXPECT_EQ(0, infobar->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, false));

  // Another pending search after the non-search should put us back into
  // "waiting for commit" mode.
  SetNavigationPending(1, true);
  ASSERT_EQ(infobar, GetInfoBar(1));
  EXPECT_EQ(1, infobar->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, true));

  // A second pending search after the first should not really change anything.
  SetNavigationPending(1, true);
  ASSERT_EQ(infobar, GetInfoBar(1));
  EXPECT_EQ(1, infobar->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, true));

  // Committing this search should change the visible infobar's search_url.
  CommitSearch(1, GURL("http://www.google.co.uk/search?q=test2"));
  ASSERT_EQ(infobar, GetInfoBar(1));
  EXPECT_EQ(GURL("http://www.google.co.uk/search?q=test2"),
            infobar->search_url());
  EXPECT_EQ(0, infobar->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, false));
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(observer_notified());
}

TEST_F(GoogleURLTrackerTest, MultipleMapEntries) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  SetNavigationPending(1, true);
  GoogleURLTrackerMapEntry* map_entry = GetMapEntry(1);
  ASSERT_FALSE(map_entry == NULL);
  EXPECT_FALSE(map_entry->has_infobar());

  SetNavigationPending(2, true);
  CommitSearch(2, GURL("http://www.google.co.uk/search?q=test2"));
  GoogleURLTrackerInfoBarDelegate* infobar2 = GetInfoBar(2);
  ASSERT_FALSE(infobar2 == NULL);
  EXPECT_EQ(GURL("http://www.google.co.uk/search?q=test2"),
            infobar2->search_url());

  SetNavigationPending(3, true);
  GoogleURLTrackerMapEntry* map_entry3 = GetMapEntry(3);
  ASSERT_FALSE(map_entry3 == NULL);
  EXPECT_FALSE(map_entry3->has_infobar());

  SetNavigationPending(4, true);
  CommitSearch(4, GURL("http://www.google.co.uk/search?q=test4"));
  GoogleURLTrackerInfoBarDelegate* infobar4 = GetInfoBar(4);
  ASSERT_FALSE(infobar4 == NULL);
  EXPECT_EQ(GURL("http://www.google.co.uk/search?q=test4"),
            infobar4->search_url());

  CommitSearch(1, GURL("http://www.google.co.uk/search?q=test"));
  EXPECT_TRUE(map_entry->has_infobar());

  infobar2->Close(false);
  EXPECT_TRUE(GetMapEntry(2) == NULL);
  EXPECT_FALSE(observer_notified());

  infobar4->Accept();
  EXPECT_TRUE(GetMapEntry(1) == NULL);
  EXPECT_TRUE(GetMapEntry(3) == NULL);
  EXPECT_TRUE(GetMapEntry(4) == NULL);
  EXPECT_EQ(GURL("http://www.google.co.jp/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(observer_notified());
}

TEST_F(GoogleURLTrackerTest, IgnoreIrrelevantNavigation) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  // This tests a particularly gnarly sequence of events that used to cause us
  // to erroneously listen for a non-search navigation to commit.
  SetNavigationPending(1, true);
  CommitSearch(1, GURL("http://www.google.co.uk/search?q=test"));
  SetNavigationPending(2, true);
  CommitSearch(2, GURL("http://www.google.co.uk/search?q=test2"));
  EXPECT_FALSE(GetInfoBar(1) == NULL);
  GoogleURLTrackerInfoBarDelegate* infobar2 = GetInfoBar(2);
  ASSERT_FALSE(infobar2 == NULL);
  SetNavigationPending(1, true);
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, true));
  infobar2->Close(false);
  SetNavigationPending(1, false);
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(1, false));
}
