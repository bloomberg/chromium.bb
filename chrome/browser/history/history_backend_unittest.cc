// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/history/in_memory_history_backend.h"
#include "chrome/browser/history/visit_filter.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/tools/profiles/thumbnail-inl.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image.h"

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

  virtual void NotifyProfileError(int backend_id,
                                  sql::InitStatus init_status) OVERRIDE {}
  virtual void SetInMemoryBackend(int backend_id,
                                  InMemoryHistoryBackend* backend) OVERRIDE;
  virtual void BroadcastNotifications(int type,
                                      HistoryDetails* details) OVERRIDE;
  virtual void DBLoaded(int backend_id) OVERRIDE;
  virtual void StartTopSitesMigration(int backend_id) OVERRIDE;
  virtual void NotifyVisitDBObserversOnAddVisit(
      const BriefVisitInfo& info) OVERRIDE {}

 private:
  // Not owned by us.
  HistoryBackendTest* test_;

  DISALLOW_COPY_AND_ASSIGN(HistoryBackendTestDelegate);
};

class HistoryBackendCancelableRequest : public CancelableRequestProvider,
                                       public CancelableRequestConsumerBase {
 public:
  HistoryBackendCancelableRequest() {}

  // CancelableRequestConsumerBase overrides:
  virtual void OnRequestAdded(
      CancelableRequestProvider* provider,
      CancelableRequestProvider::Handle handle) OVERRIDE {}
  virtual void OnRequestRemoved(
      CancelableRequestProvider* provider,
      CancelableRequestProvider::Handle handle) OVERRIDE {}
  virtual void WillExecute(
      CancelableRequestProvider* provider,
      CancelableRequestProvider::Handle handle) OVERRIDE {}
  virtual void DidExecute(
      CancelableRequestProvider* provider,
      CancelableRequestProvider::Handle handle) OVERRIDE {}

  template<class RequestType>
  CancelableRequestProvider::Handle MockScheduleOfRequest(
      RequestType* request) {
    AddRequest(request, this);
    return request->handle();
  }
};

class HistoryBackendTest : public testing::Test {
 public:
  HistoryBackendTest() : bookmark_model_(NULL), loaded_(false) {}
  virtual ~HistoryBackendTest() {
  }

  // Callback for QueryMostVisited.
  void OnQueryMostVisited(CancelableRequestProvider::Handle handle,
                          history::MostVisitedURLList data) {
    most_visited_list_.swap(data);
  }

  // Callback for QueryFiltered.
  void OnQueryFiltered(CancelableRequestProvider::Handle handle,
                       const history::FilteredURLList& data) {
    filtered_list_ = data;
  }

  const history::MostVisitedURLList& get_most_visited_list() const {
    return most_visited_list_;
  }

  const history::FilteredURLList& get_filtered_list() const {
    return filtered_list_;
  }

 protected:
  scoped_refptr<HistoryBackend> backend_;  // Will be NULL on init failure.
  scoped_ptr<InMemoryHistoryBackend> mem_backend_;

  void AddRedirectChain(const char* sequence[], int page_id) {
    AddRedirectChainWithTransitionAndTime(sequence, page_id,
                                          content::PAGE_TRANSITION_LINK,
                                          Time::Now());
  }

  void AddRedirectChainWithTransitionAndTime(
      const char* sequence[],
      int page_id,
      content::PageTransition transition,
      base::Time time) {
    history::RedirectList redirects;
    for (int i = 0; sequence[i] != NULL; ++i)
      redirects.push_back(GURL(sequence[i]));

    int int_scope = 1;
    void* scope = 0;
    memcpy(&scope, &int_scope, sizeof(int_scope));
    scoped_refptr<history::HistoryAddPageArgs> request(
        new history::HistoryAddPageArgs(
            redirects.back(), time, scope, page_id, GURL(),
            redirects, transition, history::SOURCE_BROWSED,
            true));
    backend_->AddPage(request);
  }

