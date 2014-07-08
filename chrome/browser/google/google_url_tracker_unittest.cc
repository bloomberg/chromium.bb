// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/google/core/browser/google_url_tracker.h"

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/google/core/browser/google_pref_names.h"
#include "components/google/core/browser/google_url_tracker_client.h"
#include "components/google/core/browser/google_url_tracker_infobar_delegate.h"
#include "components/google/core/browser/google_url_tracker_navigation_helper.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// TestCallbackListener ---------------------------------------------------

class TestCallbackListener {
 public:
  TestCallbackListener();
  virtual ~TestCallbackListener();

  bool HasRegisteredCallback();
  void RegisterCallback(GoogleURLTracker* google_url_tracker);

  bool notified() const { return notified_; }
  void clear_notified() { notified_ = false; }

 private:
  void OnGoogleURLUpdated();

  bool notified_;
  scoped_ptr<GoogleURLTracker::Subscription> google_url_updated_subscription_;
};

TestCallbackListener::TestCallbackListener() : notified_(false) {
}

TestCallbackListener::~TestCallbackListener() {
}

void TestCallbackListener::OnGoogleURLUpdated() {
  notified_ = true;
}

bool TestCallbackListener::HasRegisteredCallback() {
  return google_url_updated_subscription_.get();
}

void TestCallbackListener::RegisterCallback(
    GoogleURLTracker* google_url_tracker) {
  google_url_updated_subscription_ =
      google_url_tracker->RegisterCallback(base::Bind(
          &TestCallbackListener::OnGoogleURLUpdated, base::Unretained(this)));
}


// TestGoogleURLTrackerClient -------------------------------------------------

class TestGoogleURLTrackerClient : public GoogleURLTrackerClient {
 public:
  explicit TestGoogleURLTrackerClient(Profile* profile_);
  virtual ~TestGoogleURLTrackerClient();

  virtual void SetListeningForNavigationStart(bool listen) OVERRIDE;
  virtual bool IsListeningForNavigationStart() OVERRIDE;
  virtual bool IsBackgroundNetworkingEnabled() OVERRIDE;
  virtual PrefService* GetPrefs() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;

 private:
  Profile* profile_;
  bool observe_nav_start_;

  DISALLOW_COPY_AND_ASSIGN(TestGoogleURLTrackerClient);
};

TestGoogleURLTrackerClient::TestGoogleURLTrackerClient(Profile* profile)
    : profile_(profile),
      observe_nav_start_(false) {
}

TestGoogleURLTrackerClient::~TestGoogleURLTrackerClient() {
}

void TestGoogleURLTrackerClient::SetListeningForNavigationStart(bool listen) {
  observe_nav_start_ = listen;
}

bool TestGoogleURLTrackerClient::IsListeningForNavigationStart() {
  return observe_nav_start_;
}

bool TestGoogleURLTrackerClient::IsBackgroundNetworkingEnabled() {
  return true;
}

PrefService* TestGoogleURLTrackerClient::GetPrefs() {
  return profile_->GetPrefs();
}

net::URLRequestContextGetter* TestGoogleURLTrackerClient::GetRequestContext() {
  return profile_->GetRequestContext();
}


// TestGoogleURLTrackerNavigationHelper ---------------------------------------

class TestGoogleURLTrackerNavigationHelper
    : public GoogleURLTrackerNavigationHelper {
 public:
  explicit TestGoogleURLTrackerNavigationHelper(GoogleURLTracker* tracker);
  virtual ~TestGoogleURLTrackerNavigationHelper();

  virtual void SetListeningForNavigationCommit(bool listen) OVERRIDE;
  virtual bool IsListeningForNavigationCommit() OVERRIDE;
  virtual void SetListeningForTabDestruction(bool listen) OVERRIDE;
  virtual bool IsListeningForTabDestruction() OVERRIDE;
  virtual void OpenURL(GURL url,
                       WindowOpenDisposition disposition,
                       bool user_clicked_on_link) OVERRIDE;

 private:
  bool listening_for_nav_commit_;
  bool listening_for_tab_destruction_;

  DISALLOW_COPY_AND_ASSIGN(TestGoogleURLTrackerNavigationHelper);
};

