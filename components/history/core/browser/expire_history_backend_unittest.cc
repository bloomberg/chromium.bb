// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/expire_history_backend.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_backend_notifier.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/thumbnail_database.h"
#include "components/history/core/browser/top_sites.h"
#include "components/history/core/browser/top_sites_impl.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/history/core/common/thumbnail_score.h"
#include "components/history/core/test/history_client_fake_bookmarks.h"
#include "components/history/core/test/test_history_database.h"
#include "components/history/core/test/thumbnail.h"
#include "components/history/core/test/wait_top_sites_loaded_observer.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

// The test must be in the history namespace for the gtest forward declarations
// to work. It also eliminates a bunch of ugly "history::".
namespace history {

namespace {
// Returns whether |url| can be added to history.
bool MockCanAddURLToHistory(const GURL& url) {
  return url.is_valid();
}
}  // namespace

// ExpireHistoryTest -----------------------------------------------------------

class ExpireHistoryTest : public testing::Test, public HistoryBackendNotifier {
 public:
  ExpireHistoryTest()
      : backend_client_(history_client_.CreateBackendClient()),
        expirer_(this,
                 backend_client_.get(),
                 scoped_task_environment_.GetMainThreadTaskRunner()),
        now_(base::Time::Now()) {}

 protected:
  // Called by individual tests when they want data populated.
  void AddExampleData(URLID url_ids[3], base::Time visit_times[4]);
  // Add visits with source information.
  void AddExampleSourceData(const GURL& url, URLID* id);

  // Returns true if the given favicon/thumbnail has an entry in the DB.
  bool HasFavicon(favicon_base::FaviconID favicon_id);
  bool HasThumbnail(URLID url_id);

  favicon_base::FaviconID GetFavicon(const GURL& page_url,
                                     favicon_base::IconType icon_type);

  // EXPECTs that each URL-specific history thing (basically, everything but
  // favicons) is gone, the reason being either that it was automatically
  // |expired|, or manually deleted.
  void EnsureURLInfoGone(const URLRow& row, bool expired);

  // Returns whether HistoryBackendNotifier::NotifyURLsModified was
  // called for |url|.
  bool ModifiedNotificationSent(const GURL& url);

  // Clears the list of notifications received.
  void ClearLastNotifications() {
    urls_modified_notifications_.clear();
    urls_deleted_notifications_.clear();
  }

  void StarURL(const GURL& url) { history_client_.AddBookmark(url); }

  static bool IsStringInFile(const base::FilePath& filename, const char* str);

  // Returns the path the db files are created in.
  const base::FilePath& path() const { return tmp_dir_.GetPath(); }

  // This must be destroyed last.
  base::ScopedTempDir tmp_dir_;

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  HistoryClientFakeBookmarks history_client_;
  std::unique_ptr<HistoryBackendClient> backend_client_;

  ExpireHistoryBackend expirer_;

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<HistoryDatabase> main_db_;
  std::unique_ptr<ThumbnailDatabase> thumb_db_;
  scoped_refptr<TopSitesImpl> top_sites_;

  // base::Time at the beginning of the test, so everybody agrees what "now" is.
  const base::Time now_;

  typedef std::vector<URLRows> URLsModifiedNotificationList;
  URLsModifiedNotificationList urls_modified_notifications_;

  typedef std::vector<std::pair<bool, URLRows>> URLsDeletedNotificationList;
  URLsDeletedNotificationList urls_deleted_notifications_;

 private:
  void SetUp() override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());

    base::FilePath history_name = path().Append(kHistoryFilename);
    main_db_.reset(new TestHistoryDatabase);
    if (main_db_->Init(history_name) != sql::INIT_OK)
      main_db_.reset();

    base::FilePath thumb_name = path().Append(kFaviconsFilename);
    thumb_db_.reset(new ThumbnailDatabase(nullptr));
    if (thumb_db_->Init(thumb_name) != sql::INIT_OK)
      thumb_db_.reset();

    pref_service_.reset(new TestingPrefServiceSimple);
    TopSitesImpl::RegisterPrefs(pref_service_->registry());

    expirer_.SetDatabases(main_db_.get(), thumb_db_.get());
    top_sites_ = new TopSitesImpl(pref_service_.get(), nullptr,
                                  PrepopulatedPageList(),
                                  base::Bind(MockCanAddURLToHistory));
    WaitTopSitesLoadedObserver wait_top_sites_observer(top_sites_);
    top_sites_->Init(path().Append(kTopSitesFilename));
    wait_top_sites_observer.Run();
  }

  void TearDown() override {
    ClearLastNotifications();

    expirer_.SetDatabases(nullptr, nullptr);

    main_db_.reset();
    thumb_db_.reset();

    top_sites_->ShutdownOnUIThread();
    top_sites_ = nullptr;

    if (base::MessageLoop::current())
      base::RunLoop().RunUntilIdle();

    pref_service_.reset();
  }

  // HistoryBackendNotifier:
  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) override {}
  void NotifyURLVisited(ui::PageTransition transition,
                        const URLRow& row,
                        const RedirectList& redirects,
                        base::Time visit_time) override {}
  void NotifyURLsModified(const URLRows& rows) override {
    urls_modified_notifications_.push_back(rows);
  }
  void NotifyURLsDeleted(bool all_history,
                         bool expired,
                         const URLRows& rows,
                         const std::set<GURL>& favicon_urls) override {
    urls_deleted_notifications_.push_back(std::make_pair(expired, rows));
  }
};

