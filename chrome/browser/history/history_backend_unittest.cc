// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/history/in_memory_history_backend.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/tools/profiles/thumbnail-inl.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/jpeg_codec.h"

using base::Time;

// This file only tests functionality where it is most convenient to call the
// backend directly. Most of the history backend functions are tested by the
// history unit test. Because of the elaborate callbacks involved, this is no
// harder than calling it directly for many things.

namespace {

// data we'll put into the thumbnail database
static const unsigned char blob1[] =
    "12346102356120394751634516591348710478123649165419234519234512349134";

}  // namepace

namespace history {

class HistoryBackendTest;

// This must be a separate object since HistoryBackend manages its lifetime.
// This just forwards the messages we're interested in to the test object.
class HistoryBackendTestDelegate : public HistoryBackend::Delegate {
 public:
  explicit HistoryBackendTestDelegate(HistoryBackendTest* test) : test_(test) {}

  virtual void NotifyProfileError(int message_id) {}
  virtual void SetInMemoryBackend(InMemoryHistoryBackend* backend);
  virtual void BroadcastNotifications(NotificationType type,
                                      HistoryDetails* details);
  virtual void DBLoaded();
  virtual void StartTopSitesMigration();

 private:
  // Not owned by us.
  HistoryBackendTest* test_;

  DISALLOW_COPY_AND_ASSIGN(HistoryBackendTestDelegate);
};

class HistoryBackendTest : public testing::Test {
 public:
  HistoryBackendTest() : bookmark_model_(NULL), loaded_(false) {}
  virtual ~HistoryBackendTest() {
  }

 protected:
  scoped_refptr<HistoryBackend> backend_;  // Will be NULL on init failure.
  scoped_ptr<InMemoryHistoryBackend> mem_backend_;

  void AddRedirectChain(const char* sequence[], int page_id) {
    history::RedirectList redirects;
    for (int i = 0; sequence[i] != NULL; ++i)
      redirects.push_back(GURL(sequence[i]));

    int int_scope = 1;
    void* scope = 0;
    memcpy(&scope, &int_scope, sizeof(int_scope));
    scoped_refptr<history::HistoryAddPageArgs> request(
        new history::HistoryAddPageArgs(
            redirects.back(), Time::Now(), scope, page_id, GURL(),
            redirects, PageTransition::LINK, history::SOURCE_BROWSED, true));
    backend_->AddPage(request);
  }

  // Adds CLIENT_REDIRECT page transition.
  // |url1| is the source URL and |url2| is the destination.
  // |did_replace| is true if the transition is non-user initiated and the
  // navigation entry for |url2| has replaced that for |url1|. The possibly
  // updated transition code of the visit records for |url1| and |url2| is
  // returned by filling in |*transition1| and |*transition2|, respectively.
  void  AddClientRedirect(const GURL& url1, const GURL& url2, bool did_replace,
                          int* transition1, int* transition2) {
    void* const dummy_scope = reinterpret_cast<void*>(0x87654321);
    history::RedirectList redirects;
    if (url1.is_valid())
      redirects.push_back(url1);
    if (url2.is_valid())
      redirects.push_back(url2);
    scoped_refptr<HistoryAddPageArgs> request(
        new HistoryAddPageArgs(url2, base::Time(), dummy_scope, 0, url1,
            redirects, PageTransition::CLIENT_REDIRECT,
            history::SOURCE_BROWSED, did_replace));
    backend_->AddPage(request);

    *transition1 = getTransition(url1);
    *transition2 = getTransition(url2);
  }

  int getTransition(const GURL& url) {
    if (!url.is_valid())
      return 0;
    URLRow row;
    URLID id = backend_->db()->GetRowForURL(url, &row);
    VisitVector visits;
    EXPECT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
    return visits[0].transition;
  }

  FilePath getTestDir() {
    return test_dir_;
  }

  FavIconID GetFavicon(const GURL& url, IconType icon_type) {
    IconMapping icon_mapping;
    if (backend_->thumbnail_db_->GetIconMappingForPageURL(url, icon_type,
                                                          &icon_mapping))
      return icon_mapping.icon_id;
    else
      return 0;
  }

  BookmarkModel bookmark_model_;

 protected:
  bool loaded_;

 private:
  friend class HistoryBackendTestDelegate;

  // testing::Test
  virtual void SetUp() {
    if (!file_util::CreateNewTempDirectory(FILE_PATH_LITERAL("BackendTest"),
                                           &test_dir_))
      return;
    backend_ = new HistoryBackend(test_dir_,
                                  new HistoryBackendTestDelegate(this),
                                  &bookmark_model_);
    backend_->Init(std::string(), false);
  }
  virtual void TearDown() {
    if (backend_.get())
      backend_->Closing();
    backend_ = NULL;
    mem_backend_.reset();
    file_util::Delete(test_dir_, true);
  }