TestGoogleURLTrackerNavigationHelper::TestGoogleURLTrackerNavigationHelper(
    GoogleURLTracker* tracker)
    : GoogleURLTrackerNavigationHelper(tracker),
      listening_for_nav_commit_(false),
      listening_for_tab_destruction_(false) {
}

TestGoogleURLTrackerNavigationHelper::~TestGoogleURLTrackerNavigationHelper() {
}

void TestGoogleURLTrackerNavigationHelper::SetListeningForNavigationCommit(
    bool listen) {
  listening_for_nav_commit_ = listen;
}

bool TestGoogleURLTrackerNavigationHelper::IsListeningForNavigationCommit() {
  return listening_for_nav_commit_;
}

void TestGoogleURLTrackerNavigationHelper::SetListeningForTabDestruction(
    bool listen) {
  listening_for_tab_destruction_ = listen;
}

bool TestGoogleURLTrackerNavigationHelper::IsListeningForTabDestruction() {
  return listening_for_tab_destruction_;
}

void TestGoogleURLTrackerNavigationHelper::OpenURL(
    GURL url,
    WindowOpenDisposition disposition,
    bool user_clicked_on_link) {
}

// TestInfoBarManager ---------------------------------------------------------

class TestInfoBarManager : public infobars::InfoBarManager {
 public:
  explicit TestInfoBarManager(int unique_id);
  virtual ~TestInfoBarManager();
  virtual int GetActiveEntryID() OVERRIDE;

 private:
  int unique_id_;
  DISALLOW_COPY_AND_ASSIGN(TestInfoBarManager);
};

TestInfoBarManager::TestInfoBarManager(int unique_id) : unique_id_(unique_id) {
}

TestInfoBarManager::~TestInfoBarManager() {
  ShutDown();
}

int TestInfoBarManager::GetActiveEntryID() {
  return unique_id_;
}

}  // namespace

// GoogleURLTrackerTest -------------------------------------------------------

class GoogleURLTrackerTest : public testing::Test {
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
  void SetNavigationPending(infobars::InfoBarManager* infobar_manager,
                            bool is_search);
  void CommitNonSearch(infobars::InfoBarManager* infobar_manager);
  void CommitSearch(infobars::InfoBarManager* infobar_manager,
                    const GURL& search_url);
  void CloseTab(infobars::InfoBarManager* infobar_manager);
  GoogleURLTrackerMapEntry* GetMapEntry(
      infobars::InfoBarManager* infobar_manager);
  GoogleURLTrackerInfoBarDelegate* GetInfoBarDelegate(
      infobars::InfoBarManager* infobar_manager);
  GoogleURLTrackerNavigationHelper* GetNavigationHelper(
      infobars::InfoBarManager* infobar_manager);
  void ExpectDefaultURLs() const;
  void ExpectListeningForCommit(infobars::InfoBarManager* infobar_manager,
                                bool listening);
  bool listener_notified() const { return listener_.notified(); }
  void clear_listener_notified() { listener_.clear_notified(); }

 private:
  // These are required by the TestURLFetchers GoogleURLTracker will create (see
  // test_url_fetcher_factory.h).
  content::TestBrowserThreadBundle thread_bundle_;

  // Creating this allows us to call
  // net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests().
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  net::TestURLFetcherFactory fetcher_factory_;
  GoogleURLTrackerClient* client_;
  TestingProfile profile_;
  scoped_ptr<GoogleURLTracker> google_url_tracker_;
  TestCallbackListener listener_;
  // This tracks the different "tabs" a test has "opened", so we can close them
  // properly before shutting down |google_url_tracker_|, which expects that.
  std::set<infobars::InfoBarManager*> infobar_managers_seen_;
};