// The example data consists of 4 visits. The middle two visits are to the
// same URL, while the first and last are for unique ones. This allows a test
// for the oldest or newest to include both a URL that should get totally
// deleted (the one on the end) with one that should only get a visit deleted
// (with the one in the middle) when it picks the proper threshold time.
//
// Each visit has indexed data, each URL has thumbnail. The first two URLs will
// share the same favicon, while the last one will have a unique favicon. The
// second visit for the middle URL is typed.
//
// The IDs of the added URLs, and the times of the four added visits will be
// added to the given arrays.
void ExpireHistoryTest::AddExampleData(URLID url_ids[3],
                                       base::Time visit_times[4]) {
  if (!main_db_.get())
    return;

  // Four times for each visit.
  visit_times[3] = base::Time::Now();
  visit_times[2] = visit_times[3] - base::TimeDelta::FromDays(1);
  visit_times[1] = visit_times[3] - base::TimeDelta::FromDays(2);
  visit_times[0] = visit_times[3] - base::TimeDelta::FromDays(3);

  // Two favicons. The first two URLs will share the same one, while the last
  // one will have a unique favicon.
  favicon_base::FaviconID favicon1 =
      thumb_db_->AddFavicon(GURL("http://favicon/url1"), favicon_base::FAVICON);
  favicon_base::FaviconID favicon2 =
      thumb_db_->AddFavicon(GURL("http://favicon/url2"), favicon_base::FAVICON);

  // Three URLs.
  URLRow url_row1(GURL("http://www.google.com/1"));
  url_row1.set_last_visit(visit_times[0]);
  url_row1.set_visit_count(1);
  url_ids[0] = main_db_->AddURL(url_row1);
  thumb_db_->AddIconMapping(url_row1.url(), favicon1);

  URLRow url_row2(GURL("http://www.google.com/2"));
  url_row2.set_last_visit(visit_times[2]);
  url_row2.set_visit_count(2);
  url_row2.set_typed_count(1);
  url_ids[1] = main_db_->AddURL(url_row2);
  thumb_db_->AddIconMapping(url_row2.url(), favicon1);

  URLRow url_row3(GURL("http://www.google.com/3"));
  url_row3.set_last_visit(visit_times[3]);
  url_row3.set_visit_count(1);
  url_ids[2] = main_db_->AddURL(url_row3);
  thumb_db_->AddIconMapping(url_row3.url(), favicon2);

  // Thumbnails for each URL.
  gfx::Image thumbnail = CreateGoogleThumbnailForTest();
  ThumbnailScore score(0.25, true, true, base::Time::Now());

  base::Time time;
  GURL gurl;
  top_sites_->SetPageThumbnail(url_row1.url(), thumbnail, score);
  top_sites_->SetPageThumbnail(url_row2.url(), thumbnail, score);
  top_sites_->SetPageThumbnail(url_row3.url(), thumbnail, score);

  // Four visits.
  VisitRow visit_row1;
  visit_row1.url_id = url_ids[0];
  visit_row1.visit_time = visit_times[0];
  main_db_->AddVisit(&visit_row1, SOURCE_BROWSED);

  VisitRow visit_row2;
  visit_row2.url_id = url_ids[1];
  visit_row2.visit_time = visit_times[1];
  main_db_->AddVisit(&visit_row2, SOURCE_BROWSED);

  VisitRow visit_row3;
  visit_row3.url_id = url_ids[1];
  visit_row3.visit_time = visit_times[2];
  visit_row3.transition = ui::PAGE_TRANSITION_TYPED;
  main_db_->AddVisit(&visit_row3, SOURCE_BROWSED);

  VisitRow visit_row4;
  visit_row4.url_id = url_ids[2];
  visit_row4.visit_time = visit_times[3];
  main_db_->AddVisit(&visit_row4, SOURCE_BROWSED);
}

void ExpireHistoryTest::AddExampleSourceData(const GURL& url, URLID* id) {
  if (!main_db_)
    return;

  base::Time last_visit_time = base::Time::Now();
  // Add one URL.
  URLRow url_row1(url);
  url_row1.set_last_visit(last_visit_time);
  url_row1.set_visit_count(4);
  URLID url_id = main_db_->AddURL(url_row1);
  *id = url_id;

  // Four times for each visit.
  VisitRow visit_row1(url_id, last_visit_time - base::TimeDelta::FromDays(4), 0,
                      ui::PAGE_TRANSITION_TYPED, 0);
  main_db_->AddVisit(&visit_row1, SOURCE_SYNCED);

  VisitRow visit_row2(url_id, last_visit_time - base::TimeDelta::FromDays(3), 0,
                      ui::PAGE_TRANSITION_TYPED, 0);
  main_db_->AddVisit(&visit_row2, SOURCE_BROWSED);

  VisitRow visit_row3(url_id, last_visit_time - base::TimeDelta::FromDays(2), 0,
                      ui::PAGE_TRANSITION_TYPED, 0);
  main_db_->AddVisit(&visit_row3, SOURCE_EXTENSION);

  VisitRow visit_row4(url_id, last_visit_time, 0, ui::PAGE_TRANSITION_TYPED, 0);
  main_db_->AddVisit(&visit_row4, SOURCE_FIREFOX_IMPORTED);
}