  // Adds CLIENT_REDIRECT page transition.
  // |url1| is the source URL and |url2| is the destination.
  // |did_replace| is true if the transition is non-user initiated and the
  // navigation entry for |url2| has replaced that for |url1|. The possibly
  // updated transition code of the visit records for |url1| and |url2| is
  // returned by filling in |*transition1| and |*transition2|, respectively.
  // |time| is a time of the redirect.
  void  AddClientRedirect(const GURL& url1, const GURL& url2, bool did_replace,
                          base::Time time,
                          int* transition1, int* transition2) {
    void* const dummy_scope = reinterpret_cast<void*>(0x87654321);
    history::RedirectList redirects;
    if (url1.is_valid())
      redirects.push_back(url1);
    if (url2.is_valid())
      redirects.push_back(url2);
    scoped_refptr<HistoryAddPageArgs> request(
        new HistoryAddPageArgs(url2, time, dummy_scope, 0, url1,
            redirects, content::PAGE_TRANSITION_CLIENT_REDIRECT,
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

  FaviconID GetFavicon(const GURL& url, IconType icon_type) {
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
                                  0,
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

  void SetInMemoryBackend(int backend_id, InMemoryHistoryBackend* backend) {
    mem_backend_.reset(backend);
  }

  void BroadcastNotifications(int type,
                                      HistoryDetails* details) {
    // Send the notifications directly to the in-memory database.
    content::Details<HistoryDetails> det(details);
    mem_backend_->Observe(type, content::Source<HistoryBackendTest>(NULL), det);

    // The backend passes ownership of the details pointer to us.
    delete details;
  }

  MessageLoop message_loop_;
  FilePath test_dir_;
  history::MostVisitedURLList most_visited_list_;
  history::FilteredURLList filtered_list_;
};

void HistoryBackendTestDelegate::SetInMemoryBackend(int backend_id,
    InMemoryHistoryBackend* backend) {
  test_->SetInMemoryBackend(backend_id, backend);
}

void HistoryBackendTestDelegate::BroadcastNotifications(
    int type,
    HistoryDetails* details) {
  test_->BroadcastNotifications(type, details);
}

void HistoryBackendTestDelegate::DBLoaded(int backend_id) {
  test_->loaded_ = true;
}

void HistoryBackendTestDelegate::StartTopSitesMigration(int backend_id) {
  test_->backend_->MigrateThumbnailsDatabase();
}

// http://crbug.com/114287
#if defined(OS_WIN)
#define MAYBE_Loaded DISABLED_Loaded
#else
#define MAYBE_Loaded Loaded
#endif // defined(OS_WIN)
TEST_F(HistoryBackendTest, MAYBE_Loaded) {
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
  FaviconID favicon2 = backend_->thumbnail_db_->AddFavicon(favicon_url2,
                                                           FAVICON);
  FaviconID favicon1 = backend_->thumbnail_db_->AddFavicon(favicon_url1,
                                                           FAVICON);

  std::vector<unsigned char> data;
  data.push_back('1');
  EXPECT_TRUE(backend_->thumbnail_db_->SetFavicon(favicon1,
      new base::RefCountedBytes(data), Time::Now()));

  data[0] = '2';
  EXPECT_TRUE(backend_->thumbnail_db_->SetFavicon(
                  favicon2, new base::RefCountedBytes(data), Time::Now()));

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

  URLRows rows;
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

  // Add thumbnails for each page. The |Images| take ownership of SkBitmap
  // created from decoding the images.
  ThumbnailScore score(0.25, true, true);
  scoped_ptr<SkBitmap> google_bitmap(
      gfx::JPEGCodec::Decode(kGoogleThumbnail, sizeof(kGoogleThumbnail)));

  gfx::Image google_image(*google_bitmap);

  Time time;
  GURL gurl;
  backend_->thumbnail_db_->SetPageThumbnail(gurl, row1_id, &google_image,
                                            score, time);
  scoped_ptr<SkBitmap> weewar_bitmap(
     gfx::JPEGCodec::Decode(kWeewarThumbnail, sizeof(kWeewarThumbnail)));
  gfx::Image weewar_image(*weewar_bitmap);
  backend_->thumbnail_db_->SetPageThumbnail(gurl, row2_id, &weewar_image,
                                            score, time);

  // Star row1.
  bookmark_model_.AddURL(
      bookmark_model_.bookmark_bar_node(), 0, string16(), row1.url());

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
  FaviconID out_favicon1 = backend_->thumbnail_db_->
      GetFaviconIDForFaviconURL(favicon_url1, FAVICON, NULL);
  EXPECT_TRUE(out_favicon1);
  FaviconID out_favicon2 = backend_->thumbnail_db_->
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

// Checks that adding a visit, then calling DeleteAll, and then trying to add
// data for the visited page works.  This can happen when clearing the history
// immediately after visiting a page.
TEST_F(HistoryBackendTest, DeleteAllThenAddData) {
  ASSERT_TRUE(backend_.get());

  Time visit_time = Time::Now();
  GURL url("http://www.google.com/");
  scoped_refptr<HistoryAddPageArgs> request(
      new HistoryAddPageArgs(url, visit_time, NULL, 0, GURL(),
                             history::RedirectList(),
                             content::PAGE_TRANSITION_KEYWORD_GENERATED,
                             history::SOURCE_BROWSED, false));
  backend_->AddPage(request);

  // Check that a row was added.
  URLRow outrow;
  EXPECT_TRUE(backend_->db_->GetRowForURL(url, &outrow));

  // Check that the visit was added.
  VisitVector all_visits;
  backend_->db_->GetAllVisitsInRange(Time(), Time(), 0, &all_visits);
  ASSERT_EQ(1U, all_visits.size());

  // Clear all history.
  backend_->DeleteAllHistory();

  // The row should be deleted.
  EXPECT_FALSE(backend_->db_->GetRowForURL(url, &outrow));

  // The visit should be deleted.
  backend_->db_->GetAllVisitsInRange(Time(), Time(), 0, &all_visits);
  ASSERT_EQ(0U, all_visits.size());

  // Try and set the full text index.
  backend_->SetPageTitle(url, UTF8ToUTF16("Title"));
  backend_->SetPageContents(url, UTF8ToUTF16("Body"));

  // The row should still be deleted.
  EXPECT_FALSE(backend_->db_->GetRowForURL(url, &outrow));

  // The visit should still be deleted.
  backend_->db_->GetAllVisitsInRange(Time(), Time(), 0, &all_visits);
  ASSERT_EQ(0U, all_visits.size());

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
  FaviconID favicon2 = backend_->thumbnail_db_->AddFavicon(favicon_url2,
                                                           FAVICON);
  FaviconID favicon1 = backend_->thumbnail_db_->AddFavicon(favicon_url1,
                                                           FAVICON);

  std::vector<unsigned char> data;
  data.push_back('1');
  EXPECT_TRUE(backend_->thumbnail_db_->SetFavicon(
                  favicon1, new base::RefCountedBytes(data), Time::Now()));

  data[0] = '2';
  EXPECT_TRUE(backend_->thumbnail_db_->SetFavicon(
                  favicon2, new base::RefCountedBytes(data), Time::Now()));

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

  URLRows rows;
  rows.push_back(row2);  // Reversed order for the same reason as favicons.
  rows.push_back(row1);
  backend_->AddPagesWithDetails(rows, history::SOURCE_BROWSED);

  URLID row1_id = backend_->db_->GetRowForURL(row1.url(), NULL);
  URLID row2_id = backend_->db_->GetRowForURL(row2.url(), NULL);

  // Star the two URLs.
  bookmark_utils::AddIfNotBookmarked(&bookmark_model_, row1.url(), string16());
  bookmark_utils::AddIfNotBookmarked(&bookmark_model_, row2.url(), string16());

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
  bookmark_utils::RemoveAllBookmarks(&bookmark_model_, row2.url());

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
  bookmark_utils::RemoveAllBookmarks(&bookmark_model_, row1.url());
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
                             content::PAGE_TRANSITION_KEYWORD_GENERATED,
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
                                          &visits, true);
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
  AddClientRedirect(GURL(), url_a, false, base::Time(),
                    &transition1, &transition2);
  EXPECT_TRUE(transition2 & content::PAGE_TRANSITION_CHAIN_END);

  // User initiated redirect to page B.
  GURL url_b("http://google.com/b");
  AddClientRedirect(url_a, url_b, false, base::Time(),
                    &transition1, &transition2);
  EXPECT_TRUE(transition1 & content::PAGE_TRANSITION_CHAIN_END);
  EXPECT_TRUE(transition2 & content::PAGE_TRANSITION_CHAIN_END);

  // Non-user initiated redirect to page C.
  GURL url_c("http://google.com/c");
  AddClientRedirect(url_b, url_c, true, base::Time(),
                    &transition1, &transition2);
  EXPECT_FALSE(transition1 & content::PAGE_TRANSITION_CHAIN_END);
  EXPECT_TRUE(transition2 & content::PAGE_TRANSITION_CHAIN_END);
}

TEST_F(HistoryBackendTest, ImportedFaviconsTest) {
  // Setup test data - two Urls in the history, one with favicon assigned and
  // one without.
  GURL favicon_url1("http://www.google.com/favicon.ico");
  FaviconID favicon1 = backend_->thumbnail_db_->AddFavicon(favicon_url1,
                                                           FAVICON);
  std::vector<unsigned char> data;
  data.push_back('1');
  EXPECT_TRUE(backend_->thumbnail_db_->SetFavicon(favicon1,
      base::RefCountedBytes::TakeVector(&data), Time::Now()));
  URLRow row1(GURL("http://www.google.com/"));
  row1.set_visit_count(1);
  row1.set_last_visit(Time::Now());
  EXPECT_TRUE(backend_->thumbnail_db_->AddIconMapping(row1.url(), favicon1));

  URLRow row2(GURL("http://news.google.com/"));
  row2.set_visit_count(1);
  row2.set_last_visit(Time::Now());
  URLRows rows;
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
  std::vector<history::ImportedFaviconUsage> favicons;
  history::ImportedFaviconUsage favicon;
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
  bookmark_model_.AddURL(bookmark_model_.bookmark_bar_node(), 0, string16(),
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
      content::PageTransitionFromInt(
          content::PageTransitionGetQualifier(content::PAGE_TRANSITION_TYPED)),
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
  backend_->AddPageVisit(
      url, base::Time::Now(), 0, content::PAGE_TRANSITION_TYPED,
      history::SOURCE_EXTENSION);
  // Assume the url is imported from Firefox.
  backend_->AddPageVisit(url, base::Time::Now(), 0,
                         content::PAGE_TRANSITION_TYPED,
                         history::SOURCE_FIREFOX_IMPORTED);
  // Assume this url is also synced.
  backend_->AddPageVisit(url, base::Time::Now(), 0,
                         content::PAGE_TRANSITION_TYPED,
                         history::SOURCE_SYNCED);

  // Fetch the row information about the url from history db.
  VisitVector visits;
  URLID row_id = backend_->db_->GetRowForURL(url, NULL);
  backend_->db_->GetVisitsForURL(row_id, &visits);

  // Check if all the visits to the url are stored in database.
  ASSERT_EQ(3U, visits.size());
  VisitSourceMap visit_sources;
  ASSERT_TRUE(backend_->GetVisitsSource(visits, &visit_sources));
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
                             content::PAGE_TRANSITION_KEYWORD_GENERATED,
                             history::SOURCE_BROWSED, false));
  backend_->AddPage(request1);
  // Assume this page is synced.
  scoped_refptr<HistoryAddPageArgs> request2(
      new HistoryAddPageArgs(url, base::Time::Now(), NULL, 0, GURL(),
                             history::RedirectList(),
                             content::PAGE_TRANSITION_LINK,
                             history::SOURCE_SYNCED, false));
  backend_->AddPage(request2);
  // Assume this page is browsed again.
  scoped_refptr<HistoryAddPageArgs> request3(
      new HistoryAddPageArgs(url, base::Time::Now(), NULL, 0, GURL(),
                             history::RedirectList(),
                             content::PAGE_TRANSITION_TYPED,
                             history::SOURCE_BROWSED, false));
  backend_->AddPage(request3);

  // Three visits should be added with proper sources.
  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(3U, visits.size());
  VisitSourceMap visit_sources;
  ASSERT_TRUE(backend_->GetVisitsSource(visits, &visit_sources));
  ASSERT_EQ(1U, visit_sources.size());
  EXPECT_EQ(history::SOURCE_SYNCED, visit_sources.begin()->second);
}

TEST_F(HistoryBackendTest, AddVisitsSource) {
  ASSERT_TRUE(backend_.get());

  GURL url1("http://www.cnn.com");
  std::vector<VisitInfo> visits1, visits2;
  visits1.push_back(VisitInfo(
      Time::Now() - base::TimeDelta::FromDays(5),
      content::PAGE_TRANSITION_LINK));
  visits1.push_back(VisitInfo(
      Time::Now() - base::TimeDelta::FromDays(1),
      content::PAGE_TRANSITION_LINK));
  visits1.push_back(VisitInfo(
      Time::Now(), content::PAGE_TRANSITION_LINK));

  GURL url2("http://www.example.com");
  visits2.push_back(VisitInfo(
      Time::Now() - base::TimeDelta::FromDays(10),
      content::PAGE_TRANSITION_LINK));
  visits2.push_back(VisitInfo(Time::Now(), content::PAGE_TRANSITION_LINK));

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
  ASSERT_TRUE(backend_->GetVisitsSource(visits, &visit_sources));
  ASSERT_EQ(3U, visit_sources.size());
  for (int i = 0; i < 3; i++)
    EXPECT_EQ(history::SOURCE_IE_IMPORTED, visit_sources[visits[i].visit_id]);
  id = backend_->db()->GetRowForURL(url2, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(2U, visits.size());
  ASSERT_TRUE(backend_->GetVisitsSource(visits, &visit_sources));
  ASSERT_EQ(2U, visit_sources.size());
  for (int i = 0; i < 2; i++)
    EXPECT_EQ(history::SOURCE_SYNCED, visit_sources[visits[i].visit_id]);
}

TEST_F(HistoryBackendTest, GetMostRecentVisits) {
  ASSERT_TRUE(backend_.get());

  GURL url1("http://www.cnn.com");
  std::vector<VisitInfo> visits1;
  visits1.push_back(VisitInfo(
      Time::Now() - base::TimeDelta::FromDays(5),
      content::PAGE_TRANSITION_LINK));
  visits1.push_back(VisitInfo(
      Time::Now() - base::TimeDelta::FromDays(1),
      content::PAGE_TRANSITION_LINK));
  visits1.push_back(VisitInfo(
      Time::Now(), content::PAGE_TRANSITION_LINK));

  // Clear all history.
  backend_->DeleteAllHistory();

  // Add the visits.
  backend_->AddVisits(url1, visits1, history::SOURCE_IE_IMPORTED);

  // Verify the visits were added with their sources.
  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url1, &row);
  ASSERT_TRUE(backend_->db()->GetMostRecentVisitsForURL(id, 1, &visits));
  ASSERT_EQ(1U, visits.size());
  EXPECT_EQ(visits1[2].first, visits[0].visit_time);
}

TEST_F(HistoryBackendTest, RemoveVisitsTransitions) {
  ASSERT_TRUE(backend_.get());

  // Clear all history.
  backend_->DeleteAllHistory();

  GURL url1("http://www.cnn.com");
  VisitInfo typed_visit(
      Time::Now() - base::TimeDelta::FromDays(6),
      content::PAGE_TRANSITION_TYPED);
  VisitInfo reload_visit(
      Time::Now() - base::TimeDelta::FromDays(5),
      content::PAGE_TRANSITION_RELOAD);
  VisitInfo link_visit(
      Time::Now() - base::TimeDelta::FromDays(4),
      content::PAGE_TRANSITION_LINK);
  std::vector<VisitInfo> visits_to_add;
  visits_to_add.push_back(typed_visit);
  visits_to_add.push_back(reload_visit);
  visits_to_add.push_back(link_visit);

  // Add the visits.
  backend_->AddVisits(url1, visits_to_add, history::SOURCE_SYNCED);

  // Verify that the various counts are what we expect.
  VisitVector visits;
  URLRow row;
  URLID id = backend_->db()->GetRowForURL(url1, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(3U, visits.size());
  ASSERT_EQ(1, row.typed_count());
  ASSERT_EQ(2, row.visit_count());

  // Now, delete the typed visit and verify that typed_count is updated.
  ASSERT_TRUE(backend_->RemoveVisits(VisitVector(1, visits[0])));
  id = backend_->db()->GetRowForURL(url1, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(2U, visits.size());
  ASSERT_EQ(0, row.typed_count());
  ASSERT_EQ(1, row.visit_count());

  // Delete the reload visit now and verify that none of the counts have
  // changed.
  ASSERT_TRUE(backend_->RemoveVisits(VisitVector(1, visits[0])));
  id = backend_->db()->GetRowForURL(url1, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(1U, visits.size());
  ASSERT_EQ(0, row.typed_count());
  ASSERT_EQ(1, row.visit_count());

  // Delete the last visit and verify that we delete the URL.
  ASSERT_TRUE(backend_->RemoveVisits(VisitVector(1, visits[0])));
  ASSERT_EQ(0, backend_->db()->GetRowForURL(url1, &row));
}

TEST_F(HistoryBackendTest, RemoveVisitsSource) {
  ASSERT_TRUE(backend_.get());

  GURL url1("http://www.cnn.com");
  std::vector<VisitInfo> visits1, visits2;
  visits1.push_back(VisitInfo(
      Time::Now() - base::TimeDelta::FromDays(5),
      content::PAGE_TRANSITION_LINK));
  visits1.push_back(VisitInfo(Time::Now(),
    content::PAGE_TRANSITION_LINK));

  GURL url2("http://www.example.com");
  visits2.push_back(VisitInfo(
      Time::Now() - base::TimeDelta::FromDays(10),
      content::PAGE_TRANSITION_LINK));
  visits2.push_back(VisitInfo(Time::Now(), content::PAGE_TRANSITION_LINK));

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
  ASSERT_TRUE(backend_->GetVisitsSource(visits, &visit_sources));
  ASSERT_EQ(0U, visit_sources.size());
  id = backend_->db()->GetRowForURL(url2, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
  ASSERT_EQ(2U, visits.size());
  ASSERT_TRUE(backend_->GetVisitsSource(visits, &visit_sources));
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
                                0,
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
      url1, icon_url, base::RefCountedBytes::TakeVector(&data), FAVICON);
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url1, FAVICON, NULL));
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url2, FAVICON, NULL));

  // Add a touch_icon
  backend_->SetFavicon(
      url1, icon_url, base::RefCountedBytes::TakeVector(&data), TOUCH_ICON);
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url1, TOUCH_ICON, NULL));
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url2, TOUCH_ICON, NULL));
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url1, FAVICON, NULL));

  // Add a TOUCH_PRECOMPOSED_ICON
  backend_->SetFavicon(url1,
                       icon_url,
                       base::RefCountedBytes::TakeVector(&data),
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
      url1, icon_url, base::RefCountedBytes::TakeVector(&data), TOUCH_ICON);
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
      url1, icon_url2, base::RefCountedBytes::TakeVector(&data), FAVICON);
  FaviconID icon_id = backend_->thumbnail_db_->GetFaviconIDForFaviconURL(
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
      url, icon_url, base::RefCountedBytes::TakeVector(&data), FAVICON);
  FaviconID icon_id = backend_->thumbnail_db_->GetFaviconIDForFaviconURL(
      icon_url, FAVICON, NULL);

  // Add the same mapping
  FaviconID replaced;
  EXPECT_FALSE(backend_->AddOrUpdateIconMapping(
      url, icon_id, FAVICON, &replaced));
  EXPECT_EQ(0, replaced);

  std::vector<IconMapping> icon_mapping;
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(
      url, &icon_mapping));
  EXPECT_EQ(1u, icon_mapping.size());
}