GoogleURLTrackerTest::GoogleURLTrackerTest()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
  GoogleURLTrackerFactory::GetInstance()->
      RegisterUserPrefsOnBrowserContextForTest(&profile_);
}

GoogleURLTrackerTest::~GoogleURLTrackerTest() {
}

void GoogleURLTrackerTest::SetUp() {
  network_change_notifier_.reset(net::NetworkChangeNotifier::CreateMock());
  // Ownership is passed to google_url_tracker_, but a weak pointer is kept;
  // this is safe since GoogleURLTracker keeps the client for its lifetime.
  client_ = new TestGoogleURLTrackerClient(&profile_);
  scoped_ptr<GoogleURLTrackerClient> client(client_);
  google_url_tracker_.reset(new GoogleURLTracker(
      client.Pass(), GoogleURLTracker::UNIT_TEST_MODE));
}

void GoogleURLTrackerTest::TearDown() {
  while (!infobar_managers_seen_.empty())
    CloseTab(*infobar_managers_seen_.begin());
  google_url_tracker_->Shutdown();
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
  if (!listener_.HasRegisteredCallback())
    listener_.RegisterCallback(google_url_tracker_.get());
  google_url_tracker_->SetNeedToFetch();
}

void GoogleURLTrackerTest::FinishSleep() {
  google_url_tracker_->FinishSleep();
}

void GoogleURLTrackerTest::NotifyIPAddressChanged() {
  net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  // For thread safety, the NCN queues tasks to do the actual notifications, so
  // we need to spin the message loop so the tracker will actually be notified.
  base::MessageLoop::current()->RunUntilIdle();
}

void GoogleURLTrackerTest::SetLastPromptedGoogleURL(const GURL& url) {
  profile_.GetPrefs()->SetString(prefs::kLastPromptedGoogleURL, url.spec());
}

GURL GoogleURLTrackerTest::GetLastPromptedGoogleURL() {
  return GURL(profile_.GetPrefs()->GetString(prefs::kLastPromptedGoogleURL));
}

void GoogleURLTrackerTest::SetNavigationPending(
    infobars::InfoBarManager* infobar_manager,
    bool is_search) {
  if (is_search) {
    google_url_tracker_->SearchCommitted();
    // Note that the call above might not have actually registered a listener
    // for navigation starts if the searchdomaincheck response was bogus.
  }
  infobar_managers_seen_.insert(infobar_manager);
  if (client_->IsListeningForNavigationStart()) {
    google_url_tracker_->OnNavigationPending(
        scoped_ptr<GoogleURLTrackerNavigationHelper>(
            new TestGoogleURLTrackerNavigationHelper(
                google_url_tracker_.get())),
        infobar_manager,
        infobar_manager->GetActiveEntryID());
  }
}

void GoogleURLTrackerTest::CommitNonSearch(
    infobars::InfoBarManager* infobar_manager) {
  GoogleURLTrackerMapEntry* map_entry = GetMapEntry(infobar_manager);
  if (!map_entry)
    return;

  ExpectListeningForCommit(infobar_manager, false);

  // The infobar should be showing; otherwise the pending non-search should
  // have closed it.
  ASSERT_TRUE(map_entry->has_infobar_delegate());

  // The pending_id should have been reset to 0 when the non-search became
  // pending.
  EXPECT_EQ(0, map_entry->infobar_delegate()->pending_id());

  // Committing the navigation would close the infobar.
  map_entry->infobar_delegate()->Close(false);
}

void GoogleURLTrackerTest::CommitSearch(
    infobars::InfoBarManager* infobar_manager,
    const GURL& search_url) {
  DCHECK(search_url.is_valid());
  GoogleURLTrackerNavigationHelper* nav_helper =
      GetNavigationHelper(infobar_manager);
  if (nav_helper && nav_helper->IsListeningForNavigationCommit()) {
    google_url_tracker_->OnNavigationCommitted(infobar_manager, search_url);
  }
}