bool ExpireHistoryTest::HasFavicon(favicon_base::FaviconID favicon_id) {
  if (!thumb_db_.get() || favicon_id == 0)
    return false;
  return thumb_db_->GetFaviconHeader(favicon_id, nullptr, nullptr);
}

favicon_base::FaviconID ExpireHistoryTest::GetFavicon(
    const GURL& page_url,
    favicon_base::IconType icon_type) {
  std::vector<IconMapping> icon_mappings;
  if (thumb_db_->GetIconMappingsForPageURL(page_url, icon_type,
                                           &icon_mappings)) {
    return icon_mappings[0].icon_id;
  }
  return 0;
}

bool ExpireHistoryTest::HasThumbnail(URLID url_id) {
  // TODO(sky): fix this. This test isn't really valid for TopSites. For
  // TopSites we should be checking URL always, not the id.
  URLRow info;
  if (!main_db_->GetURLRow(url_id, &info))
    return false;
  GURL url = info.url();
  scoped_refptr<base::RefCountedMemory> data;
  return top_sites_->GetPageThumbnail(url, false, &data);
}

void ExpireHistoryTest::EnsureURLInfoGone(const URLRow& row, bool expired) {
  // The passed in |row| must originate from |main_db_| so that its ID will be
  // set to what had been in effect in |main_db_| before the deletion.
  ASSERT_NE(0, row.id());

  // Verify the URL no longer exists.
  URLRow temp_row;
  EXPECT_FALSE(main_db_->GetURLRow(row.id(), &temp_row));

  // There should be no visits.
  VisitVector visits;
  main_db_->GetVisitsForURL(row.id(), &visits);
  EXPECT_EQ(0U, visits.size());

  // Thumbnail should be gone.
  // TODO(sky): fix this, see comment in HasThumbnail.
  // EXPECT_FALSE(HasThumbnail(row.id()));

  bool found_delete_notification = false;
  for (const auto& pair : urls_deleted_notifications_) {
    EXPECT_EQ(expired, pair.first);
    const history::URLRows& rows(pair.second);
    history::URLRows::const_iterator it_row = std::find_if(
        rows.begin(), rows.end(), history::URLRow::URLRowHasURL(row.url()));
    if (it_row != rows.end()) {
      // Further verify that the ID is set to what had been in effect in the
      // main database before the deletion. The InMemoryHistoryBackend relies
      // on this to delete its cached copy of the row.
      EXPECT_EQ(row.id(), it_row->id());
      found_delete_notification = true;
    }
  }
  for (const auto& rows : urls_modified_notifications_) {
    EXPECT_TRUE(std::find_if(rows.begin(), rows.end(),
                             history::URLRow::URLRowHasURL(row.url())) ==
                rows.end());
  }
  EXPECT_TRUE(found_delete_notification);
}

bool ExpireHistoryTest::ModifiedNotificationSent(const GURL& url) {
  for (const auto& rows : urls_modified_notifications_) {
    if (std::find_if(rows.begin(), rows.end(),
                     history::URLRow::URLRowHasURL(url)) != rows.end())
      return true;
  }
  return false;
}

TEST_F(ExpireHistoryTest, DeleteFaviconsIfPossible) {
  // Add a favicon record.
  const GURL favicon_url("http://www.google.com/favicon.ico");
  favicon_base::FaviconID icon_id =
      thumb_db_->AddFavicon(favicon_url, favicon_base::FAVICON);
  EXPECT_TRUE(icon_id);
  EXPECT_TRUE(HasFavicon(icon_id));

  // The favicon should be deletable with no users.
  {
    ExpireHistoryBackend::DeleteEffects effects;
    effects.affected_favicons.insert(icon_id);
    expirer_.DeleteFaviconsIfPossible(&effects);
    EXPECT_FALSE(HasFavicon(icon_id));
    EXPECT_EQ(1U, effects.deleted_favicons.size());
    EXPECT_EQ(1U, effects.deleted_favicons.count(favicon_url));
  }

  // Add back the favicon.
  icon_id = thumb_db_->AddFavicon(favicon_url, favicon_base::TOUCH_ICON);
  EXPECT_TRUE(icon_id);
  EXPECT_TRUE(HasFavicon(icon_id));

  // Add a page that references the favicon.
  URLRow row(GURL("http://www.google.com/2"));
  row.set_visit_count(1);
  EXPECT_TRUE(main_db_->AddURL(row));
  thumb_db_->AddIconMapping(row.url(), icon_id);

  // Favicon should not be deletable.
  {
    ExpireHistoryBackend::DeleteEffects effects;
    effects.affected_favicons.insert(icon_id);
    expirer_.DeleteFaviconsIfPossible(&effects);
    EXPECT_TRUE(HasFavicon(icon_id));
    EXPECT_TRUE(effects.deleted_favicons.empty());
  }
}

// static
bool ExpireHistoryTest::IsStringInFile(const base::FilePath& filename,
                                       const char* str) {
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(filename, &contents));
  return contents.find(str) != std::string::npos;
}