  void SetInMemoryBackend(InMemoryHistoryBackend* backend) {
    mem_backend_.reset(backend);
  }

  void BroadcastNotifications(NotificationType type,
                                      HistoryDetails* details) {
    // Send the notifications directly to the in-memory database.
    Details<HistoryDetails> det(details);
    mem_backend_->Observe(type, Source<HistoryBackendTest>(NULL), det);

    // The backend passes ownership of the details pointer to us.
    delete details;
  }

  MessageLoop message_loop_;
  FilePath test_dir_;
};

void HistoryBackendTestDelegate::SetInMemoryBackend(
    InMemoryHistoryBackend* backend) {
  test_->SetInMemoryBackend(backend);
}

void HistoryBackendTestDelegate::BroadcastNotifications(
    NotificationType type,
    HistoryDetails* details) {
  test_->BroadcastNotifications(type, details);
}

void HistoryBackendTestDelegate::DBLoaded() {
  test_->loaded_ = true;
}

void HistoryBackendTestDelegate::StartTopSitesMigration() {
  test_->backend_->MigrateThumbnailsDatabase();
}

TEST_F(HistoryBackendTest, Loaded) {
  ASSERT_TRUE(backend_.get());
  ASSERT_TRUE(loaded_);
}

TEST_F(HistoryBackendTest, DeleteAll) {
  ASSERT_TRUE(backend_.get());

  // Add two favicons, use the characters '1' and '2' for the image data. Note
  // that we do these in the opposite order. This is so the first one gets ID
  // 2 autoassigned to the database, which will change when the other one is
  // deleted. This way we can test that updating works properly.
  GURL favicon_url1("http://www.google.com/favicon.ico");
  GURL favicon_url2("http://news.google.com/favicon.ico");
  FavIconID favicon2 = backend_->thumbnail_db_->AddFavIcon(favicon_url2,
                                                           FAVICON);
  FavIconID favicon1 = backend_->thumbnail_db_->AddFavIcon(favicon_url1,
                                                           FAVICON);

  std::vector<unsigned char> data;
  data.push_back('1');
  EXPECT_TRUE(backend_->thumbnail_db_->SetFavicon(favicon1,
      new RefCountedBytes(data), Time::Now()));

  data[0] = '2';
  EXPECT_TRUE(backend_->thumbnail_db_->SetFavicon(
                  favicon2, new RefCountedBytes(data), Time::Now()));

  // First visit two URLs.
  URLRow row1(GURL("http://www.google.com/"));
  row1.set_visit_count(2);
  row1.set_typed_count(1);
  row1.set_last_visit(Time::Now());
  backend_->thumbnail_db_->AddIconMapping(row1.url(), favicon1);

  URLRow row2(GURL("http://news.google.com/"));
  row2.set_visit_count(1);
  row2.set_last_visit(Time::Now());
  backend_->thumbnail_db_->AddIconMapping(row2.url(), favicon2);

  std::vector<URLRow> rows;
  rows.push_back(row2);  // Reversed order for the same reason as favicons.
  rows.push_back(row1);
  backend_->AddPagesWithDetails(rows, history::SOURCE_BROWSED);

  URLID row1_id = backend_->db_->GetRowForURL(row1.url(), NULL);
  URLID row2_id = backend_->db_->GetRowForURL(row2.url(), NULL);

  // Get the two visits for the URLs we just added.
  VisitVector visits;
  backend_->db_->GetVisitsForURL(row1_id, &visits);
  ASSERT_EQ(1U, visits.size());
  VisitID visit1_id = visits[0].visit_id;

  visits.clear();
  backend_->db_->GetVisitsForURL(row2_id, &visits);
  ASSERT_EQ(1U, visits.size());
  VisitID visit2_id = visits[0].visit_id;

  // The in-memory backend should have been set and it should have gotten the
  // typed URL.
  ASSERT_TRUE(mem_backend_.get());
  URLRow outrow1;
  EXPECT_TRUE(mem_backend_->db_->GetRowForURL(row1.url(), NULL));

  // Add thumbnails for each page.
  ThumbnailScore score(0.25, true, true);
  scoped_ptr<SkBitmap> google_bitmap(
      gfx::JPEGCodec::Decode(kGoogleThumbnail, sizeof(kGoogleThumbnail)));

  Time time;
  GURL gurl;
  backend_->thumbnail_db_->SetPageThumbnail(gurl, row1_id, *google_bitmap,
                                            score, time);
  scoped_ptr<SkBitmap> weewar_bitmap(
     gfx::JPEGCodec::Decode(kWeewarThumbnail, sizeof(kWeewarThumbnail)));
  backend_->thumbnail_db_->SetPageThumbnail(gurl, row2_id, *weewar_bitmap,
                                            score, time);

  // Star row1.
  bookmark_model_.AddURL(
      bookmark_model_.GetBookmarkBarNode(), 0, string16(), row1.url());

  // Set full text index for each one.
  backend_->text_database_->AddPageData(row1.url(), row1_id, visit1_id,
                                        row1.last_visit(),
                                        UTF8ToUTF16("Title 1"),
                                        UTF8ToUTF16("Body 1"));
  backend_->text_database_->AddPageData(row2.url(), row2_id, visit2_id,
                                        row2.last_visit(),
                                        UTF8ToUTF16("Title 2"),
                                        UTF8ToUTF16("Body 2"));

  // Now finally clear all history.
  backend_->DeleteAllHistory();

  // The first URL should be preserved but the time should be cleared.
  EXPECT_TRUE(backend_->db_->GetRowForURL(row1.url(), &outrow1));
  EXPECT_EQ(row1.url(), outrow1.url());
  EXPECT_EQ(0, outrow1.visit_count());
  EXPECT_EQ(0, outrow1.typed_count());
  EXPECT_TRUE(Time() == outrow1.last_visit());

  // The second row should be deleted.
  URLRow outrow2;
  EXPECT_FALSE(backend_->db_->GetRowForURL(row2.url(), &outrow2));

  // All visits should be deleted for both URLs.
  VisitVector all_visits;
  backend_->db_->GetAllVisitsInRange(Time(), Time(), 0, &all_visits);
  ASSERT_EQ(0U, all_visits.size());

  // All thumbnails should be deleted.
  std::vector<unsigned char> out_data;
  EXPECT_FALSE(backend_->thumbnail_db_->GetPageThumbnail(outrow1.id(),
                                                         &out_data));
  EXPECT_FALSE(backend_->thumbnail_db_->GetPageThumbnail(row2_id, &out_data));

  // We should have a favicon for the first URL only. We look them up by favicon
  // URL since the IDs may hav changed.
  FavIconID out_favicon1 = backend_->thumbnail_db_->
      GetFaviconIDForFaviconURL(favicon_url1, FAVICON, NULL);
  EXPECT_TRUE(out_favicon1);
  FavIconID out_favicon2 = backend_->thumbnail_db_->
      GetFaviconIDForFaviconURL(favicon_url2, FAVICON, NULL);
  EXPECT_FALSE(out_favicon2) << "Favicon not deleted";

  // The remaining URL should still reference the same favicon, even if its
  // ID has changed.
  EXPECT_EQ(out_favicon1, GetFavicon(outrow1.url(), FAVICON));

  // The first URL should still be bookmarked.
  EXPECT_TRUE(bookmark_model_.IsBookmarked(row1.url()));

  // The full text database should have no data.
  std::vector<TextDatabase::Match> text_matches;
  Time first_time_searched;
  backend_->text_database_->GetTextMatches(UTF8ToUTF16("Body"),
                                           QueryOptions(),
                                           &text_matches,
                                           &first_time_searched);
  EXPECT_EQ(0U, text_matches.size());
}

TEST_F(HistoryBackendTest, URLsNoLongerBookmarked) {
  GURL favicon_url1("http://www.google.com/favicon.ico");
  GURL favicon_url2("http://news.google.com/favicon.ico");
  FavIconID favicon2 = backend_->thumbnail_db_->AddFavIcon(favicon_url2,
                                                           FAVICON);
  FavIconID favicon1 = backend_->thumbnail_db_->AddFavIcon(favicon_url1,
                                                           FAVICON);

  std::vector<unsigned char> data;
  data.push_back('1');
  EXPECT_TRUE(backend_->thumbnail_db_->SetFavicon(
                  favicon1, new RefCountedBytes(data), Time::Now()));

  data[0] = '2';
  EXPECT_TRUE(backend_->thumbnail_db_->SetFavicon(
                  favicon2, new RefCountedBytes(data), Time::Now()));

  // First visit two URLs.
  URLRow row1(GURL("http://www.google.com/"));
  row1.set_visit_count(2);
  row1.set_typed_count(1);
  row1.set_last_visit(Time::Now());
  EXPECT_TRUE(backend_->thumbnail_db_->AddIconMapping(row1.url(), favicon1));

  URLRow row2(GURL("http://news.google.com/"));
  row2.set_visit_count(1);
  row2.set_last_visit(Time::Now());
  EXPECT_TRUE(backend_->thumbnail_db_->AddIconMapping(row2.url(), favicon2));

  std::vector<URLRow> rows;
  rows.push_back(row2);  // Reversed order for the same reason as favicons.
  rows.push_back(row1);
  backend_->AddPagesWithDetails(rows, history::SOURCE_BROWSED);

  URLID row1_id = backend_->db_->GetRowForURL(row1.url(), NULL);
  URLID row2_id = backend_->db_->GetRowForURL(row2.url(), NULL);

  // Star the two URLs.
  bookmark_model_.SetURLStarred(row1.url(), string16(), true);
  bookmark_model_.SetURLStarred(row2.url(), string16(), true);

  // Delete url 2. Because url 2 is starred this won't delete the URL, only
  // the visits.
  backend_->expirer_.DeleteURL(row2.url());

  // Make sure url 2 is still valid, but has no visits.
  URLRow tmp_url_row;
  EXPECT_EQ(row2_id, backend_->db_->GetRowForURL(row2.url(), NULL));
  VisitVector visits;
  backend_->db_->GetVisitsForURL(row2_id, &visits);
  EXPECT_EQ(0U, visits.size());
  // The favicon should still be valid.
  EXPECT_EQ(favicon2,
      backend_->thumbnail_db_->GetFaviconIDForFaviconURL(favicon_url2,
                                                         FAVICON,
                                                         NULL));

  // Unstar row2.
  bookmark_model_.SetURLStarred(row2.url(), string16(), false);
  // Tell the backend it was unstarred. We have to explicitly do this as
  // BookmarkModel isn't wired up to the backend during testing.
  std::set<GURL> unstarred_urls;
  unstarred_urls.insert(row2.url());
  backend_->URLsNoLongerBookmarked(unstarred_urls);

  // The URL should no longer exist.
  EXPECT_FALSE(backend_->db_->GetRowForURL(row2.url(), &tmp_url_row));
  // And the favicon should be deleted.
  EXPECT_EQ(0,
      backend_->thumbnail_db_->GetFaviconIDForFaviconURL(favicon_url2,
                                                         FAVICON,
                                                         NULL));

  // Unstar row 1.
  bookmark_model_.SetURLStarred(row1.url(), string16(), false);
  // Tell the backend it was unstarred. We have to explicitly do this as
  // BookmarkModel isn't wired up to the backend during testing.
  unstarred_urls.clear();
  unstarred_urls.insert(row1.url());
  backend_->URLsNoLongerBookmarked(unstarred_urls);

  // The URL should still exist (because there were visits).
  EXPECT_EQ(row1_id, backend_->db_->GetRowForURL(row1.url(), NULL));

  // There should still be visits.
  visits.clear();
  backend_->db_->GetVisitsForURL(row1_id, &visits);
  EXPECT_EQ(1U, visits.size());

  // The favicon should still be valid.
  EXPECT_EQ(favicon1,
      backend_->thumbnail_db_->GetFaviconIDForFaviconURL(favicon_url1,
                                                         FAVICON,
                                                         NULL));
}

// Tests a handful of assertions for a navigation with a type of
// KEYWORD_GENERATED.
TEST_F(HistoryBackendTest, KeywordGenerated) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://google.com");