void GoogleURLTrackerTest::CloseTab(infobars::InfoBarManager* infobar_manager) {
  infobar_managers_seen_.erase(infobar_manager);
  GoogleURLTrackerNavigationHelper* nav_helper =
      GetNavigationHelper(infobar_manager);
  if (nav_helper && nav_helper->IsListeningForTabDestruction()) {
    google_url_tracker_->OnTabClosed(nav_helper);
  } else {
    // Closing a tab with an infobar showing would close the infobar.
    GoogleURLTrackerInfoBarDelegate* delegate =
        GetInfoBarDelegate(infobar_manager);
    if (delegate)
      delegate->Close(false);
  }
}

GoogleURLTrackerMapEntry* GoogleURLTrackerTest::GetMapEntry(
    infobars::InfoBarManager* infobar_manager) {
  GoogleURLTracker::EntryMap::const_iterator i =
      google_url_tracker_->entry_map_.find(infobar_manager);
  return (i == google_url_tracker_->entry_map_.end()) ? NULL : i->second;
}

GoogleURLTrackerInfoBarDelegate* GoogleURLTrackerTest::GetInfoBarDelegate(
    infobars::InfoBarManager* infobar_manager) {
  GoogleURLTrackerMapEntry* map_entry = GetMapEntry(infobar_manager);
  return map_entry ? map_entry->infobar_delegate() : NULL;
}

GoogleURLTrackerNavigationHelper* GoogleURLTrackerTest::GetNavigationHelper(
    infobars::InfoBarManager* infobar_manager) {
  GoogleURLTrackerMapEntry* map_entry = GetMapEntry(infobar_manager);
  return map_entry ? map_entry->navigation_helper() : NULL;
}

void GoogleURLTrackerTest::ExpectDefaultURLs() const {
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL(), fetched_google_url());
}

void GoogleURLTrackerTest::ExpectListeningForCommit(
    infobars::InfoBarManager* infobar_manager,
    bool listening) {
  GoogleURLTrackerMapEntry* map_entry = GetMapEntry(infobar_manager);
  if (map_entry) {
    EXPECT_EQ(listening,
              map_entry->navigation_helper()->IsListeningForNavigationCommit());
  } else {
    EXPECT_FALSE(listening);
  }
}

// Tests ----------------------------------------------------------------------