// Deletes a URL with a favicon that it is the last referencer of, so that it
// should also get deleted.
// Fails near end of month. http://crbug.com/43586
TEST_F(ExpireHistoryTest, DISABLED_DeleteURLAndFavicon) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  // Verify things are the way we expect with a URL row, favicon, thumbnail.
  URLRow last_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &last_row));
  favicon_base::FaviconID favicon_id =
      GetFavicon(last_row.url(), favicon_base::FAVICON);
  EXPECT_TRUE(HasFavicon(favicon_id));
  // TODO(sky): fix this, see comment in HasThumbnail.
  // EXPECT_TRUE(HasThumbnail(url_ids[2]));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[2], &visits);
  ASSERT_EQ(1U, visits.size());

  // Delete the URL and its dependencies.
  expirer_.DeleteURL(last_row.url());

  // All the normal data + the favicon should be gone.
  EnsureURLInfoGone(last_row, false);
  EXPECT_FALSE(GetFavicon(last_row.url(), favicon_base::FAVICON));
  EXPECT_FALSE(HasFavicon(favicon_id));
}

// Deletes a URL with a favicon that other URLs reference, so that the favicon
// should not get deleted. This also tests deleting more than one visit.
TEST_F(ExpireHistoryTest, DeleteURLWithoutFavicon) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  // Verify things are the way we expect with a URL row, favicon, thumbnail.
  URLRow last_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &last_row));
  favicon_base::FaviconID favicon_id =
      GetFavicon(last_row.url(), favicon_base::FAVICON);
  EXPECT_TRUE(HasFavicon(favicon_id));
  // TODO(sky): fix this, see comment in HasThumbnail.
  // EXPECT_TRUE(HasThumbnail(url_ids[1]));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[1], &visits);
  EXPECT_EQ(2U, visits.size());

  // Delete the URL and its dependencies.
  expirer_.DeleteURL(last_row.url());

  // All the normal data except the favicon should be gone.
  EnsureURLInfoGone(last_row, false);
  EXPECT_TRUE(HasFavicon(favicon_id));
}

// DeleteURL should delete the history of starred urls, but the URL should
// remain starred and its favicon should remain too.
TEST_F(ExpireHistoryTest, DeleteStarredVisitedURL) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row));

  // Star the last URL.
  StarURL(url_row.url());

  // Attempt to delete the url.
  expirer_.DeleteURL(url_row.url());

  // Verify it no longer exists.
  GURL url = url_row.url();
  ASSERT_FALSE(main_db_->GetRowForURL(url, &url_row));
  EnsureURLInfoGone(url_row, false);

  // Yet the favicon should exist.
  favicon_base::FaviconID favicon_id = GetFavicon(url, favicon_base::FAVICON);
  EXPECT_TRUE(HasFavicon(favicon_id));

  // Should still have the thumbnail.
  // TODO(sky): fix this, see comment in HasThumbnail.
  // ASSERT_TRUE(HasThumbnail(url_row.id()));
}

// DeleteURL should not delete the favicon of bookmarked URLs.
TEST_F(ExpireHistoryTest, DeleteStarredUnvisitedURL) {
  // Create a bookmark associated with a favicon.
  const GURL url("http://www.google.com/starred");
  favicon_base::FaviconID favicon =
      thumb_db_->AddFavicon(GURL("http://favicon/url1"), favicon_base::FAVICON);
  thumb_db_->AddIconMapping(url, favicon);
  StarURL(url);

  // Delete it.
  expirer_.DeleteURL(url);

  // The favicon should exist.
  favicon_base::FaviconID favicon_id = GetFavicon(url, favicon_base::FAVICON);
  EXPECT_TRUE(HasFavicon(favicon_id));

  // Unstar the URL and try again to delete it.
  history_client_.ClearAllBookmarks();
  expirer_.DeleteURL(url);

  // The favicon should be gone.
  favicon_id = GetFavicon(url, favicon_base::FAVICON);
  EXPECT_FALSE(HasFavicon(favicon_id));
}

// Deletes multiple URLs at once.  The favicon for the third one but
// not the first two should be deleted.
TEST_F(ExpireHistoryTest, DeleteURLs) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  // Verify things are the way we expect with URL rows, favicons,
  // thumbnails.
  URLRow rows[3];
  favicon_base::FaviconID favicon_ids[3];
  std::vector<GURL> urls;
  // Push back a bogus URL (which shouldn't change anything).
  urls.push_back(GURL());
  for (size_t i = 0; i < arraysize(rows); ++i) {
    ASSERT_TRUE(main_db_->GetURLRow(url_ids[i], &rows[i]));
    favicon_ids[i] = GetFavicon(rows[i].url(), favicon_base::FAVICON);
    EXPECT_TRUE(HasFavicon(favicon_ids[i]));
    // TODO(sky): fix this, see comment in HasThumbnail.
    // EXPECT_TRUE(HasThumbnail(url_ids[i]));
    urls.push_back(rows[i].url());
  }

  StarURL(rows[0].url());

  // Delete the URLs and their dependencies.
  expirer_.DeleteURLs(urls);

  EnsureURLInfoGone(rows[0], false);
  EnsureURLInfoGone(rows[1], false);
  EnsureURLInfoGone(rows[2], false);
  EXPECT_TRUE(HasFavicon(favicon_ids[0]));
  EXPECT_TRUE(HasFavicon(favicon_ids[1]));
  EXPECT_FALSE(HasFavicon(favicon_ids[2]));
}