TEST_F(HistoryBackendTest, GetFaviconForURL) {
  // This test will add a fav icon and touch icon for the same URL
  // and check the behaviour of backend's GetFaviconForURL implementation.
  const GURL url("http://www.google.com/");
  const GURL icon_url("http://www.google.com/icon");
  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<base::RefCountedBytes> bytes(new base::RefCountedBytes(data));
  // Used for testing the icon data after getting from DB
  std::string blob_data(bytes->front(),
                        bytes->front() + bytes->size());

  // Add a favicon
  backend_->SetFavicon(
      url, icon_url, bytes.get(), FAVICON);
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url, FAVICON, NULL));

  // Add a touch_icon
  backend_->SetFavicon(
      url, icon_url, bytes.get(), TOUCH_ICON);
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url, TOUCH_ICON, NULL));

  // Test the Fav icon for this URL.
  FaviconData favicon;
  ASSERT_TRUE(backend_->GetFaviconFromDB(url, FAVICON, &favicon));
  std::string favicon_data(
      favicon.image_data->front(),
      favicon.image_data->front() + favicon.image_data->size());

  EXPECT_EQ(FAVICON, favicon.icon_type);
  EXPECT_EQ(icon_url, favicon.icon_url);
  EXPECT_EQ(blob_data, favicon_data);

  // Test the touch icon for this URL.
  ASSERT_TRUE(backend_->GetFaviconFromDB(url, TOUCH_ICON, &favicon));
  std::string touchicon_data(
      favicon.image_data->front(),
      favicon.image_data->front() + favicon.image_data->size());

  EXPECT_EQ(TOUCH_ICON, favicon.icon_type);
  EXPECT_EQ(icon_url, favicon.icon_url);
  EXPECT_EQ(blob_data, touchicon_data);
}