  Time visit_time = Time::Now() - base::TimeDelta::FromDays(1);
  scoped_refptr<HistoryAddPageArgs> request(
      new HistoryAddPageArgs(url, visit_time, NULL, 0, GURL(),
                             history::RedirectList(),
                             PageTransition::KEYWORD_GENERATED,
                             history::SOURCE_BROWSED, false));
  backend_->AddPage(request);

  // A row should have been added for the url.
  URLRow row;
  URLID url_id = backend_->db()->GetRowForURL(url, &row);
  ASSERT_NE(0, url_id);

  // The typed count should be 1.
  ASSERT_EQ(1, row.typed_count());

  // KEYWORD_GENERATED urls should not be added to the segment db.
  std::string segment_name = VisitSegmentDatabase::ComputeSegmentName(url);
  EXPECT_EQ(0, backend_->db()->GetSegmentNamed(segment_name));

  // One visit should be added.
  VisitVector visits;
  EXPECT_TRUE(backend_->db()->GetVisitsForURL(url_id, &visits));
  EXPECT_EQ(1U, visits.size());

  // But no visible visits.
  visits.clear();
  backend_->db()->GetVisibleVisitsInRange(base::Time(), base::Time(), 1,
                                          &visits);
  EXPECT_TRUE(visits.empty());