// Expires all URLs more recent than a given time, with no starred items.
// Our time threshold is such that one URL should be updated (we delete one of
// the two visits) and one is deleted.
TEST_F(ExpireHistoryTest, FlushRecentURLsUnstarred) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row1, url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row2));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[2], &visits);
  ASSERT_EQ(1U, visits.size());

  // This should delete the last two visits.
  std::set<GURL> restrict_urls;
  expirer_.ExpireHistoryBetween(restrict_urls, visit_times[2], base::Time());

  // Verify that the middle URL had its last visit deleted only.
  visits.clear();
  main_db_->GetVisitsForURL(url_ids[1], &visits);
  EXPECT_EQ(1U, visits.size());

  // Verify that the middle URL visit time and visit counts were updated.
  EXPECT_TRUE(ModifiedNotificationSent(url_row1.url()));
  URLRow temp_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &temp_row));
  EXPECT_TRUE(visit_times[2] == url_row1.last_visit());  // Previous value.
  EXPECT_TRUE(visit_times[1] == temp_row.last_visit());  // New value.
  EXPECT_EQ(2, url_row1.visit_count());
  EXPECT_EQ(1, temp_row.visit_count());
  EXPECT_EQ(1, url_row1.typed_count());
  EXPECT_EQ(0, temp_row.typed_count());

  // Verify that the middle URL's favicon and thumbnail is still there.
  favicon_base::FaviconID favicon_id =
      GetFavicon(url_row1.url(), favicon_base::FAVICON);
  EXPECT_TRUE(HasFavicon(favicon_id));
  // TODO(sky): fix this, see comment in HasThumbnail.
  // EXPECT_TRUE(HasThumbnail(url_row1.id()));

  // Verify that the last URL was deleted.
  favicon_base::FaviconID favicon_id2 =
      GetFavicon(url_row2.url(), favicon_base::FAVICON);
  EnsureURLInfoGone(url_row2, false);
  EXPECT_FALSE(HasFavicon(favicon_id2));
}

// Expires all URLs with times in a given set.
TEST_F(ExpireHistoryTest, FlushURLsForTimes) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row1, url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row2));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[2], &visits);
  ASSERT_EQ(1U, visits.size());

  // This should delete the last two visits.
  std::vector<base::Time> times;
  times.push_back(visit_times[3]);
  times.push_back(visit_times[2]);
  expirer_.ExpireHistoryForTimes(times);

  // Verify that the middle URL had its last visit deleted only.
  visits.clear();
  main_db_->GetVisitsForURL(url_ids[1], &visits);
  EXPECT_EQ(1U, visits.size());

  // Verify that the middle URL visit time and visit counts were updated.
  EXPECT_TRUE(ModifiedNotificationSent(url_row1.url()));
  URLRow temp_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &temp_row));
  EXPECT_TRUE(visit_times[2] == url_row1.last_visit());  // Previous value.
  EXPECT_TRUE(visit_times[1] == temp_row.last_visit());  // New value.
  EXPECT_EQ(2, url_row1.visit_count());
  EXPECT_EQ(1, temp_row.visit_count());
  EXPECT_EQ(1, url_row1.typed_count());
  EXPECT_EQ(0, temp_row.typed_count());

  // Verify that the middle URL's favicon and thumbnail is still there.
  favicon_base::FaviconID favicon_id =
      GetFavicon(url_row1.url(), favicon_base::FAVICON);
  EXPECT_TRUE(HasFavicon(favicon_id));
  // TODO(sky): fix this, see comment in HasThumbnail.
  // EXPECT_TRUE(HasThumbnail(url_row1.id()));

  // Verify that the last URL was deleted.
  favicon_base::FaviconID favicon_id2 =
      GetFavicon(url_row2.url(), favicon_base::FAVICON);
  EnsureURLInfoGone(url_row2, false);
  EXPECT_FALSE(HasFavicon(favicon_id2));
}

// Expires only a specific URLs more recent than a given time, with no starred
// items.  Our time threshold is such that the URL should be updated (we delete
// one of the two visits).
TEST_F(ExpireHistoryTest, FlushRecentURLsUnstarredRestricted) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row1, url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row2));

  VisitVector visits;
  main_db_->GetVisitsForURL(url_ids[2], &visits);
  ASSERT_EQ(1U, visits.size());

  // This should delete the last two visits.
  std::set<GURL> restrict_urls;
  restrict_urls.insert(url_row1.url());
  expirer_.ExpireHistoryBetween(restrict_urls, visit_times[2], base::Time());

  // Verify that the middle URL had its last visit deleted only.
  visits.clear();
  main_db_->GetVisitsForURL(url_ids[1], &visits);
  EXPECT_EQ(1U, visits.size());

  // Verify that the middle URL visit time and visit counts were updated.
  EXPECT_TRUE(ModifiedNotificationSent(url_row1.url()));
  URLRow temp_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &temp_row));
  EXPECT_TRUE(visit_times[2] == url_row1.last_visit());  // Previous value.
  EXPECT_TRUE(visit_times[1] == temp_row.last_visit());  // New value.
  EXPECT_EQ(2, url_row1.visit_count());
  EXPECT_EQ(1, temp_row.visit_count());
  EXPECT_EQ(1, url_row1.typed_count());
  EXPECT_EQ(0, temp_row.typed_count());

  // Verify that the middle URL's favicon and thumbnail is still there.
  favicon_base::FaviconID favicon_id =
      GetFavicon(url_row1.url(), favicon_base::FAVICON);
  EXPECT_TRUE(HasFavicon(favicon_id));
  // TODO(sky): fix this, see comment in HasThumbnail.
  // EXPECT_TRUE(HasThumbnail(url_row1.id()));

  // Verify that the last URL was not touched.
  EXPECT_TRUE(main_db_->GetURLRow(url_ids[2], &temp_row));
  EXPECT_TRUE(HasFavicon(favicon_id));
  // TODO(sky): fix this, see comment in HasThumbnail.
  // EXPECT_TRUE(HasThumbnail(url_row2.id()));
}