TEST_F(HistoryBackendTest, CloneFaviconIsRestrictedToSameDomain) {
  const GURL url("http://www.google.com/");
  const GURL icon_url("http://www.google.com/icon");
  const GURL same_domain_url("http://www.google.com/subdir/index.html");
  const GURL foreign_domain_url("http://www.not-google.com/");

  // Add a favicon
  std::vector<unsigned char> data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<base::RefCountedBytes> bytes(new base::RefCountedBytes(data));
  backend_->SetFavicon(
      url, icon_url, bytes.get(), FAVICON);
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingForPageURL(
      url, FAVICON, NULL));

  // Validate starting state.
  FaviconData favicon;
  EXPECT_TRUE(backend_->GetFaviconFromDB(url, FAVICON, &favicon));
  EXPECT_FALSE(backend_->GetFaviconFromDB(same_domain_url, FAVICON, &favicon));
  EXPECT_FALSE(backend_->GetFaviconFromDB(foreign_domain_url,
                                          FAVICON, &favicon));

  // Same-domain cloning should work.
  backend_->CloneFavicon(url, same_domain_url);
  EXPECT_TRUE(backend_->GetFaviconFromDB(same_domain_url, FAVICON, &favicon));

  // Foreign-domain cloning is forbidden.
  backend_->CloneFavicon(url, foreign_domain_url);
  EXPECT_FALSE(backend_->GetFaviconFromDB(foreign_domain_url,
                                          FAVICON, &favicon));
}