TEST_F(GoogleURLTrackerTest, DontFetchWhenNoOneRequestsCheck) {
  ExpectDefaultURLs();
  FinishSleep();
  // No one called RequestServerCheck() so nothing should have happened.
  EXPECT_FALSE(GetFetcher());
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  ExpectDefaultURLs();
  EXPECT_FALSE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, UpdateOnFirstRun) {
  RequestServerCheck();
  EXPECT_FALSE(GetFetcher());
  ExpectDefaultURLs();
  EXPECT_FALSE(listener_notified());

  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  // GoogleURL should be updated, becase there was no last prompted URL.
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_TRUE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, DontUpdateWhenUnchanged) {
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));

  RequestServerCheck();
  EXPECT_FALSE(GetFetcher());
  ExpectDefaultURLs();
  EXPECT_FALSE(listener_notified());

  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  // GoogleURL should not be updated, because the fetched and prompted URLs
  // match.
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, DontPromptOnBadReplies) {
  TestInfoBarManager infobar_manager(1);
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));

  RequestServerCheck();
  EXPECT_FALSE(GetFetcher());
  ExpectDefaultURLs();
  EXPECT_FALSE(listener_notified());

  // Old-style domain string.
  FinishSleep();
  MockSearchDomainCheckResponse(".google.co.in");
  EXPECT_EQ(GURL(), fetched_google_url());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(listener_notified());
  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.co.uk/search?q=test"));
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);

  // Bad subdomain.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://mail.google.com/");
  EXPECT_EQ(GURL(), fetched_google_url());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(listener_notified());
  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.co.uk/search?q=test"));
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);

  // Non-empty path.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.com/search");
  EXPECT_EQ(GURL(), fetched_google_url());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(listener_notified());
  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.co.uk/search?q=test"));
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);

  // Non-empty query.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.com/?q=foo");
  EXPECT_EQ(GURL(), fetched_google_url());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(listener_notified());
  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.co.uk/search?q=test"));
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);

  // Non-empty ref.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.com/#anchor");
  EXPECT_EQ(GURL(), fetched_google_url());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(listener_notified());
  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.co.uk/search?q=test"));
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);

  // Complete garbage.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("HJ)*qF)_*&@f1");
  EXPECT_EQ(GURL(), fetched_google_url());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_FALSE(listener_notified());
  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.co.uk/search?q=test"));
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);
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
  EXPECT_FALSE(listener_notified());
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
  EXPECT_TRUE(listener_notified());

  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, RefetchOnIPAddressChange) {
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_TRUE(listener_notified());
  clear_listener_notified();

  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.in/");
  EXPECT_EQ(GURL("http://www.google.co.in/"), fetched_google_url());
  // Just fetching a new URL shouldn't reset things without a prompt.
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_FALSE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, DontRefetchWhenNoOneRequestsCheck) {
  FinishSleep();
  NotifyIPAddressChanged();
  // No one called RequestServerCheck() so nothing should have happened.
  EXPECT_FALSE(GetFetcher());
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  ExpectDefaultURLs();
  EXPECT_FALSE(listener_notified());
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
  EXPECT_TRUE(listener_notified());
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
  EXPECT_TRUE(listener_notified());
  clear_listener_notified();

  RequestServerCheck();
  // The second request should be ignored.
  EXPECT_FALSE(GetFetcher());
  MockSearchDomainCheckResponse("http://www.google.co.in/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_FALSE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, SearchingDoesNothingIfNoNeedToPrompt) {
  TestInfoBarManager infobar_manager(1);
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(listener_notified());
  clear_listener_notified();

  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.co.uk/search?q=test"));
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, TabClosedOnPendingSearch) {
  TestInfoBarManager infobar_manager(1);
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), fetched_google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(listener_notified());

  SetNavigationPending(&infobar_manager, true);
  GoogleURLTrackerMapEntry* map_entry = GetMapEntry(&infobar_manager);
  ASSERT_FALSE(map_entry == NULL);
  EXPECT_FALSE(map_entry->has_infobar_delegate());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(listener_notified());

  CloseTab(&infobar_manager);
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, TabClosedOnCommittedSearch) {
  TestInfoBarManager infobar_manager(1);
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.co.uk/search?q=test"));
  EXPECT_FALSE(GetInfoBarDelegate(&infobar_manager) == NULL);

  CloseTab(&infobar_manager);
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, InfoBarClosed) {
  TestInfoBarManager infobar_manager(1);
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.co.uk/search?q=test"));
  GoogleURLTrackerInfoBarDelegate* infobar =
      GetInfoBarDelegate(&infobar_manager);
  ASSERT_FALSE(infobar == NULL);

  infobar->Close(false);
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, InfoBarRefused) {
  TestInfoBarManager infobar_manager(1);
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.co.uk/search?q=test"));
  GoogleURLTrackerInfoBarDelegate* infobar =
      GetInfoBarDelegate(&infobar_manager);
  ASSERT_FALSE(infobar == NULL);

  infobar->Cancel();
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, InfoBarAccepted) {
  TestInfoBarManager infobar_manager(1);
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.co.uk/search?q=test"));
  GoogleURLTrackerInfoBarDelegate* infobar =
      GetInfoBarDelegate(&infobar_manager);
  ASSERT_FALSE(infobar == NULL);

  infobar->Accept();
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);
  EXPECT_EQ(GURL("http://www.google.co.jp/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, FetchesCanAutomaticallyCloseInfoBars) {
  TestInfoBarManager infobar_manager(1);
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(google_url().spec());

  // Re-fetching the accepted URL after showing an infobar for another URL
  // should close the infobar.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.com/search?q=test"));
  EXPECT_FALSE(GetInfoBarDelegate(&infobar_manager) == NULL);
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse(google_url().spec());
  EXPECT_EQ(google_url(), GetLastPromptedGoogleURL());
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);

  // As should fetching a URL that differs from the accepted only by the scheme.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.com/search?q=test"));
  EXPECT_FALSE(GetInfoBarDelegate(&infobar_manager) == NULL);
  NotifyIPAddressChanged();
  url::Replacements<char> replacements;
  const std::string& scheme("https");
  replacements.SetScheme(scheme.data(), url::Component(0, scheme.length()));
  GURL new_google_url(google_url().ReplaceComponents(replacements));
  MockSearchDomainCheckResponse(new_google_url.spec());
  EXPECT_EQ(new_google_url, GetLastPromptedGoogleURL());
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);

  // As should re-fetching the last prompted URL.
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");
  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.com/search?q=test"));
  EXPECT_FALSE(GetInfoBarDelegate(&infobar_manager) == NULL);
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  EXPECT_EQ(new_google_url, google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);

  // And one that differs from the last prompted URL only by the scheme.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");
  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.com/search?q=test"));
  EXPECT_FALSE(GetInfoBarDelegate(&infobar_manager) == NULL);
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("https://www.google.co.uk/");
  EXPECT_EQ(new_google_url, google_url());
  EXPECT_EQ(GURL("https://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);

  // And fetching a different URL entirely.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");
  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.com/search?q=test"));
  EXPECT_FALSE(GetInfoBarDelegate(&infobar_manager) == NULL);
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("https://www.google.co.in/");
  EXPECT_EQ(new_google_url, google_url());
  EXPECT_EQ(GURL("https://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);
}

TEST_F(GoogleURLTrackerTest, ResetInfoBarGoogleURLs) {
  TestInfoBarManager infobar_manager(1);
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse(google_url().spec());

  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("http://www.google.co.uk/");
  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.com/search?q=test"));
  GoogleURLTrackerInfoBarDelegate* delegate =
      GetInfoBarDelegate(&infobar_manager);
  ASSERT_FALSE(delegate == NULL);
  EXPECT_EQ(GURL("http://www.google.co.uk/"), fetched_google_url());

  // If while an infobar is showing we fetch a new URL that differs from the
  // infobar's only by scheme, the infobar should stay showing.
  NotifyIPAddressChanged();
  MockSearchDomainCheckResponse("https://www.google.co.uk/");
  EXPECT_EQ(delegate, GetInfoBarDelegate(&infobar_manager));
  EXPECT_EQ(GURL("https://www.google.co.uk/"), fetched_google_url());
}

TEST_F(GoogleURLTrackerTest, NavigationsAfterPendingSearch) {
  TestInfoBarManager infobar_manager(1);
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  // A pending non-search after a pending search should delete the map entry.
  SetNavigationPending(&infobar_manager, true);
  GoogleURLTrackerMapEntry* map_entry = GetMapEntry(&infobar_manager);
  ASSERT_FALSE(map_entry == NULL);
  EXPECT_FALSE(map_entry->has_infobar_delegate());
  SetNavigationPending(&infobar_manager, false);
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);

  // A pending search after a pending search should leave the map entry alive.
  SetNavigationPending(&infobar_manager, true);
  map_entry = GetMapEntry(&infobar_manager);
  ASSERT_FALSE(map_entry == NULL);
  EXPECT_FALSE(map_entry->has_infobar_delegate());
  SetNavigationPending(&infobar_manager, true);
  ASSERT_EQ(map_entry, GetMapEntry(&infobar_manager));
  EXPECT_FALSE(map_entry->has_infobar_delegate());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(&infobar_manager, true));

  // Committing this search should show an infobar.
  CommitSearch(&infobar_manager,
               GURL("http://www.google.co.uk/search?q=test2"));
  EXPECT_TRUE(map_entry->has_infobar_delegate());
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(listener_notified());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(&infobar_manager, false));
}