// Expire a starred URL, it shouldn't get deleted
TEST_F(ExpireHistoryTest, FlushRecentURLsStarred) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row1, url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row2));

  // Star the last two URLs.
  StarURL(url_row1.url());
  StarURL(url_row2.url());

  // This should delete the last two visits.
  std::set<GURL> restrict_urls;
  expirer_.ExpireHistoryBetween(restrict_urls, visit_times[2], base::Time());

  // The URL rows should still exist.
  URLRow new_url_row1, new_url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &new_url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &new_url_row2));

  // The visit times should be updated.
  EXPECT_TRUE(new_url_row1.last_visit() == visit_times[1]);
  EXPECT_TRUE(new_url_row2.last_visit().is_null());  // No last visit time.

  // Visit/typed count should be updated.
  EXPECT_TRUE(ModifiedNotificationSent(url_row1.url()));
  EXPECT_TRUE(ModifiedNotificationSent(url_row2.url()));
  EXPECT_EQ(0, new_url_row1.typed_count());
  EXPECT_EQ(1, new_url_row1.visit_count());
  EXPECT_EQ(0, new_url_row2.typed_count());
  EXPECT_EQ(0, new_url_row2.visit_count());

  // Thumbnails and favicons should still exist. Note that we keep thumbnails
  // that may have been updated since the time threshold. Since the URL still
  // exists in history, this should not be a privacy problem, we only update
  // the visit counts in this case for consistency anyway.
  favicon_base::FaviconID favicon_id =
      GetFavicon(url_row1.url(), favicon_base::FAVICON);
  EXPECT_TRUE(HasFavicon(favicon_id));
  // TODO(sky): fix this, see comment in HasThumbnail.
  // EXPECT_TRUE(HasThumbnail(new_url_row1.id()));
  favicon_id = GetFavicon(url_row1.url(), favicon_base::FAVICON);
  EXPECT_TRUE(HasFavicon(favicon_id));
  // TODO(sky): fix this, see comment in HasThumbnail.
  // EXPECT_TRUE(HasThumbnail(new_url_row2.id()));
}

TEST_F(ExpireHistoryTest, ExpireHistoryBeforeUnstarred) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row0, url_row1, url_row2;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[0], &url_row0));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[2], &url_row2));

  // Expire the oldest two visits.
  expirer_.ExpireHistoryBefore(visit_times[1]);

  // The first URL should be deleted along with its sole visit. The second URL
  // itself should not be affected, as there is still one more visit to it, but
  // its first visit should be deleted.
  URLRow temp_row;
  EnsureURLInfoGone(url_row0, true);
  EXPECT_TRUE(main_db_->GetURLRow(url_ids[1], &temp_row));
  EXPECT_TRUE(ModifiedNotificationSent(url_row1.url()));
  VisitVector visits;
  main_db_->GetVisitsForURL(temp_row.id(), &visits);
  EXPECT_EQ(1U, visits.size());
  EXPECT_EQ(visit_times[2], visits[0].visit_time);
  EXPECT_TRUE(main_db_->GetURLRow(url_ids[2], &temp_row));

  // Now expire one more visit so that the second URL should be removed. The
  // third URL and its visit should be intact.
  ClearLastNotifications();
  expirer_.ExpireHistoryBefore(visit_times[2]);
  EnsureURLInfoGone(url_row1, true);
  EXPECT_TRUE(main_db_->GetURLRow(url_ids[2], &temp_row));
  main_db_->GetVisitsForURL(temp_row.id(), &visits);
  EXPECT_EQ(1U, visits.size());
}

TEST_F(ExpireHistoryTest, ExpireHistoryBeforeStarred) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  URLRow url_row0, url_row1;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[0], &url_row0));
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &url_row1));

  // Star the URLs.
  StarURL(url_row0.url());
  StarURL(url_row1.url());

  // Now expire the first three visits (first two URLs). The first three visits
  // should be deleted, but the URL records themselves should not, as they are
  // starred.
  expirer_.ExpireHistoryBefore(visit_times[2]);

  URLRow temp_row;
  ASSERT_TRUE(main_db_->GetURLRow(url_ids[0], &temp_row));
  EXPECT_TRUE(ModifiedNotificationSent(url_row0.url()));
  VisitVector visits;
  main_db_->GetVisitsForURL(temp_row.id(), &visits);
  EXPECT_EQ(0U, visits.size());

  ASSERT_TRUE(main_db_->GetURLRow(url_ids[1], &temp_row));
  EXPECT_TRUE(ModifiedNotificationSent(url_row1.url()));
  main_db_->GetVisitsForURL(temp_row.id(), &visits);
  EXPECT_EQ(0U, visits.size());

  // The third URL should be unchanged.
  EXPECT_TRUE(main_db_->GetURLRow(url_ids[2], &temp_row));
  EXPECT_FALSE(ModifiedNotificationSent(temp_row.url()));
  main_db_->GetVisitsForURL(temp_row.id(), &visits);
  EXPECT_EQ(1U, visits.size());
}