TEST_F(HistoryBackendTest, QueryFilteredURLs) {
  const char* google = "http://www.google.com/";
  const char* yahoo = "http://www.yahoo.com/";
  const char* yahoo_sports = "http://sports.yahoo.com/";
  const char* yahoo_sports_with_article1 =
      "http://sports.yahoo.com/article1.htm";
  const char* yahoo_sports_with_article2 =
      "http://sports.yahoo.com/article2.htm";
  const char* yahoo_sports_soccer = "http://sports.yahoo.com/soccer";

  // Clear all history.
  backend_->DeleteAllHistory();

  Time tested_time = Time::Now().LocalMidnight() +
                     base::TimeDelta::FromHours(4);
  base::TimeDelta half_an_hour = base::TimeDelta::FromMinutes(30);
  base::TimeDelta one_hour = base::TimeDelta::FromHours(1);
  base::TimeDelta one_day = base::TimeDelta::FromDays(1);

  const content::PageTransition kTypedTransition =
      content::PAGE_TRANSITION_TYPED;

  const char* redirect_sequence[2];
  redirect_sequence[1] = NULL;

  redirect_sequence[0] = google;
  AddRedirectChainWithTransitionAndTime(
      redirect_sequence, 0, kTypedTransition,
      tested_time - one_day - half_an_hour * 2);
  AddRedirectChainWithTransitionAndTime(
      redirect_sequence, 0,
      kTypedTransition, tested_time - one_day);
  AddRedirectChainWithTransitionAndTime(
      redirect_sequence, 0,
      kTypedTransition, tested_time - half_an_hour / 2);
  AddRedirectChainWithTransitionAndTime(redirect_sequence, 0,
                                        kTypedTransition, tested_time);

  redirect_sequence[0] = yahoo;
  AddRedirectChainWithTransitionAndTime(redirect_sequence, 0,
                                        kTypedTransition,
                                        tested_time - one_day + half_an_hour);
  AddRedirectChainWithTransitionAndTime(
      redirect_sequence, 0, kTypedTransition,
      tested_time - one_day + half_an_hour * 2);

  redirect_sequence[0] = yahoo_sports;
  AddRedirectChainWithTransitionAndTime(
      redirect_sequence, 0, kTypedTransition,
      tested_time - one_day - half_an_hour * 2);
  AddRedirectChainWithTransitionAndTime(redirect_sequence, 0, kTypedTransition,
                                        tested_time - one_day);
  int transition1, transition2;
  AddClientRedirect(GURL(yahoo_sports), GURL(yahoo_sports_with_article1), false,
                    tested_time - one_day + half_an_hour,
                    &transition1, &transition2);
  AddClientRedirect(GURL(yahoo_sports_with_article1),
                    GURL(yahoo_sports_with_article2),
                    false,
                    tested_time - one_day + half_an_hour * 2,
                    &transition1, &transition2);

  redirect_sequence[0] = yahoo_sports_soccer;
  AddRedirectChainWithTransitionAndTime(redirect_sequence, 0,
                                        kTypedTransition,
                                        tested_time - half_an_hour);
  backend_->Commit();

  scoped_refptr<QueryFilteredURLsRequest> request1 =
      new history::QueryFilteredURLsRequest(
          base::Bind(&HistoryBackendTest::OnQueryFiltered,
                     base::Unretained(static_cast<HistoryBackendTest*>(this))));
  HistoryBackendCancelableRequest cancellable_request;
  cancellable_request.MockScheduleOfRequest<QueryFilteredURLsRequest>(
      request1);

  VisitFilter filter;
  // Time limit is |tested_time| +/- 45 min.
  base::TimeDelta three_quarters_of_an_hour = base::TimeDelta::FromMinutes(45);
  filter.SetFilterTime(tested_time);
  filter.SetFilterWidth(three_quarters_of_an_hour);
  backend_->QueryFilteredURLs(request1, 100, filter);

  ASSERT_EQ(4U, get_filtered_list().size());
  EXPECT_EQ(std::string(google), get_filtered_list()[0].url.spec());
  EXPECT_EQ(std::string(yahoo_sports_soccer),
            get_filtered_list()[1].url.spec());
  EXPECT_EQ(std::string(yahoo), get_filtered_list()[2].url.spec());
  EXPECT_EQ(std::string(yahoo_sports),
            get_filtered_list()[3].url.spec());

  // Time limit is between |tested_time| and |tested_time| + 2 hours.
  scoped_refptr<QueryFilteredURLsRequest> request2 =
      new history::QueryFilteredURLsRequest(
          base::Bind(&HistoryBackendTest::OnQueryFiltered,
                     base::Unretained(static_cast<HistoryBackendTest*>(this))));
  cancellable_request.MockScheduleOfRequest<QueryFilteredURLsRequest>(
      request2);
  filter.SetFilterTime(tested_time + one_hour);
  filter.SetFilterWidth(one_hour);
  backend_->QueryFilteredURLs(request2, 100, filter);

  ASSERT_EQ(3U, get_filtered_list().size());
  EXPECT_EQ(std::string(google), get_filtered_list()[0].url.spec());
  EXPECT_EQ(std::string(yahoo), get_filtered_list()[1].url.spec());
  EXPECT_EQ(std::string(yahoo_sports), get_filtered_list()[2].url.spec());

  // Time limit is between |tested_time| - 2 hours and |tested_time|.
  scoped_refptr<QueryFilteredURLsRequest> request3 =
      new history::QueryFilteredURLsRequest(
          base::Bind(&HistoryBackendTest::OnQueryFiltered,
                     base::Unretained(static_cast<HistoryBackendTest*>(this))));
  cancellable_request.MockScheduleOfRequest<QueryFilteredURLsRequest>(
      request3);
  filter.SetFilterTime(tested_time - one_hour);
  filter.SetFilterWidth(one_hour);
  backend_->QueryFilteredURLs(request3, 100, filter);

  ASSERT_EQ(3U, get_filtered_list().size());
  EXPECT_EQ(std::string(google), get_filtered_list()[0].url.spec());
  EXPECT_EQ(std::string(yahoo_sports_soccer),
            get_filtered_list()[1].url.spec());
  EXPECT_EQ(std::string(yahoo_sports), get_filtered_list()[2].url.spec());

  filter.ClearFilters();
  base::Time::Exploded exploded_time;
  tested_time.LocalExplode(&exploded_time);

  // Today.
  scoped_refptr<QueryFilteredURLsRequest> request4 =
      new history::QueryFilteredURLsRequest(
          base::Bind(&HistoryBackendTest::OnQueryFiltered,
                     base::Unretained(static_cast<HistoryBackendTest*>(this))));
  cancellable_request.MockScheduleOfRequest<QueryFilteredURLsRequest>(
      request4);
  filter.SetFilterTime(tested_time);
  filter.SetDayOfTheWeekFilter(static_cast<int>(exploded_time.day_of_week));
  backend_->QueryFilteredURLs(request4, 100, filter);

  ASSERT_EQ(2U, get_filtered_list().size());
  EXPECT_EQ(std::string(google), get_filtered_list()[0].url.spec());
  EXPECT_EQ(std::string(yahoo_sports_soccer),
            get_filtered_list()[1].url.spec());

  // Today + time limit - only yahoo_sports_soccer should fit.
  scoped_refptr<QueryFilteredURLsRequest> request5 =
      new history::QueryFilteredURLsRequest(
          base::Bind(&HistoryBackendTest::OnQueryFiltered,
                     base::Unretained(static_cast<HistoryBackendTest*>(this))));
  cancellable_request.MockScheduleOfRequest<QueryFilteredURLsRequest>(
      request5);
  filter.SetFilterTime(tested_time - base::TimeDelta::FromMinutes(40));
  filter.SetFilterWidth(base::TimeDelta::FromMinutes(20));
  backend_->QueryFilteredURLs(request5, 100, filter);

  ASSERT_EQ(1U, get_filtered_list().size());
  EXPECT_EQ(std::string(yahoo_sports_soccer),
            get_filtered_list()[0].url.spec());
}