  // Expire the visits.
  std::set<GURL> restrict_urls;
  backend_->expire_backend()->ExpireHistoryBetween(restrict_urls,
                                                   visit_time, Time::Now());

  // The visit should have been nuked.
  visits.clear();
  EXPECT_TRUE(backend_->db()->GetVisitsForURL(url_id, &visits));
  EXPECT_TRUE(visits.empty());

  // As well as the url.
  ASSERT_EQ(0, backend_->db()->GetRowForURL(url, &row));
}

TEST_F(HistoryBackendTest, ClientRedirect) {
  ASSERT_TRUE(backend_.get());

  int transition1;
  int transition2;

  // Initial transition to page A.
  GURL url_a("http://google.com/a");
  AddClientRedirect(GURL(), url_a, false, &transition1, &transition2);
  EXPECT_TRUE(transition2 & PageTransition::CHAIN_END);

  // User initiated redirect to page B.
  GURL url_b("http://google.com/b");
  AddClientRedirect(url_a, url_b, false, &transition1, &transition2);
  EXPECT_TRUE(transition1 & PageTransition::CHAIN_END);
  EXPECT_TRUE(transition2 & PageTransition::CHAIN_END);

  // Non-user initiated redirect to page C.
  GURL url_c("http://google.com/c");
  AddClientRedirect(url_b, url_c, true, &transition1, &transition2);
  EXPECT_FALSE(transition1 & PageTransition::CHAIN_END);
  EXPECT_TRUE(transition2 & PageTransition::CHAIN_END);
}