TEST_F(GoogleURLTrackerTest, NavigationsAfterCommittedSearch) {
  TestInfoBarManager infobar_manager(1);
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");
  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.co.uk/search?q=test"));
  GoogleURLTrackerInfoBarDelegate* delegate =
      GetInfoBarDelegate(&infobar_manager);
  ASSERT_FALSE(delegate == NULL);
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(&infobar_manager, false));

  // A pending non-search on a visible infobar should basically do nothing.
  SetNavigationPending(&infobar_manager, false);
  ASSERT_EQ(delegate, GetInfoBarDelegate(&infobar_manager));
  EXPECT_EQ(0, delegate->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(&infobar_manager, false));

  // As should another pending non-search after the first.
  SetNavigationPending(&infobar_manager, false);
  ASSERT_EQ(delegate, GetInfoBarDelegate(&infobar_manager));
  EXPECT_EQ(0, delegate->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(&infobar_manager, false));

  // Committing this non-search should close the infobar.  The control flow in
  // these tests is not really comparable to in the real browser, but at least a
  // few sanity-checks will be performed.
  ASSERT_NO_FATAL_FAILURE(CommitNonSearch(&infobar_manager));
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);

  // A pending search on a visible infobar should cause the infobar to listen
  // for the search to commit.
  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.co.uk/search?q=test"));
  delegate = GetInfoBarDelegate(&infobar_manager);
  ASSERT_FALSE(delegate == NULL);
  SetNavigationPending(&infobar_manager, true);
  ASSERT_EQ(delegate, GetInfoBarDelegate(&infobar_manager));
  EXPECT_EQ(1, delegate->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(&infobar_manager, true));

  // But a non-search after this should cancel that state.
  SetNavigationPending(&infobar_manager, false);
  ASSERT_EQ(delegate, GetInfoBarDelegate(&infobar_manager));
  EXPECT_EQ(0, delegate->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(&infobar_manager, false));

  // Another pending search after the non-search should put us back into
  // "waiting for commit" mode.
  SetNavigationPending(&infobar_manager, true);
  ASSERT_EQ(delegate, GetInfoBarDelegate(&infobar_manager));
  EXPECT_EQ(1, delegate->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(&infobar_manager, true));

  // A second pending search after the first should not really change anything.
  SetNavigationPending(&infobar_manager, true);
  ASSERT_EQ(delegate, GetInfoBarDelegate(&infobar_manager));
  EXPECT_EQ(1, delegate->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(&infobar_manager, true));

  // Committing this search should change the visible infobar's search_url.
  CommitSearch(&infobar_manager,
               GURL("http://www.google.co.uk/search?q=test2"));
  ASSERT_EQ(delegate, GetInfoBarDelegate(&infobar_manager));
  EXPECT_EQ(GURL("http://www.google.co.uk/search?q=test2"),
            delegate->search_url());
  EXPECT_EQ(0, delegate->pending_id());
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(&infobar_manager, false));
  EXPECT_EQ(GURL(GoogleURLTracker::kDefaultGoogleHomepage), google_url());
  EXPECT_EQ(GURL("http://www.google.co.uk/"), GetLastPromptedGoogleURL());
  EXPECT_FALSE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, MultipleMapEntries) {
  TestInfoBarManager infobar_manager(1);
  TestInfoBarManager infobar_manager2(2);
  TestInfoBarManager infobar_manager3(3);
  TestInfoBarManager infobar_manager4(4);
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  SetNavigationPending(&infobar_manager, true);
  GoogleURLTrackerMapEntry* map_entry = GetMapEntry(&infobar_manager);
  ASSERT_FALSE(map_entry == NULL);
  EXPECT_FALSE(map_entry->has_infobar_delegate());

  SetNavigationPending(&infobar_manager2, true);
  CommitSearch(&infobar_manager2,
               GURL("http://www.google.co.uk/search?q=test2"));
  GoogleURLTrackerInfoBarDelegate* delegate2 =
      GetInfoBarDelegate(&infobar_manager2);
  ASSERT_FALSE(delegate2 == NULL);
  EXPECT_EQ(GURL("http://www.google.co.uk/search?q=test2"),
            delegate2->search_url());

  SetNavigationPending(&infobar_manager3, true);
  GoogleURLTrackerMapEntry* map_entry3 = GetMapEntry(&infobar_manager3);
  ASSERT_FALSE(map_entry3 == NULL);
  EXPECT_FALSE(map_entry3->has_infobar_delegate());

  SetNavigationPending(&infobar_manager4, true);
  CommitSearch(&infobar_manager4,
               GURL("http://www.google.co.uk/search?q=test4"));
  GoogleURLTrackerInfoBarDelegate* delegate4 =
      GetInfoBarDelegate(&infobar_manager4);
  ASSERT_FALSE(delegate4 == NULL);
  EXPECT_EQ(GURL("http://www.google.co.uk/search?q=test4"),
            delegate4->search_url());

  CommitSearch(&infobar_manager, GURL("http://www.google.co.uk/search?q=test"));
  EXPECT_TRUE(map_entry->has_infobar_delegate());

  delegate2->Close(false);
  EXPECT_TRUE(GetMapEntry(&infobar_manager2) == NULL);
  EXPECT_FALSE(listener_notified());

  delegate4->Accept();
  EXPECT_TRUE(GetMapEntry(&infobar_manager) == NULL);
  EXPECT_TRUE(GetMapEntry(&infobar_manager3) == NULL);
  EXPECT_TRUE(GetMapEntry(&infobar_manager4) == NULL);
  EXPECT_EQ(GURL("http://www.google.co.jp/"), google_url());
  EXPECT_EQ(GURL("http://www.google.co.jp/"), GetLastPromptedGoogleURL());
  EXPECT_TRUE(listener_notified());
}