TEST_F(HistoryBackendTest, UpdateVisitDuration) {
  // This unit test will test adding and deleting visit details information.
  ASSERT_TRUE(backend_.get());

  GURL url1("http://www.cnn.com");
  std::vector<VisitInfo> visit_info1, visit_info2;
  Time start_ts = Time::Now() - base::TimeDelta::FromDays(5);
  Time end_ts = start_ts + base::TimeDelta::FromDays(2);
  visit_info1.push_back(VisitInfo(start_ts, content::PAGE_TRANSITION_LINK));

  GURL url2("http://www.example.com");
  visit_info2.push_back(VisitInfo(Time::Now() - base::TimeDelta::FromDays(10),
                                  content::PAGE_TRANSITION_LINK));

  // Clear all history.
  backend_->DeleteAllHistory();

  // Add the visits.
  backend_->AddVisits(url1, visit_info1, history::SOURCE_BROWSED);
  backend_->AddVisits(url2, visit_info2, history::SOURCE_BROWSED);

  // Verify the entries for both visits were added in visit_details.
  VisitVector visits1, visits2;
  URLRow row;
  URLID url_id1 = backend_->db()->GetRowForURL(url1, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(url_id1, &visits1));
  ASSERT_EQ(1U, visits1.size());
  EXPECT_EQ(0, visits1[0].visit_duration.ToInternalValue());

  URLID url_id2 = backend_->db()->GetRowForURL(url2, &row);
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(url_id2, &visits2));
  ASSERT_EQ(1U, visits2.size());
  EXPECT_EQ(0, visits2[0].visit_duration.ToInternalValue());

  // Update the visit to cnn.com.
  backend_->UpdateVisitDuration(visits1[0].visit_id, end_ts);

  // Check the duration for visiting cnn.com was correctly updated.
  ASSERT_TRUE(backend_->db()->GetVisitsForURL(url_id1, &visits1));
  ASSERT_EQ(1U, visits1.size());
  base::TimeDelta expected_duration = end_ts - start_ts;
  EXPECT_EQ(expected_duration.ToInternalValue(),
            visits1[0].visit_duration.ToInternalValue());

  // Remove the visit to cnn.com.
  ASSERT_TRUE(backend_->RemoveVisits(visits1));
}