TEST_F(HistoryBackendTest, ImportedFaviconsTest) {
  // Setup test data - two Urls in the history, one with favicon assigned and
  // one without.
  GURL favicon_url1("http://www.google.com/favicon.ico");
  FavIconID favicon1 = backend_->thumbnail_db_->AddFavIcon(favicon_url1,
                                                           FAVICON);
  std::vector<unsigned char> data;
  data.push_back('1');
  EXPECT_TRUE(backend_->thumbnail_db_->SetFavicon(favicon1,
      RefCountedBytes::TakeVector(&data), Time::Now()));
  URLRow row1(GURL("http://www.google.com/"));
  row1.set_visit_count(1);
  row1.set_last_visit(Time::Now());
  EXPECT_TRUE(backend_->thumbnail_db_->AddIconMapping(row1.url(), favicon1));

  URLRow row2(GURL("http://news.google.com/"));
  row2.set_visit_count(1);
  row2.set_last_visit(Time::Now());
  std::vector<URLRow> rows;
  rows.push_back(row1);
  rows.push_back(row2);
  backend_->AddPagesWithDetails(rows, history::SOURCE_BROWSED);
  URLRow url_row1, url_row2;
  EXPECT_FALSE(backend_->db_->GetRowForURL(row1.url(), &url_row1) == 0);
  EXPECT_FALSE(backend_->db_->GetRowForURL(row2.url(), &url_row2) == 0);
  EXPECT_FALSE(GetFavicon(row1.url(), FAVICON) == 0);
  EXPECT_TRUE(GetFavicon(row2.url(), FAVICON) == 0);

  // Now provide one imported favicon for both URLs already in the registry.
  // The new favicon should only be used with the URL that doesn't already have
  // a favicon.
  std::vector<history::ImportedFavIconUsage> favicons;
  history::ImportedFavIconUsage favicon;
  favicon.favicon_url = GURL("http://news.google.com/favicon.ico");
  favicon.png_data.push_back('2');
  favicon.urls.insert(row1.url());
  favicon.urls.insert(row2.url());
  favicons.push_back(favicon);
  backend_->SetImportedFavicons(favicons);
  EXPECT_FALSE(backend_->db_->GetRowForURL(row1.url(), &url_row1) == 0);
  EXPECT_FALSE(backend_->db_->GetRowForURL(row2.url(), &url_row2) == 0);
  EXPECT_FALSE(GetFavicon(row1.url(), FAVICON) == 0);
  EXPECT_FALSE(GetFavicon(row2.url(), FAVICON) == 0);
  EXPECT_FALSE(GetFavicon(row1.url(), FAVICON) ==
      GetFavicon(row2.url(), FAVICON));

  // A URL should not be added to history (to store favicon), if
  // the URL is not bookmarked.
  GURL url3("http://mail.google.com");
  favicons.clear();
  favicon.favicon_url = GURL("http://mail.google.com/favicon.ico");
  favicon.png_data.push_back('3');
  favicon.urls.insert(url3);
  favicons.push_back(favicon);
  backend_->SetImportedFavicons(favicons);
  URLRow url_row3;
  EXPECT_TRUE(backend_->db_->GetRowForURL(url3, &url_row3) == 0);

  // If the URL is bookmarked, it should get added to history with 0 visits.
  bookmark_model_.AddURL(bookmark_model_.GetBookmarkBarNode(), 0, string16(),
                         url3);
  backend_->SetImportedFavicons(favicons);
  EXPECT_FALSE(backend_->db_->GetRowForURL(url3, &url_row3) == 0);
  EXPECT_TRUE(url_row3.visit_count() == 0);
}