// Tests the return values from ExpireSomeOldHistory. The rest of the
// functionality of this function is tested by the ExpireHistoryBefore*
// tests which use this function internally.
TEST_F(ExpireHistoryTest, ExpireSomeOldHistory) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);
  const ExpiringVisitsReader* reader = expirer_.GetAllVisitsReader();

  // Deleting a time range with no URLs should return false (nothing found).
  EXPECT_FALSE(expirer_.ExpireSomeOldHistory(
      visit_times[0] - base::TimeDelta::FromDays(100), reader, 1));

  // Deleting a time range with not up the the max results should also return
  // false (there will only be one visit deleted in this range).
  EXPECT_FALSE(expirer_.ExpireSomeOldHistory(visit_times[0], reader, 2));

  // Deleting a time range with the max number of results should return true
  // (max deleted).
  EXPECT_TRUE(expirer_.ExpireSomeOldHistory(visit_times[2], reader, 1));
}

TEST_F(ExpireHistoryTest, ExpiringVisitsReader) {
  URLID url_ids[3];
  base::Time visit_times[4];
  AddExampleData(url_ids, visit_times);

  const ExpiringVisitsReader* all = expirer_.GetAllVisitsReader();
  const ExpiringVisitsReader* auto_subframes =
      expirer_.GetAutoSubframeVisitsReader();

  VisitVector visits;
  base::Time now = base::Time::Now();

  // Verify that the early expiration threshold, stored in the meta table is
  // initialized.
  EXPECT_TRUE(main_db_->GetEarlyExpirationThreshold() ==
              base::Time::FromInternalValue(1L));

  // First, attempt reading AUTO_SUBFRAME visits. We should get none.
  EXPECT_FALSE(auto_subframes->Read(now, main_db_.get(), &visits, 1));
  EXPECT_EQ(0U, visits.size());

  // Verify that the early expiration threshold was updated, since there are no
  // AUTO_SUBFRAME visits in the given time range.
  EXPECT_TRUE(now <= main_db_->GetEarlyExpirationThreshold());

  // Now, read all visits and verify that there's at least one.
  EXPECT_TRUE(all->Read(now, main_db_.get(), &visits, 1));
  EXPECT_EQ(1U, visits.size());
}

// Test that ClearOldOnDemandFavicons() deletes favicons associated only to
// unstarred page URLs.
TEST_F(ExpireHistoryTest, ClearOldOnDemandFaviconsDoesDeleteUnstarred) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(internal::kClearOldOnDemandFavicons);

  // The blob does not encode any real bitmap, obviously.
  const unsigned char kBlob[] = "0";
  scoped_refptr<base::RefCountedBytes> favicon(
      new base::RefCountedBytes(kBlob, sizeof(kBlob)));

  // Icon: old and not bookmarked case.
  GURL url("http://google.com/favicon.ico");
  favicon_base::FaviconID icon_id = thumb_db_->AddFavicon(
      url, favicon_base::FAVICON, favicon, FaviconBitmapType::ON_DEMAND,
      base::Time::Now() - base::TimeDelta::FromDays(100), gfx::Size());
  ASSERT_NE(0, icon_id);
  GURL page_url("http://google.com/");
  ASSERT_NE(0, thumb_db_->AddIconMapping(page_url, icon_id));

  expirer_.ClearOldOnDemandFavicons(base::Time::Now() -
                                    base::TimeDelta::FromDays(90));

  // The icon gets deleted.
  EXPECT_FALSE(thumb_db_->GetIconMappingsForPageURL(page_url, nullptr));
  EXPECT_FALSE(thumb_db_->GetFaviconHeader(icon_id, nullptr, nullptr));
  EXPECT_FALSE(thumb_db_->GetFaviconBitmaps(icon_id, nullptr));
}

// Test that ClearOldOnDemandFavicons() deletes favicons associated to at least
// one starred page URL.
TEST_F(ExpireHistoryTest, ClearOldOnDemandFaviconsDoesNotDeleteStarred) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(internal::kClearOldOnDemandFavicons);

  // The blob does not encode any real bitmap, obviously.
  const unsigned char kBlob[] = "0";
  scoped_refptr<base::RefCountedBytes> favicon(
      new base::RefCountedBytes(kBlob, sizeof(kBlob)));

  // Icon: old but bookmarked case.
  GURL url("http://google.com/favicon.ico");
  favicon_base::FaviconID icon_id = thumb_db_->AddFavicon(
      url, favicon_base::FAVICON, favicon, FaviconBitmapType::ON_DEMAND,
      base::Time::Now() - base::TimeDelta::FromDays(100), gfx::Size());
  ASSERT_NE(0, icon_id);
  GURL page_url1("http://google.com/1");
  ASSERT_NE(0, thumb_db_->AddIconMapping(page_url1, icon_id));
  StarURL(page_url1);
  GURL page_url2("http://google.com/2");
  ASSERT_NE(0, thumb_db_->AddIconMapping(page_url2, icon_id));

  expirer_.ClearOldOnDemandFavicons(base::Time::Now() -
                                    base::TimeDelta::FromDays(90));

  // Nothing gets deleted.
  EXPECT_TRUE(thumb_db_->GetFaviconHeader(icon_id, nullptr, nullptr));
  std::vector<FaviconBitmap> favicon_bitmaps;
  EXPECT_TRUE(thumb_db_->GetFaviconBitmaps(icon_id, &favicon_bitmaps));
  EXPECT_EQ(1u, favicon_bitmaps.size());
  std::vector<IconMapping> icon_mapping;
  EXPECT_TRUE(thumb_db_->GetIconMappingsForPageURL(page_url1, &icon_mapping));
  EXPECT_TRUE(thumb_db_->GetIconMappingsForPageURL(page_url2, &icon_mapping));
  EXPECT_EQ(2u, icon_mapping.size());
  EXPECT_EQ(icon_id, icon_mapping[0].icon_id);
  EXPECT_EQ(icon_id, icon_mapping[1].icon_id);
}