// Test for migration of adding visit_duration column.
TEST_F(HistoryBackendTest, MigrationVisitDuration) {
  ASSERT_TRUE(backend_.get());
  backend_->Closing();
  backend_ = NULL;

  FilePath old_history_path, old_history, old_archived;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &old_history_path));
  old_history_path = old_history_path.AppendASCII("History");
  old_history = old_history_path.AppendASCII("HistoryNoDuration");
  old_archived = old_history_path.AppendASCII("ArchivedNoDuration");

  // Copy history database file to current directory so that it will be deleted
  // in Teardown.
  FilePath new_history_path(getTestDir());
  file_util::Delete(new_history_path, true);
  file_util::CreateDirectory(new_history_path);
  FilePath new_history_file = new_history_path.Append(chrome::kHistoryFilename);
  FilePath new_archived_file =
      new_history_path.Append(chrome::kArchivedHistoryFilename);
  ASSERT_TRUE(file_util::CopyFile(old_history, new_history_file));
  ASSERT_TRUE(file_util::CopyFile(old_archived, new_archived_file));

  backend_ = new HistoryBackend(new_history_path,
                                0,
                                new HistoryBackendTestDelegate(this),
                                &bookmark_model_);
  backend_->Init(std::string(), false);
  backend_->Closing();
  backend_ = NULL;

  // Now both history and archived_history databases should already be migrated.

  // Check version in history database first.
  int cur_version = HistoryDatabase::GetCurrentVersion();
  sql::Connection db;
  ASSERT_TRUE(db.Open(new_history_file));
  sql::Statement s(db.GetUniqueStatement(
      "SELECT value FROM meta WHERE key = 'version'"));
  ASSERT_TRUE(s.Step());
  int file_version = s.ColumnInt(0);
  EXPECT_EQ(cur_version, file_version);

  // Check visit_duration column in visits table is created and set to 0.
  s.Assign(db.GetUniqueStatement(
      "SELECT visit_duration FROM visits LIMIT 1"));
  ASSERT_TRUE(s.Step());
  EXPECT_EQ(0, s.ColumnInt(0));

  // Repeat version and visit_duration checks in archived history database
  // also.
  cur_version = ArchivedDatabase::GetCurrentVersion();
  sql::Connection archived_db;
  ASSERT_TRUE(archived_db.Open(new_archived_file));
  sql::Statement s1(archived_db.GetUniqueStatement(
      "SELECT value FROM meta WHERE key = 'version'"));
  ASSERT_TRUE(s1.Step());
  file_version = s1.ColumnInt(0);
  EXPECT_EQ(cur_version, file_version);

  // Check visit_duration column in visits table is created and set to 0.
  s1.Assign(archived_db.GetUniqueStatement(
      "SELECT visit_duration FROM visits LIMIT 1"));
  ASSERT_TRUE(s1.Step());
  EXPECT_EQ(0, s1.ColumnInt(0));
}

}  // namespace history