TEST_F(HistoryBackendTest, StripUsernamePasswordTest) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://anyuser:anypass@www.google.com");
  GURL stripped_url("http://www.google.com");

  // Clear all history.
  backend_->DeleteAllHistory();

  // Visit the url with username, password.
  backend_->AddPageVisit(url, base::Time::Now(), 0,
    PageTransition::GetQualifier(PageTransition::TYPED),
    history::SOURCE_BROWSED);

  // Fetch the row information about stripped url from history db.
  VisitVector visits;
  URLID row_id = backend_->db_->GetRowForURL(stripped_url, NULL);
  backend_->db_->GetVisitsForURL(row_id, &visits);

  // Check if stripped url is stored in database.
  ASSERT_EQ(1U, visits.size());
}

TEST_F(HistoryBackendTest, AddPageVisitSource) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://www.google.com");

  // Clear all history.
  backend_->DeleteAllHistory();

  // Assume visiting the url from an externsion.
  backend_->AddPageVisit(url, base::Time::Now(), 0, PageTransition::TYPED,
                         history::SOURCE_EXTENSION);
  // Assume the url is imported from Firefox.
  backend_->AddPageVisit(url, base::Time::Now(), 0, PageTransition::TYPED,
                         history::SOURCE_FIREFOX_IMPORTED);
  // Assume this url is also synced.
  backend_->AddPageVisit(url, base::Time::Now(), 0, PageTransition::TYPED,
                         history::SOURCE_SYNCED);

  // Fetch the row information about the url from history db.
  VisitVector visits;
  URLID row_id = backend_->db_->GetRowForURL(url, NULL);
  backend_->db_->GetVisitsForURL(row_id, &visits);

  // Check if all the visits to the url are stored in database.
  ASSERT_EQ(3U, visits.size());
  VisitSourceMap visit_sources;
  backend_->db_->GetVisitsSource(visits, &visit_sources);
  ASSERT_EQ(3U, visit_sources.size());
  int sources = 0;
  for (int i = 0; i < 3; i++) {
    switch (visit_sources[visits[i].visit_id]) {
      case history::SOURCE_EXTENSION:
        sources |= 0x1;
        break;
      case history::SOURCE_FIREFOX_IMPORTED:
        sources |= 0x2;
        break;
      case history::SOURCE_SYNCED:
        sources |= 0x4;
      default:
        break;
    }
  }
  EXPECT_EQ(0x7, sources);
}

TEST_F(HistoryBackendTest, AddPageArgsSource) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://testpageargs.com");

  // Assume this page is browsed by user.
  scoped_refptr<HistoryAddPageArgs> request1(
      new HistoryAddPageArgs(url, base::Time::Now(), NULL, 0, GURL(),
                             history::RedirectList(),
                             PageTransition::KEYWORD_GENERATED,
                             history::SOURCE_BROWSED, false));
  backend_->AddPage(request1);
  // Assume this page is synced.
  scoped_refptr<HistoryAddPageArgs> request2(
      new HistoryAddPageArgs(url, base::Time::Now(), NULL, 0, GURL(),
                             history::RedirectList(),
                             PageTransition::LINK,
                             history::SOURCE_SYNCED, false));
  backend_->AddPage(request2);
  // Assume this page is browsed again.
  scoped_refptr<HistoryAddPageArgs> request3(
      new HistoryAddPageArgs(url, base::Time::Now(), NULL, 0, GURL(),
                             history::RedirectList(),
                             PageTransition::TYPED,
                             history::SOURCE_BROWSED, false));
  backend_->AddPage(request3);

  // Three visits should be added with proper sources.
  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(3U, visits.size());
  VisitSourceMap visit_sources;
  backend_->db_->GetVisitsSource(visits, &visit_sources);
  ASSERT_EQ(1U, visit_sources.size());
  EXPECT_EQ(history::SOURCE_SYNCED, visit_sources.begin()->second);
}