// Test that ClearOldOnDemandFavicons() has effect if the last clearing was long
// time age (such as 2 days ago).
TEST_F(ExpireHistoryTest, ClearOldOnDemandFaviconsDoesDeleteAfterLongDelay) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(internal::kClearOldOnDemandFavicons);

  // Previous clearing (2 days ago).
  expirer_.ClearOldOnDemandFavicons(base::Time::Now() -
                                    base::TimeDelta::FromDays(92));

  // The blob does not encode any real bitmap, obviously.
  const unsigned char kBlob[] = "0";
  scoped_refptr<base::RefCountedBytes> favicon(
      new base::RefCountedBytes(kBlob, sizeof(kBlob)));

  // Icon: old and not bookmarked case.
  GURL url("http://google.com/favicon.ico");
  favicon_base::FaviconID icon_id = thumb_db_->AddFavicon(
      url, favicon_base::FAVICON, favicon, FaviconBitmapType::ON_DEMAND,
      base::Time::Now() - base::TimeDelta::FromDays(100), gfx::Size());
  ASSERT_NE(0, icon_id);
  GURL page_url("http://google.com/");
  ASSERT_NE(0, thumb_db_->AddIconMapping(page_url, icon_id));

  expirer_.ClearOldOnDemandFavicons(base::Time::Now() -
                                    base::TimeDelta::FromDays(90));

  // The icon gets deleted.
  EXPECT_FALSE(thumb_db_->GetIconMappingsForPageURL(page_url, nullptr));
  EXPECT_FALSE(thumb_db_->GetFaviconHeader(icon_id, nullptr, nullptr));
  EXPECT_FALSE(thumb_db_->GetFaviconBitmaps(icon_id, nullptr));
}

// Test that ClearOldOnDemandFavicons() deletes favicons associated to at least
// one starred page URL.
TEST_F(ExpireHistoryTest,
       ClearOldOnDemandFaviconsDoesNotDeleteAfterShortDelay) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(internal::kClearOldOnDemandFavicons);

  // Previous clearing (5 minutes ago).
  expirer_.ClearOldOnDemandFavicons(base::Time::Now() -
                                    base::TimeDelta::FromDays(90) -
                                    base::TimeDelta::FromMinutes(5));

  // The blob does not encode any real bitmap, obviously.
  const unsigned char kBlob[] = "0";
  scoped_refptr<base::RefCountedBytes> favicon(
      new base::RefCountedBytes(kBlob, sizeof(kBlob)));

  // Icon: old but bookmarked case.
  GURL url("http://google.com/favicon.ico");
  favicon_base::FaviconID icon_id = thumb_db_->AddFavicon(
      url, favicon_base::FAVICON, favicon, FaviconBitmapType::ON_DEMAND,
      base::Time::Now() - base::TimeDelta::FromDays(100), gfx::Size());
  ASSERT_NE(0, icon_id);
  GURL page_url1("http://google.com/1");
  ASSERT_NE(0, thumb_db_->AddIconMapping(page_url1, icon_id));
  GURL page_url2("http://google.com/2");
  ASSERT_NE(0, thumb_db_->AddIconMapping(page_url2, icon_id));

  expirer_.ClearOldOnDemandFavicons(base::Time::Now() -
                                    base::TimeDelta::FromDays(90));

  // Nothing gets deleted.
  EXPECT_TRUE(thumb_db_->GetFaviconHeader(icon_id, nullptr, nullptr));
  std::vector<FaviconBitmap> favicon_bitmaps;
  EXPECT_TRUE(thumb_db_->GetFaviconBitmaps(icon_id, &favicon_bitmaps));
  EXPECT_EQ(1u, favicon_bitmaps.size());
  std::vector<IconMapping> icon_mapping;
  EXPECT_TRUE(thumb_db_->GetIconMappingsForPageURL(page_url1, &icon_mapping));
  EXPECT_TRUE(thumb_db_->GetIconMappingsForPageURL(page_url2, &icon_mapping));
  EXPECT_EQ(2u, icon_mapping.size());
  EXPECT_EQ(icon_id, icon_mapping[0].icon_id);
  EXPECT_EQ(icon_id, icon_mapping[1].icon_id);
}

// TODO(brettw) add some visits with no URL to make sure everything is updated
// properly. Have the visits also refer to nonexistent FTS rows.
//
// Maybe also refer to invalid favicons.

}  // namespace history