TEST_F(GoogleURLTrackerTest, IgnoreIrrelevantNavigation) {
  TestInfoBarManager infobar_manager(1);
  TestInfoBarManager infobar_manager2(2);
  SetLastPromptedGoogleURL(GURL("http://www.google.co.uk/"));
  RequestServerCheck();
  FinishSleep();
  MockSearchDomainCheckResponse("http://www.google.co.jp/");

  // This tests a particularly gnarly sequence of events that used to cause us
  // to erroneously listen for a non-search navigation to commit.
  SetNavigationPending(&infobar_manager, true);
  CommitSearch(&infobar_manager, GURL("http://www.google.co.uk/search?q=test"));
  SetNavigationPending(&infobar_manager2, true);
  CommitSearch(&infobar_manager2,
               GURL("http://www.google.co.uk/search?q=test2"));
  EXPECT_FALSE(GetInfoBarDelegate(&infobar_manager) == NULL);
  GoogleURLTrackerInfoBarDelegate* delegate2 =
      GetInfoBarDelegate(&infobar_manager2);
  ASSERT_FALSE(delegate2 == NULL);
  SetNavigationPending(&infobar_manager, true);
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(&infobar_manager, true));
  delegate2->Close(false);
  SetNavigationPending(&infobar_manager, false);
  ASSERT_NO_FATAL_FAILURE(ExpectListeningForCommit(&infobar_manager, false));
}