TEST_F(HistoryBackendTest, AddVisitsSource) {
  ASSERT_TRUE(backend_.get());

  GURL url1("http://www.cnn.com");
  std::vector<base::Time> visits1;
  visits1.push_back(Time::Now() - base::TimeDelta::FromDays(5));
  visits1.push_back(Time::Now() - base::TimeDelta::FromDays(1));
  visits1.push_back(Time::Now());

  GURL url2("http://www.example.com");
  std::vector<base::Time> visits2;
  visits2.push_back(Time::Now() - base::TimeDelta::FromDays(10));
  visits2.push_back(Time::Now());

  // Clear all history.
  backend_->DeleteAllHistory();

  // Add the visits.
  backend_->AddVisits(url1, visits1, history::SOURCE_IE_IMPORTED);
  backend_->AddVisits(url2, visits2, history::SOURCE_SYNCED);

  // Verify the visits were added with their sources.
  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url1, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(3U, visits.size());
  VisitSourceMap visit_sources;
  backend_->db_->GetVisitsSource(visits, &visit_sources);
  ASSERT_EQ(3U, visit_sources.size());
  for (int i = 0; i < 3; i++)
    EXPECT_EQ(history::SOURCE_IE_IMPORTED, visit_sources[visits[i].visit_id]);
  id = backend_->db()->GetRowForURL(url2, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(2U, visits.size());
  backend_->db_->GetVisitsSource(visits, &visit_sources);
  ASSERT_EQ(2U, visit_sources.size());
  for (int i = 0; i < 2; i++)
    EXPECT_EQ(history::SOURCE_SYNCED, visit_sources[visits[i].visit_id]);
}

TEST_F(HistoryBackendTest, RemoveVisitsSource) {
  ASSERT_TRUE(backend_.get());

  GURL url1("http://www.cnn.com");
  std::vector<base::Time> visits1;
  visits1.push_back(Time::Now() - base::TimeDelta::FromDays(5));
  visits1.push_back(Time::Now());

  GURL url2("http://www.example.com");
  std::vector<base::Time> visits2;
  visits2.push_back(Time::Now() - base::TimeDelta::FromDays(10));
  visits2.push_back(Time::Now());

  // Clear all history.
  backend_->DeleteAllHistory();

  // Add the visits.
  backend_->AddVisits(url1, visits1, history::SOURCE_IE_IMPORTED);
  backend_->AddVisits(url2, visits2, history::SOURCE_SYNCED);

  // Verify the visits of url1 were added.
  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url1, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(2U, visits.size());
  // Remove these visits.
  ASSERT_TRUE(backend_->RemoveVisits(visits));

  // Now check only url2's source in visit_source table.
  VisitSourceMap visit_sources;
  backend_->db_->GetVisitsSource(visits, &visit_sources);
  ASSERT_EQ(0U, visit_sources.size());
  id = backend_->db()->GetRowForURL(url2, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(2U, visits.size());
  backend_->db_->GetVisitsSource(visits, &visit_sources);
  ASSERT_EQ(2U, visit_sources.size());
  for (int i = 0; i < 2; i++)
    EXPECT_EQ(history::SOURCE_SYNCED, visit_sources[visits[i].visit_id]);
}

// Test for migration of adding visit_source table.
TEST_F(HistoryBackendTest, MigrationVisitSource) {
  ASSERT_TRUE(backend_.get());
  backend_->Closing();
  backend_ = NULL;

  FilePath old_history_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &old_history_path));
  old_history_path = old_history_path.AppendASCII("History");
  old_history_path = old_history_path.AppendASCII("HistoryNoSource");

  // Copy history database file to current directory so that it will be deleted
  // in Teardown.
  FilePath new_history_path(getTestDir());
  file_util::Delete(new_history_path, true);
  file_util::CreateDirectory(new_history_path);
  FilePath new_history_file = new_history_path.Append(chrome::kHistoryFilename);
  ASSERT_TRUE(file_util::CopyFile(old_history_path, new_history_file));

  backend_ = new HistoryBackend(new_history_path,
                                new HistoryBackendTestDelegate(this),
                                &bookmark_model_);
  backend_->Init(std::string(), false);
  backend_->Closing();
  backend_ = NULL;

  // Now the database should already be migrated.
  // Check version first.
  int cur_version = HistoryDatabase::GetCurrentVersion();
  sql::Connection db;
  ASSERT_TRUE(db.Open(new_history_file));
  sql::Statement s(db.GetUniqueStatement(
      "SELECT value FROM meta WHERE key = 'version'"));
  ASSERT_TRUE(s.Step());
  int file_version = s.ColumnInt(0);
  EXPECT_EQ(cur_version, file_version);

  // Check visit_source table is created and empty.
  s.Assign(db.GetUniqueStatement(
      "SELECT name FROM sqlite_master WHERE name=\"visit_source\""));
  ASSERT_TRUE(s.Step());
  s.Assign(db.GetUniqueStatement("SELECT * FROM visit_source LIMIT 10"));
  EXPECT_FALSE(s.Step());
}

TEST_F(HistoryBackendTest, SetFaviconMapping) {
  // Init recent_redirects_
  const GURL url1("http://www.google.com");
  const GURL url2("http://www.google.com/m");
  URLRow url_info1(url1);
  url_info1.set_visit_count(0);
  url_info1.set_typed_count(0);
  url_info1.set_last_visit(base::Time());
  url_info1.set_hidden(false);
  backend_->db_->AddURL(url_info1);

  URLRow url_info2(url2);
  url_info2.set_visit_count(0);
  url_info2.set_typed_count(0);
  url_info2.set_last_visit(base::Time());
  url_info2.set_hidden(false);
  backend_->db_->AddURL(url_info2);

  history::RedirectList redirects;
  redirects.push_back(url2);
  redirects.push_back(url1);
  backend_->recent_redirects_.Put(url1, redirects);

  const GURL icon_url("http://www.google.com/icon");
  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
  // Add a favicon
  backend_->SetFavicon(
      url1, icon_url, RefCountedBytes::TakeVector(&data), FAVICON);
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url1, FAVICON, NULL));
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url2, FAVICON, NULL));

  // Add a touch_icon
  backend_->SetFavicon(
      url1, icon_url, RefCountedBytes::TakeVector(&data), TOUCH_ICON);
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url1, TOUCH_ICON, NULL));
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url2, TOUCH_ICON, NULL));
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url1, FAVICON, NULL));

  // Add a TOUCH_PRECOMPOSED_ICON
  backend_->SetFavicon(url1,
                       icon_url,
                       RefCountedBytes::TakeVector(&data),
                       TOUCH_PRECOMPOSED_ICON);
  // The touch_icon was replaced.
  EXPECT_FALSE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url1, TOUCH_ICON, NULL));
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url1, FAVICON, NULL));
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url1, TOUCH_PRECOMPOSED_ICON, NULL));
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url2, TOUCH_PRECOMPOSED_ICON, NULL));

  // Add a touch_icon
  backend_->SetFavicon(
      url1, icon_url, RefCountedBytes::TakeVector(&data), TOUCH_ICON);
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url1, TOUCH_ICON, NULL));
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url1, FAVICON, NULL));
  // The TOUCH_PRECOMPOSED_ICON was replaced.
  EXPECT_FALSE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url1, TOUCH_PRECOMPOSED_ICON, NULL));

  // Add a favicon
  const GURL icon_url2("http://www.google.com/icon2");
  backend_->SetFavicon(
      url1, icon_url2, RefCountedBytes::TakeVector(&data), FAVICON);
  FavIconID icon_id = backend_->thumbnail_db_->GetFaviconIDForFaviconURL(
      icon_url2, FAVICON, NULL);
  EXPECT_NE(0, icon_id);
  std::vector<IconMapping> icon_mapping;
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(
      url1, &icon_mapping));
  // The old icon was replaced.
  EXPECT_TRUE(icon_mapping.size() > 1);
  EXPECT_EQ(icon_id, icon_mapping[1].icon_id);
}

TEST_F(HistoryBackendTest, AddOrUpdateIconMapping) {
  // Test the same icon and page mapping will not be added twice. other case
  // should be covered in TEST_F(HistoryBackendTest, SetFaviconMapping)
  const GURL url("http://www.google.com/");
  const GURL icon_url("http://www.google.com/icon");
  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));

  backend_->SetFavicon(
      url, icon_url, RefCountedBytes::TakeVector(&data), FAVICON);
  FavIconID icon_id = backend_->thumbnail_db_->GetFaviconIDForFaviconURL(
      icon_url, FAVICON, NULL);

  // Add the same mapping
  FavIconID replaced;
  EXPECT_FALSE(backend_->AddOrUpdateIconMapping(
      url, icon_id, FAVICON, &replaced));
  EXPECT_EQ(0, replaced);

  std::vector<IconMapping> icon_mapping;
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(
      url, &icon_mapping));
  EXPECT_EQ(1u, icon_mapping.size());
}

}  // namespace history
