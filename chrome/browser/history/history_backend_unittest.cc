// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_backend.h"

#include <algorithm>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/in_memory_history_backend.h"
#include "chrome/browser/history/visit_filter.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/importer/imported_favicon_usage.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/in_memory_database.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/test/history_client_fake_bookmarks.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::Time;

// This file only tests functionality where it is most convenient to call the
// backend directly. Most of the history backend functions are tested by the
// history unit test. Because of the elaborate callbacks involved, this is no
// harder than calling it directly for many things.

namespace {

const int kTinyEdgeSize = 10;
const int kSmallEdgeSize = 16;
const int kLargeEdgeSize = 32;

const gfx::Size kTinySize = gfx::Size(kTinyEdgeSize, kTinyEdgeSize);
const gfx::Size kSmallSize = gfx::Size(kSmallEdgeSize, kSmallEdgeSize);
const gfx::Size kLargeSize = gfx::Size(kLargeEdgeSize, kLargeEdgeSize);

// Comparison functions as to make it easier to check results of
// GetFaviconBitmaps() and GetIconMappingsForPageURL().
bool IconMappingLessThan(const history::IconMapping& a,
                         const history::IconMapping& b) {
  return a.icon_url < b.icon_url;
}

bool FaviconBitmapLessThan(const history::FaviconBitmap& a,
                           const history::FaviconBitmap& b) {
  return a.pixel_size.GetArea() < b.pixel_size.GetArea();
}

class HistoryClientMock : public history::HistoryClientFakeBookmarks {
 public:
  MOCK_METHOD0(BlockUntilBookmarksLoaded, void());
};

}  // namespace

namespace history {

class HistoryBackendTestBase;

// This must be a separate object since HistoryBackend manages its lifetime.
// This just forwards the messages we're interested in to the test object.
class HistoryBackendTestDelegate : public HistoryBackend::Delegate {
 public:
  explicit HistoryBackendTestDelegate(HistoryBackendTestBase* test)
      : test_(test) {}

  virtual void NotifyProfileError(sql::InitStatus init_status) OVERRIDE {}
  virtual void SetInMemoryBackend(
      scoped_ptr<InMemoryHistoryBackend> backend) OVERRIDE;
  virtual void BroadcastNotifications(
      int type,
      scoped_ptr<HistoryDetails> details) OVERRIDE;
  virtual void DBLoaded() OVERRIDE;
  virtual void NotifyVisitDBObserversOnAddVisit(
      const BriefVisitInfo& info) OVERRIDE {}

 private:
  // Not owned by us.
  HistoryBackendTestBase* test_;

  DISALLOW_COPY_AND_ASSIGN(HistoryBackendTestDelegate);
};

class HistoryBackendTestBase : public testing::Test {
 public:
  typedef std::vector<std::pair<int, HistoryDetails*> > NotificationList;

  HistoryBackendTestBase()
      : loaded_(false),
        ui_thread_(content::BrowserThread::UI, &message_loop_) {}

  virtual ~HistoryBackendTestBase() {
    STLDeleteValues(&broadcasted_notifications_);
  }

 protected:
  int num_broadcasted_notifications() const {
    return broadcasted_notifications_.size();
  }

  const NotificationList& broadcasted_notifications() const {
    return broadcasted_notifications_;
  }

  void ClearBroadcastedNotifications() {
    STLDeleteValues(&broadcasted_notifications_);
  }

  base::FilePath test_dir() {
    return test_dir_;
  }

  void BroadcastNotifications(int type, scoped_ptr<HistoryDetails> details) {
    // Send the notifications directly to the in-memory database.
    content::Details<HistoryDetails> det(details.get());
    mem_backend_->Observe(
        type, content::Source<HistoryBackendTestBase>(NULL), det);

    // The backend passes ownership of the details pointer to us.
    broadcasted_notifications_.push_back(
        std::make_pair(type, details.release()));
  }

  history::HistoryClientFakeBookmarks history_client_;
  scoped_refptr<HistoryBackend> backend_;  // Will be NULL on init failure.
  scoped_ptr<InMemoryHistoryBackend> mem_backend_;
  bool loaded_;

 private:
  friend class HistoryBackendTestDelegate;

  // testing::Test
  virtual void SetUp() {
    if (!base::CreateNewTempDirectory(FILE_PATH_LITERAL("BackendTest"),
                                      &test_dir_))
      return;
    backend_ = new HistoryBackend(
        test_dir_, new HistoryBackendTestDelegate(this), &history_client_);
    backend_->Init(std::string(), false);
  }

  virtual void TearDown() {
    if (backend_.get())
      backend_->Closing();
    backend_ = NULL;
    mem_backend_.reset();
    base::DeleteFile(test_dir_, true);
    base::RunLoop().RunUntilIdle();
    history_client_.ClearAllBookmarks();
  }

  void SetInMemoryBackend(scoped_ptr<InMemoryHistoryBackend> backend) {
    mem_backend_.swap(backend);
  }

  // The types and details of notifications which were broadcasted.
  NotificationList broadcasted_notifications_;

  base::MessageLoop message_loop_;
  base::FilePath test_dir_;
  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(HistoryBackendTestBase);
};

void HistoryBackendTestDelegate::SetInMemoryBackend(
    scoped_ptr<InMemoryHistoryBackend> backend) {
  test_->SetInMemoryBackend(backend.Pass());
}

void HistoryBackendTestDelegate::BroadcastNotifications(
    int type,
    scoped_ptr<HistoryDetails> details) {
  test_->BroadcastNotifications(type, details.Pass());
}

void HistoryBackendTestDelegate::DBLoaded() {
  test_->loaded_ = true;
}

class HistoryBackendTest : public HistoryBackendTestBase {
 public:
  HistoryBackendTest() {}
  virtual ~HistoryBackendTest() {}

 protected:
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

    ContextID context_id = reinterpret_cast<ContextID>(1);
    history::HistoryAddPageArgs request(
        redirects.back(), time, context_id, page_id, GURL(),
        redirects, transition, history::SOURCE_BROWSED,
        true);
    backend_->AddPage(request);
  }

  // Adds CLIENT_REDIRECT page transition.
  // |url1| is the source URL and |url2| is the destination.
  // |did_replace| is true if the transition is non-user initiated and the
  // navigation entry for |url2| has replaced that for |url1|. The possibly
  // updated transition code of the visit records for |url1| and |url2| is
  // returned by filling in |*transition1| and |*transition2|, respectively.
  // |time| is a time of the redirect.
  void AddClientRedirect(const GURL& url1, const GURL& url2, bool did_replace,
                         base::Time time,
                         int* transition1, int* transition2) {
    ContextID dummy_context_id = reinterpret_cast<ContextID>(0x87654321);
    history::RedirectList redirects;
    if (url1.is_valid())
      redirects.push_back(url1);
    if (url2.is_valid())
      redirects.push_back(url2);
    HistoryAddPageArgs request(
        url2, time, dummy_context_id, 0, url1,
        redirects, content::PAGE_TRANSITION_CLIENT_REDIRECT,
        history::SOURCE_BROWSED, did_replace);
    backend_->AddPage(request);

    *transition1 = GetTransition(url1);
    *transition2 = GetTransition(url2);
  }

  int GetTransition(const GURL& url) {
    if (!url.is_valid())
      return 0;
    URLRow row;
    URLID id = backend_->db()->GetRowForURL(url, &row);
    VisitVector visits;
    EXPECT_TRUE(backend_->db()->GetVisitsForURL(id, &visits));
    return visits[0].transition;
  }

  // Returns a vector with the small edge size.
  const std::vector<int> GetEdgeSizesSmall() {
    std::vector<int> sizes_small;
    sizes_small.push_back(kSmallEdgeSize);
    return sizes_small;
  }

  // Returns a vector with the large edge size.
  const std::vector<int> GetEdgeSizesLarge() {
    std::vector<int> sizes_large;
    sizes_large.push_back(kLargeEdgeSize);
    return sizes_large;
  }

  // Returns a vector with the small and large edge sizes.
  const std::vector<int> GetEdgeSizesSmallAndLarge() {
    std::vector<int> sizes_small_and_large;
    sizes_small_and_large.push_back(kSmallEdgeSize);
    sizes_small_and_large.push_back(kLargeEdgeSize);
    return sizes_small_and_large;
  }

  // Returns a vector with the tiny, small, and large edge sizes.
  const std::vector<int> GetEdgeSizesTinySmallAndLarge() {
    std::vector<int> sizes_tiny_small_and_large;
    sizes_tiny_small_and_large.push_back(kTinyEdgeSize);
    sizes_tiny_small_and_large.push_back(kSmallEdgeSize);
    sizes_tiny_small_and_large.push_back(kLargeEdgeSize);
    return sizes_tiny_small_and_large;
  }

  // Returns the number of icon mappings of |icon_type| to |page_url|.
  size_t NumIconMappingsForPageURL(const GURL& page_url,
                                   favicon_base::IconType icon_type) {
    std::vector<IconMapping> icon_mappings;
    backend_->thumbnail_db_->GetIconMappingsForPageURL(page_url, icon_type,
                                                       &icon_mappings);
    return icon_mappings.size();
  }

  // Returns the icon mappings for |page_url| sorted alphabetically by icon
  // URL in ascending order. Returns true if there is at least one icon
  // mapping.
  bool GetSortedIconMappingsForPageURL(
      const GURL& page_url,
      std::vector<IconMapping>* icon_mappings) {
    if (!backend_->thumbnail_db_->GetIconMappingsForPageURL(page_url,
        icon_mappings)) {
      return false;
    }
    std::sort(icon_mappings->begin(), icon_mappings->end(),
              IconMappingLessThan);
    return true;
  }

  // Returns the favicon bitmaps for |icon_id| sorted by pixel size in
  // ascending order. Returns true if there is at least one favicon bitmap.
  bool GetSortedFaviconBitmaps(favicon_base::FaviconID icon_id,
                               std::vector<FaviconBitmap>* favicon_bitmaps) {
    if (!backend_->thumbnail_db_->GetFaviconBitmaps(icon_id, favicon_bitmaps))
      return false;
    std::sort(favicon_bitmaps->begin(), favicon_bitmaps->end(),
              FaviconBitmapLessThan);
    return true;
  }

  // Returns true if there is exactly one favicon bitmap associated to
  // |favicon_id|. If true, returns favicon bitmap in output parameter.
  bool GetOnlyFaviconBitmap(const favicon_base::FaviconID icon_id,
                            FaviconBitmap* favicon_bitmap) {
    std::vector<FaviconBitmap> favicon_bitmaps;
    if (!backend_->thumbnail_db_->GetFaviconBitmaps(icon_id, &favicon_bitmaps))
      return false;
    if (favicon_bitmaps.size() != 1)
      return false;
    *favicon_bitmap = favicon_bitmaps[0];
    return true;
  }

  // Generates |favicon_bitmap_data| with entries for the icon_urls and sizes
  // specified. The bitmap_data for entries are lowercase letters of the
  // alphabet starting at 'a' for the entry at index 0.
  void GenerateFaviconBitmapData(
      const GURL& icon_url1,
      const std::vector<int>& icon_url1_sizes,
      std::vector<favicon_base::FaviconRawBitmapData>* favicon_bitmap_data) {
    GenerateFaviconBitmapData(icon_url1, icon_url1_sizes, GURL(),
                              std::vector<int>(), favicon_bitmap_data);
  }

  void GenerateFaviconBitmapData(
      const GURL& icon_url1,
      const std::vector<int>& icon_url1_sizes,
      const GURL& icon_url2,
      const std::vector<int>& icon_url2_sizes,
      std::vector<favicon_base::FaviconRawBitmapData>* favicon_bitmap_data) {
    favicon_bitmap_data->clear();

    char bitmap_char = 'a';
    for (size_t i = 0; i < icon_url1_sizes.size(); ++i) {
      std::vector<unsigned char> data;
      data.push_back(bitmap_char);
      favicon_base::FaviconRawBitmapData bitmap_data_element;
      bitmap_data_element.bitmap_data =
          base::RefCountedBytes::TakeVector(&data);
      bitmap_data_element.pixel_size =
          gfx::Size(icon_url1_sizes[i], icon_url1_sizes[i]);
      bitmap_data_element.icon_url = icon_url1;
      favicon_bitmap_data->push_back(bitmap_data_element);

      ++bitmap_char;
    }

    for (size_t i = 0; i < icon_url2_sizes.size(); ++i) {
      std::vector<unsigned char> data;
      data.push_back(bitmap_char);
      favicon_base::FaviconRawBitmapData bitmap_data_element;
      bitmap_data_element.bitmap_data =
          base::RefCountedBytes::TakeVector(&data);
      bitmap_data_element.pixel_size =
          gfx::Size(icon_url2_sizes[i], icon_url2_sizes[i]);
      bitmap_data_element.icon_url = icon_url2;
      favicon_bitmap_data->push_back(bitmap_data_element);

      ++bitmap_char;
    }
  }

  // Returns true if |bitmap_data| is equal to |expected_data|.
  bool BitmapDataEqual(char expected_data,
                       scoped_refptr<base::RefCountedMemory> bitmap_data) {
    return bitmap_data.get() &&
           bitmap_data->size() == 1u &&
           *bitmap_data->front() == expected_data;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryBackendTest);
};

class InMemoryHistoryBackendTest : public HistoryBackendTestBase {
 public:
  InMemoryHistoryBackendTest() {}
  virtual ~InMemoryHistoryBackendTest() {}

 protected:
  void SimulateNotification(int type,
                            const URLRow* row1,
                            const URLRow* row2 = NULL,
                            const URLRow* row3 = NULL) {
    URLRows rows;
    rows.push_back(*row1);
    if (row2) rows.push_back(*row2);
    if (row3) rows.push_back(*row3);

    if (type == chrome::NOTIFICATION_HISTORY_URLS_MODIFIED) {
      scoped_ptr<URLsModifiedDetails> details(new URLsModifiedDetails());
      details->changed_urls.swap(rows);
      BroadcastNotifications(type, details.PassAs<HistoryDetails>());
    } else if (type == chrome::NOTIFICATION_HISTORY_URL_VISITED) {
      for (URLRows::const_iterator it = rows.begin(); it != rows.end(); ++it) {
        scoped_ptr<URLVisitedDetails> details(new URLVisitedDetails());
        details->row = *it;
        BroadcastNotifications(type, details.PassAs<HistoryDetails>());
      }
    } else if (type == chrome::NOTIFICATION_HISTORY_URLS_DELETED) {
      scoped_ptr<URLsDeletedDetails> details(new URLsDeletedDetails());
      details->rows = rows;
      BroadcastNotifications(type, details.PassAs<HistoryDetails>());
    } else {
      NOTREACHED();
    }
  }

  size_t GetNumberOfMatchingSearchTerms(const int keyword_id,
                                        const base::string16& prefix) {
    std::vector<KeywordSearchTermVisit> matching_terms;
    mem_backend_->db()->GetMostRecentKeywordSearchTerms(
        keyword_id, prefix, 1, &matching_terms);
    return matching_terms.size();
  }

  static URLRow CreateTestTypedURL() {
    URLRow url_row(GURL("https://www.google.com/"));
    url_row.set_id(10);
    url_row.set_title(base::UTF8ToUTF16("Google Search"));
    url_row.set_typed_count(1);
    url_row.set_visit_count(1);
    url_row.set_last_visit(Time::Now() - base::TimeDelta::FromHours(1));
    return url_row;
  }

  static URLRow CreateAnotherTestTypedURL() {
    URLRow url_row(GURL("https://maps.google.com/"));
    url_row.set_id(20);
    url_row.set_title(base::UTF8ToUTF16("Google Maps"));
    url_row.set_typed_count(2);
    url_row.set_visit_count(3);
    url_row.set_last_visit(Time::Now() - base::TimeDelta::FromHours(2));
    return url_row;
  }

  static URLRow CreateTestNonTypedURL() {
    URLRow url_row(GURL("https://news.google.com/"));
    url_row.set_id(30);
    url_row.set_title(base::UTF8ToUTF16("Google News"));
    url_row.set_visit_count(5);
    url_row.set_last_visit(Time::Now() - base::TimeDelta::FromHours(3));
    return url_row;
  }

  void PopulateTestURLsAndSearchTerms(URLRow* row1,
                                      URLRow* row2,
                                      const base::string16& term1,
                                      const base::string16& term2);

  void TestAddingAndChangingURLRows(int notification_type);

  static const KeywordID kTestKeywordId;
  static const char kTestSearchTerm1[];
  static const char kTestSearchTerm2[];

 private:
  DISALLOW_COPY_AND_ASSIGN(InMemoryHistoryBackendTest);
};

const KeywordID InMemoryHistoryBackendTest::kTestKeywordId = 42;
const char InMemoryHistoryBackendTest::kTestSearchTerm1[] = "banana";
const char InMemoryHistoryBackendTest::kTestSearchTerm2[] = "orange";

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

  // Add two favicons, each with two bitmaps. Note that we add favicon2 before
  // adding favicon1. This is so that favicon1 one gets ID 2 autoassigned to
  // the database, which will change when the other one is deleted. This way
  // we can test that updating works properly.
  GURL favicon_url1("http://www.google.com/favicon.ico");
  GURL favicon_url2("http://news.google.com/favicon.ico");
  favicon_base::FaviconID favicon2 =
      backend_->thumbnail_db_->AddFavicon(favicon_url2, favicon_base::FAVICON);
  favicon_base::FaviconID favicon1 =
      backend_->thumbnail_db_->AddFavicon(favicon_url1, favicon_base::FAVICON);

  std::vector<unsigned char> data;
  data.push_back('a');
  EXPECT_TRUE(backend_->thumbnail_db_->AddFaviconBitmap(favicon1,
      new base::RefCountedBytes(data), Time::Now(), kSmallSize));
  data[0] = 'b';
  EXPECT_TRUE(backend_->thumbnail_db_->AddFaviconBitmap(favicon1,
     new base::RefCountedBytes(data), Time::Now(), kLargeSize));

  data[0] = 'c';
  EXPECT_TRUE(backend_->thumbnail_db_->AddFaviconBitmap(favicon2,
      new base::RefCountedBytes(data), Time::Now(), kSmallSize));
  data[0] = 'd';
  EXPECT_TRUE(backend_->thumbnail_db_->AddFaviconBitmap(favicon2,
     new base::RefCountedBytes(data), Time::Now(), kLargeSize));

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

  visits.clear();
  backend_->db_->GetVisitsForURL(row2_id, &visits);
  ASSERT_EQ(1U, visits.size());

  // The in-memory backend should have been set and it should have gotten the
  // typed URL.
  ASSERT_TRUE(mem_backend_.get());
  URLRow outrow1;
  EXPECT_TRUE(mem_backend_->db_->GetRowForURL(row1.url(), NULL));

  // Star row1.
  history_client_.AddBookmark(row1.url());

  // Now finally clear all history.
  ClearBroadcastedNotifications();
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

  // We should have a favicon and favicon bitmaps for the first URL only. We
  // look them up by favicon URL since the IDs may have changed.
  favicon_base::FaviconID out_favicon1 =
      backend_->thumbnail_db_->GetFaviconIDForFaviconURL(
          favicon_url1, favicon_base::FAVICON, NULL);
  EXPECT_TRUE(out_favicon1);

  std::vector<FaviconBitmap> favicon_bitmaps;
  EXPECT_TRUE(backend_->thumbnail_db_->GetFaviconBitmaps(
      out_favicon1, &favicon_bitmaps));
  ASSERT_EQ(2u, favicon_bitmaps.size());

  FaviconBitmap favicon_bitmap1 = favicon_bitmaps[0];
  FaviconBitmap favicon_bitmap2 = favicon_bitmaps[1];

  // Favicon bitmaps do not need to be in particular order.
  if (favicon_bitmap1.pixel_size == kLargeSize) {
    FaviconBitmap tmp_favicon_bitmap = favicon_bitmap1;
    favicon_bitmap1 = favicon_bitmap2;
    favicon_bitmap2 = tmp_favicon_bitmap;
  }

  EXPECT_TRUE(BitmapDataEqual('a', favicon_bitmap1.bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmap1.pixel_size);

  EXPECT_TRUE(BitmapDataEqual('b', favicon_bitmap2.bitmap_data));
  EXPECT_EQ(kLargeSize, favicon_bitmap2.pixel_size);

  favicon_base::FaviconID out_favicon2 =
      backend_->thumbnail_db_->GetFaviconIDForFaviconURL(
          favicon_url2, favicon_base::FAVICON, NULL);
  EXPECT_FALSE(out_favicon2) << "Favicon not deleted";

  // The remaining URL should still reference the same favicon, even if its
  // ID has changed.
  std::vector<IconMapping> mappings;
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(
      outrow1.url(), favicon_base::FAVICON, &mappings));
  EXPECT_EQ(1u, mappings.size());
  EXPECT_EQ(out_favicon1, mappings[0].icon_id);

  // The first URL should still be bookmarked.
  EXPECT_TRUE(history_client_.IsBookmarked(row1.url()));

  // Check that we fire the notification about all history having been deleted.
  ASSERT_EQ(1u, broadcasted_notifications().size());
  ASSERT_EQ(chrome::NOTIFICATION_HISTORY_URLS_DELETED,
            broadcasted_notifications()[0].first);
  const URLsDeletedDetails* details = static_cast<const URLsDeletedDetails*>(
      broadcasted_notifications()[0].second);
  EXPECT_TRUE(details->all_history);
  EXPECT_FALSE(details->expired);
}

// Checks that adding a visit, then calling DeleteAll, and then trying to add
// data for the visited page works.  This can happen when clearing the history
// immediately after visiting a page.
TEST_F(HistoryBackendTest, DeleteAllThenAddData) {
  ASSERT_TRUE(backend_.get());

  Time visit_time = Time::Now();
  GURL url("http://www.google.com/");
  HistoryAddPageArgs request(url, visit_time, NULL, 0, GURL(),
                             history::RedirectList(),
                             content::PAGE_TRANSITION_KEYWORD_GENERATED,
                             history::SOURCE_BROWSED, false);
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

  // Try and set the title.
  backend_->SetPageTitle(url, base::UTF8ToUTF16("Title"));

  // The row should still be deleted.
  EXPECT_FALSE(backend_->db_->GetRowForURL(url, &outrow));

  // The visit should still be deleted.
  backend_->db_->GetAllVisitsInRange(Time(), Time(), 0, &all_visits);
  ASSERT_EQ(0U, all_visits.size());
}

TEST_F(HistoryBackendTest, URLsNoLongerBookmarked) {
  GURL favicon_url1("http://www.google.com/favicon.ico");
  GURL favicon_url2("http://news.google.com/favicon.ico");

  std::vector<unsigned char> data;
  data.push_back('1');
  favicon_base::FaviconID favicon1 =
      backend_->thumbnail_db_->AddFavicon(favicon_url1,
                                          favicon_base::FAVICON,
                                          new base::RefCountedBytes(data),
                                          Time::Now(),
                                          gfx::Size());

  data[0] = '2';
  favicon_base::FaviconID favicon2 =
      backend_->thumbnail_db_->AddFavicon(favicon_url2,
                                          favicon_base::FAVICON,
                                          new base::RefCountedBytes(data),
                                          Time::Now(),
                                          gfx::Size());

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
  history_client_.AddBookmark(row1.url());
  history_client_.AddBookmark(row2.url());

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
            backend_->thumbnail_db_->GetFaviconIDForFaviconURL(
                favicon_url2, favicon_base::FAVICON, NULL));

  // Unstar row2.
  history_client_.DelBookmark(row2.url());

  // Tell the backend it was unstarred. We have to explicitly do this as
  // BookmarkModel isn't wired up to the backend during testing.
  std::set<GURL> unstarred_urls;
  unstarred_urls.insert(row2.url());
  backend_->URLsNoLongerBookmarked(unstarred_urls);

  // The URL should no longer exist.
  EXPECT_FALSE(backend_->db_->GetRowForURL(row2.url(), &tmp_url_row));
  // And the favicon should be deleted.
  EXPECT_EQ(0,
            backend_->thumbnail_db_->GetFaviconIDForFaviconURL(
                favicon_url2, favicon_base::FAVICON, NULL));

  // Unstar row 1.
  history_client_.DelBookmark(row1.url());

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
            backend_->thumbnail_db_->GetFaviconIDForFaviconURL(
                favicon_url1, favicon_base::FAVICON, NULL));
}

// Tests a handful of assertions for a navigation with a type of
// KEYWORD_GENERATED.
TEST_F(HistoryBackendTest, KeywordGenerated) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://google.com");

  Time visit_time = Time::Now() - base::TimeDelta::FromDays(1);
  HistoryAddPageArgs request(url, visit_time, NULL, 0, GURL(),
                             history::RedirectList(),
                             content::PAGE_TRANSITION_KEYWORD_GENERATED,
                             history::SOURCE_BROWSED, false);
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
  QueryOptions query_options;
  query_options.max_count = 1;
  backend_->db()->GetVisibleVisitsInRange(query_options, &visits);
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

TEST_F(HistoryBackendTest, AddPagesWithDetails) {
  ASSERT_TRUE(backend_.get());

  // Import one non-typed URL, and two recent and one expired typed URLs.
  URLRow row1(GURL("https://news.google.com/"));
  row1.set_visit_count(1);
  row1.set_last_visit(Time::Now());
  URLRow row2(GURL("https://www.google.com/"));
  row2.set_typed_count(1);
  row2.set_last_visit(Time::Now());
  URLRow row3(GURL("https://mail.google.com/"));
  row3.set_visit_count(1);
  row3.set_typed_count(1);
  row3.set_last_visit(Time::Now() - base::TimeDelta::FromDays(7 - 1));
  URLRow row4(GURL("https://maps.google.com/"));
  row4.set_visit_count(1);
  row4.set_typed_count(1);
  row4.set_last_visit(Time::Now() - base::TimeDelta::FromDays(365 + 2));

  URLRows rows;
  rows.push_back(row1);
  rows.push_back(row2);
  rows.push_back(row3);
  rows.push_back(row4);
  backend_->AddPagesWithDetails(rows, history::SOURCE_BROWSED);

  // Verify that recent URLs have ended up in the main |db_|, while the already
  // expired URL has been ignored.
  URLRow stored_row1, stored_row2, stored_row3, stored_row4;
  EXPECT_NE(0, backend_->db_->GetRowForURL(row1.url(), &stored_row1));
  EXPECT_NE(0, backend_->db_->GetRowForURL(row2.url(), &stored_row2));
  EXPECT_NE(0, backend_->db_->GetRowForURL(row3.url(), &stored_row3));
  EXPECT_EQ(0, backend_->db_->GetRowForURL(row4.url(), &stored_row4));

  // Ensure that a notification was fired for both typed and non-typed URLs.
  // Further verify that the IDs in the notification are set to those that are
  // in effect in the main database. The InMemoryHistoryBackend relies on this
  // for caching.
  ASSERT_EQ(1u, broadcasted_notifications().size());
  ASSERT_EQ(chrome::NOTIFICATION_HISTORY_URLS_MODIFIED,
            broadcasted_notifications()[0].first);
  const URLsModifiedDetails* details = static_cast<const URLsModifiedDetails*>(
      broadcasted_notifications()[0].second);
  EXPECT_EQ(3u, details->changed_urls.size());

  URLRows::const_iterator it_row1 = std::find_if(
      details->changed_urls.begin(),
      details->changed_urls.end(),
      history::URLRow::URLRowHasURL(row1.url()));
  ASSERT_NE(details->changed_urls.end(), it_row1);
  EXPECT_EQ(stored_row1.id(), it_row1->id());

  URLRows::const_iterator it_row2 = std::find_if(
        details->changed_urls.begin(),
        details->changed_urls.end(),
        history::URLRow::URLRowHasURL(row2.url()));
  ASSERT_NE(details->changed_urls.end(), it_row2);
  EXPECT_EQ(stored_row2.id(), it_row2->id());

  URLRows::const_iterator it_row3 = std::find_if(
        details->changed_urls.begin(),
        details->changed_urls.end(),
        history::URLRow::URLRowHasURL(row3.url()));
  ASSERT_NE(details->changed_urls.end(), it_row3);
  EXPECT_EQ(stored_row3.id(), it_row3->id());
}

TEST_F(HistoryBackendTest, UpdateURLs) {
  ASSERT_TRUE(backend_.get());

  // Add three pages directly to the database.
  URLRow row1(GURL("https://news.google.com/"));
  row1.set_visit_count(1);
  row1.set_last_visit(Time::Now());
  URLRow row2(GURL("https://maps.google.com/"));
  row2.set_visit_count(2);
  row2.set_last_visit(Time::Now());
  URLRow row3(GURL("https://www.google.com/"));
  row3.set_visit_count(3);
  row3.set_last_visit(Time::Now());

  backend_->db_->AddURL(row1);
  backend_->db_->AddURL(row2);
  backend_->db_->AddURL(row3);

  // Now create changed versions of all URLRows by incrementing their visit
  // counts, and in the meantime, also delete the second row from the database.
  URLRow altered_row1, altered_row2, altered_row3;
  backend_->db_->GetRowForURL(row1.url(), &altered_row1);
  altered_row1.set_visit_count(42);
  backend_->db_->GetRowForURL(row2.url(), &altered_row2);
  altered_row2.set_visit_count(43);
  backend_->db_->GetRowForURL(row3.url(), &altered_row3);
  altered_row3.set_visit_count(44);

  backend_->db_->DeleteURLRow(altered_row2.id());

  // Now try to update all three rows at once. The change to the second URLRow
  // should be ignored, as it is no longer present in the DB.
  URLRows rows;
  rows.push_back(altered_row1);
  rows.push_back(altered_row2);
  rows.push_back(altered_row3);
  EXPECT_EQ(2u, backend_->UpdateURLs(rows));

  URLRow stored_row1, stored_row3;
  EXPECT_NE(0, backend_->db_->GetRowForURL(row1.url(), &stored_row1));
  EXPECT_NE(0, backend_->db_->GetRowForURL(row3.url(), &stored_row3));
  EXPECT_EQ(altered_row1.visit_count(), stored_row1.visit_count());
  EXPECT_EQ(altered_row3.visit_count(), stored_row3.visit_count());

  // Ensure that a notification was fired, and further verify that the IDs in
  // the notification are set to those that are in effect in the main database.
  // The InMemoryHistoryBackend relies on this for caching.
  ASSERT_EQ(1u, broadcasted_notifications().size());
  ASSERT_EQ(chrome::NOTIFICATION_HISTORY_URLS_MODIFIED,
            broadcasted_notifications()[0].first);
  const URLsModifiedDetails* details = static_cast<const URLsModifiedDetails*>(
      broadcasted_notifications()[0].second);
  EXPECT_EQ(2u, details->changed_urls.size());

  URLRows::const_iterator it_row1 =
      std::find_if(details->changed_urls.begin(),
                   details->changed_urls.end(),
                   history::URLRow::URLRowHasURL(row1.url()));
  ASSERT_NE(details->changed_urls.end(), it_row1);
  EXPECT_EQ(altered_row1.id(), it_row1->id());
  EXPECT_EQ(altered_row1.visit_count(), it_row1->visit_count());

  URLRows::const_iterator it_row3 =
      std::find_if(details->changed_urls.begin(),
                   details->changed_urls.end(),
                   history::URLRow::URLRowHasURL(row3.url()));
  ASSERT_NE(details->changed_urls.end(), it_row3);
  EXPECT_EQ(altered_row3.id(), it_row3->id());
  EXPECT_EQ(altered_row3.visit_count(), it_row3->visit_count());
}

// This verifies that a notification is fired. In-depth testing of logic should
// be done in HistoryTest.SetTitle.
TEST_F(HistoryBackendTest, SetPageTitleFiresNotificationWithCorrectDetails) {
  const char kTestUrlTitle[] = "Google Search";

  ASSERT_TRUE(backend_.get());

  // Add two pages, then change the title of the second one.
  URLRow row1(GURL("https://news.google.com/"));
  row1.set_typed_count(1);
  row1.set_last_visit(Time::Now());
  URLRow row2(GURL("https://www.google.com/"));
  row2.set_visit_count(2);
  row2.set_last_visit(Time::Now());

  URLRows rows;
  rows.push_back(row1);
  rows.push_back(row2);
  backend_->AddPagesWithDetails(rows, history::SOURCE_BROWSED);

  ClearBroadcastedNotifications();
  backend_->SetPageTitle(row2.url(), base::UTF8ToUTF16(kTestUrlTitle));

  // Ensure that a notification was fired, and further verify that the IDs in
  // the notification are set to those that are in effect in the main database.
  // The InMemoryHistoryBackend relies on this for caching.
  URLRow stored_row2;
  EXPECT_TRUE(backend_->GetURL(row2.url(), &stored_row2));
  ASSERT_EQ(1u, broadcasted_notifications().size());
  ASSERT_EQ(chrome::NOTIFICATION_HISTORY_URLS_MODIFIED,
            broadcasted_notifications()[0].first);
  const URLsModifiedDetails* details = static_cast<const URLsModifiedDetails*>(
      broadcasted_notifications()[0].second);
  ASSERT_EQ(1u, details->changed_urls.size());
  EXPECT_EQ(base::UTF8ToUTF16(kTestUrlTitle), details->changed_urls[0].title());
  EXPECT_EQ(stored_row2.id(), details->changed_urls[0].id());
}

// There's no importer on Android.
#if !defined(OS_ANDROID)
TEST_F(HistoryBackendTest, ImportedFaviconsTest) {
  // Setup test data - two Urls in the history, one with favicon assigned and
  // one without.
  GURL favicon_url1("http://www.google.com/favicon.ico");
  std::vector<unsigned char> data;
  data.push_back('1');
  favicon_base::FaviconID favicon1 = backend_->thumbnail_db_->AddFavicon(
      favicon_url1,
      favicon_base::FAVICON,
      base::RefCountedBytes::TakeVector(&data),
      Time::Now(),
      gfx::Size());
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
  EXPECT_EQ(1u, NumIconMappingsForPageURL(row1.url(), favicon_base::FAVICON));
  EXPECT_EQ(0u, NumIconMappingsForPageURL(row2.url(), favicon_base::FAVICON));

  // Now provide one imported favicon for both URLs already in the registry.
  // The new favicon should only be used with the URL that doesn't already have
  // a favicon.
  std::vector<ImportedFaviconUsage> favicons;
  ImportedFaviconUsage favicon;
  favicon.favicon_url = GURL("http://news.google.com/favicon.ico");
  favicon.png_data.push_back('2');
  favicon.urls.insert(row1.url());
  favicon.urls.insert(row2.url());
  favicons.push_back(favicon);
  backend_->SetImportedFavicons(favicons);
  EXPECT_FALSE(backend_->db_->GetRowForURL(row1.url(), &url_row1) == 0);
  EXPECT_FALSE(backend_->db_->GetRowForURL(row2.url(), &url_row2) == 0);

  std::vector<IconMapping> mappings;
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(
      row1.url(), favicon_base::FAVICON, &mappings));
  EXPECT_EQ(1u, mappings.size());
  EXPECT_EQ(favicon1, mappings[0].icon_id);
  EXPECT_EQ(favicon_url1, mappings[0].icon_url);

  mappings.clear();
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(
      row2.url(), favicon_base::FAVICON, &mappings));
  EXPECT_EQ(1u, mappings.size());
  EXPECT_EQ(favicon.favicon_url, mappings[0].icon_url);

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
  history_client_.AddBookmark(url3);
  backend_->SetImportedFavicons(favicons);
  EXPECT_FALSE(backend_->db_->GetRowForURL(url3, &url_row3) == 0);
  EXPECT_TRUE(url_row3.visit_count() == 0);
}
#endif  // !defined(OS_ANDROID)

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

TEST_F(HistoryBackendTest, AddPageVisitNotLastVisit) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://www.google.com");

  // Clear all history.
  backend_->DeleteAllHistory();

  // Create visit times
  base::Time recent_time = base::Time::Now();
  base::TimeDelta visit_age = base::TimeDelta::FromDays(3);
  base::Time older_time = recent_time - visit_age;

  // Visit the url with recent time.
  backend_->AddPageVisit(url, recent_time, 0,
      content::PageTransitionFromInt(
          content::PageTransitionGetQualifier(content::PAGE_TRANSITION_TYPED)),
      history::SOURCE_BROWSED);

  // Add to the url a visit with older time (could be syncing from another
  // client, etc.).
  backend_->AddPageVisit(url, older_time, 0,
      content::PageTransitionFromInt(
          content::PageTransitionGetQualifier(content::PAGE_TRANSITION_TYPED)),
      history::SOURCE_SYNCED);

  // Fetch the row information about url from history db.
  VisitVector visits;
  URLRow row;
  URLID row_id = backend_->db_->GetRowForURL(url, &row);
  backend_->db_->GetVisitsForURL(row_id, &visits);

  // Last visit time should be the most recent time, not the most recently added
  // visit.
  ASSERT_EQ(2U, visits.size());
  ASSERT_EQ(recent_time, row.last_visit());
}

TEST_F(HistoryBackendTest, AddPageVisitFiresNotificationWithCorrectDetails) {
  ASSERT_TRUE(backend_.get());

  GURL url1("http://www.google.com");
  GURL url2("http://maps.google.com");

  // Clear all history.
  backend_->DeleteAllHistory();
  ClearBroadcastedNotifications();

  // Visit two distinct URLs, the second one twice.
  backend_->AddPageVisit(url1, base::Time::Now(), 0,
                         content::PAGE_TRANSITION_LINK,
                         history::SOURCE_BROWSED);
  for (int i = 0; i < 2; ++i) {
    backend_->AddPageVisit(url2, base::Time::Now(), 0,
                           content::PAGE_TRANSITION_TYPED,
                           history::SOURCE_BROWSED);
  }

  URLRow stored_row1, stored_row2;
  EXPECT_NE(0, backend_->db_->GetRowForURL(url1, &stored_row1));
  EXPECT_NE(0, backend_->db_->GetRowForURL(url2, &stored_row2));

  // Expect that NOTIFICATION_HISTORY_URLS_VISITED has been fired 3x, and that
  // each time, the URLRows have the correct URLs and IDs set.
  ASSERT_EQ(3, num_broadcasted_notifications());
  ASSERT_EQ(chrome::NOTIFICATION_HISTORY_URL_VISITED,
            broadcasted_notifications()[0].first);
  const URLVisitedDetails* details = static_cast<const URLVisitedDetails*>(
      broadcasted_notifications()[0].second);
  EXPECT_EQ(content::PAGE_TRANSITION_LINK,
            content::PageTransitionStripQualifier(details->transition));
  EXPECT_EQ(stored_row1.id(), details->row.id());
  EXPECT_EQ(stored_row1.url(), details->row.url());

  // No further checking, this case analogous to the first one.
  ASSERT_EQ(chrome::NOTIFICATION_HISTORY_URL_VISITED,
            broadcasted_notifications()[1].first);

  ASSERT_EQ(chrome::NOTIFICATION_HISTORY_URL_VISITED,
            broadcasted_notifications()[2].first);
  details = static_cast<const URLVisitedDetails*>(
      broadcasted_notifications()[2].second);
  EXPECT_EQ(content::PAGE_TRANSITION_TYPED,
            content::PageTransitionStripQualifier(details->transition));
  EXPECT_EQ(stored_row2.id(), details->row.id());
  EXPECT_EQ(stored_row2.url(), details->row.url());
}

TEST_F(HistoryBackendTest, AddPageArgsSource) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://testpageargs.com");

  // Assume this page is browsed by user.
  HistoryAddPageArgs request1(url, base::Time::Now(), NULL, 0, GURL(),
                             history::RedirectList(),
                             content::PAGE_TRANSITION_KEYWORD_GENERATED,
                             history::SOURCE_BROWSED, false);
  backend_->AddPage(request1);
  // Assume this page is synced.
  HistoryAddPageArgs request2(url, base::Time::Now(), NULL, 0, GURL(),
                             history::RedirectList(),
                             content::PAGE_TRANSITION_LINK,
                             history::SOURCE_SYNCED, false);
  backend_->AddPage(request2);
  // Assume this page is browsed again.
  HistoryAddPageArgs request3(url, base::Time::Now(), NULL, 0, GURL(),
                             history::RedirectList(),
                             content::PAGE_TRANSITION_TYPED,
                             history::SOURCE_BROWSED, false);
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

  base::FilePath old_history_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &old_history_path));
  old_history_path = old_history_path.AppendASCII("History");
  old_history_path = old_history_path.AppendASCII("HistoryNoSource");

  // Copy history database file to current directory so that it will be deleted
  // in Teardown.
  base::FilePath new_history_path(test_dir());
  base::DeleteFile(new_history_path, true);
  base::CreateDirectory(new_history_path);
  base::FilePath new_history_file =
      new_history_path.Append(chrome::kHistoryFilename);
  ASSERT_TRUE(base::CopyFile(old_history_path, new_history_file));

  backend_ = new HistoryBackend(
      new_history_path, new HistoryBackendTestDelegate(this), &history_client_);
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

// Test that SetFaviconMappingsForPageAndRedirects correctly updates icon
// mappings based on redirects, icon URLs and icon types.
TEST_F(HistoryBackendTest, SetFaviconMappingsForPageAndRedirects) {
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

  const GURL icon_url1("http://www.google.com/icon");
  const GURL icon_url2("http://www.google.com/icon2");

  // Generate bitmap data for a page with two favicons.
  std::vector<favicon_base::FaviconRawBitmapData> two_favicon_bitmap_data;
  GenerateFaviconBitmapData(icon_url1, GetEdgeSizesSmallAndLarge(),
      icon_url2, GetEdgeSizesSmallAndLarge(), &two_favicon_bitmap_data);

  // Generate bitmap data for a page with a single favicon.
  std::vector<favicon_base::FaviconRawBitmapData> one_favicon_bitmap_data;
  GenerateFaviconBitmapData(icon_url1, GetEdgeSizesSmallAndLarge(),
                            &one_favicon_bitmap_data);

  // Add two favicons
  backend_->SetFavicons(url1, favicon_base::FAVICON, two_favicon_bitmap_data);
  EXPECT_EQ(2u, NumIconMappingsForPageURL(url1, favicon_base::FAVICON));
  EXPECT_EQ(2u, NumIconMappingsForPageURL(url2, favicon_base::FAVICON));

  // Add one touch_icon
  backend_->SetFavicons(
      url1, favicon_base::TOUCH_ICON, one_favicon_bitmap_data);
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url1, favicon_base::TOUCH_ICON));
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url2, favicon_base::TOUCH_ICON));
  EXPECT_EQ(2u, NumIconMappingsForPageURL(url1, favicon_base::FAVICON));

  // Add one TOUCH_PRECOMPOSED_ICON
  backend_->SetFavicons(
      url1, favicon_base::TOUCH_PRECOMPOSED_ICON, one_favicon_bitmap_data);
  // The touch_icon was replaced.
  EXPECT_EQ(0u, NumIconMappingsForPageURL(url1, favicon_base::TOUCH_ICON));
  EXPECT_EQ(2u, NumIconMappingsForPageURL(url1, favicon_base::FAVICON));
  EXPECT_EQ(
      1u,
      NumIconMappingsForPageURL(url1, favicon_base::TOUCH_PRECOMPOSED_ICON));
  EXPECT_EQ(
      1u,
      NumIconMappingsForPageURL(url2, favicon_base::TOUCH_PRECOMPOSED_ICON));

  // Add a touch_icon.
  backend_->SetFavicons(
      url1, favicon_base::TOUCH_ICON, one_favicon_bitmap_data);
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url1, favicon_base::TOUCH_ICON));
  EXPECT_EQ(2u, NumIconMappingsForPageURL(url1, favicon_base::FAVICON));
  // The TOUCH_PRECOMPOSED_ICON was replaced.
  EXPECT_EQ(
      0u,
      NumIconMappingsForPageURL(url1, favicon_base::TOUCH_PRECOMPOSED_ICON));

  // Add a single favicon.
  backend_->SetFavicons(url1, favicon_base::FAVICON, one_favicon_bitmap_data);
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url1, favicon_base::TOUCH_ICON));
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url1, favicon_base::FAVICON));
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url2, favicon_base::FAVICON));

  // Add two favicons.
  backend_->SetFavicons(url1, favicon_base::FAVICON, two_favicon_bitmap_data);
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url1, favicon_base::TOUCH_ICON));
  EXPECT_EQ(2u, NumIconMappingsForPageURL(url1, favicon_base::FAVICON));
}

// Test that there is no churn in icon mappings from calling
// SetFavicons() twice with the same |favicon_bitmap_data| parameter.
TEST_F(HistoryBackendTest, SetFaviconMappingsForPageDuplicates) {
  const GURL url("http://www.google.com/");
  const GURL icon_url("http://www.google.com/icon");

  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  GenerateFaviconBitmapData(icon_url, GetEdgeSizesSmallAndLarge(),
                            &favicon_bitmap_data);

  backend_->SetFavicons(url, favicon_base::FAVICON, favicon_bitmap_data);

  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(
      url, favicon_base::FAVICON, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  IconMappingID mapping_id = icon_mappings[0].mapping_id;

  backend_->SetFavicons(url, favicon_base::FAVICON, favicon_bitmap_data);

  icon_mappings.clear();
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(
      url, favicon_base::FAVICON, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());

  // The same row in the icon_mapping table should be used for the mapping as
  // before.
  EXPECT_EQ(mapping_id, icon_mappings[0].mapping_id);
}

// Test that calling SetFavicons() with FaviconBitmapData of different pixel
// sizes than the initially passed in FaviconBitmapData deletes the no longer
// used favicon bitmaps.
TEST_F(HistoryBackendTest, SetFaviconsDeleteBitmaps) {
  const GURL page_url("http://www.google.com/");
  const GURL icon_url("http://www.google.com/icon");

  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  GenerateFaviconBitmapData(icon_url, GetEdgeSizesSmallAndLarge(),
                            &favicon_bitmap_data);
  backend_->SetFavicons(page_url, favicon_base::FAVICON, favicon_bitmap_data);

  // Test initial state.
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(GetSortedIconMappingsForPageURL(page_url, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url, icon_mappings[0].icon_url);
  EXPECT_EQ(favicon_base::FAVICON, icon_mappings[0].icon_type);
  favicon_base::FaviconID favicon_id = icon_mappings[0].icon_id;

  std::vector<FaviconBitmap> favicon_bitmaps;
  EXPECT_TRUE(GetSortedFaviconBitmaps(favicon_id, &favicon_bitmaps));
  EXPECT_EQ(2u, favicon_bitmaps.size());
  FaviconBitmapID small_bitmap_id = favicon_bitmaps[0].bitmap_id;
  EXPECT_NE(0, small_bitmap_id);
  EXPECT_TRUE(BitmapDataEqual('a', favicon_bitmaps[0].bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmaps[0].pixel_size);
  FaviconBitmapID large_bitmap_id = favicon_bitmaps[1].bitmap_id;
  EXPECT_NE(0, large_bitmap_id);
  EXPECT_TRUE(BitmapDataEqual('b', favicon_bitmaps[1].bitmap_data));
  EXPECT_EQ(kLargeSize, favicon_bitmaps[1].pixel_size);

  // Call SetFavicons() with bitmap data for only the large bitmap. Check that
  // the small bitmap is in fact deleted.
  GenerateFaviconBitmapData(icon_url, GetEdgeSizesLarge(),
      &favicon_bitmap_data);
  backend_->SetFavicons(page_url, favicon_base::FAVICON, favicon_bitmap_data);

  scoped_refptr<base::RefCountedMemory> bitmap_data_out;
  gfx::Size pixel_size_out;
  EXPECT_FALSE(backend_->thumbnail_db_->GetFaviconBitmap(small_bitmap_id,
      NULL, &bitmap_data_out, &pixel_size_out));
  EXPECT_TRUE(backend_->thumbnail_db_->GetFaviconBitmap(large_bitmap_id,
      NULL, &bitmap_data_out, &pixel_size_out));
  EXPECT_TRUE(BitmapDataEqual('a', bitmap_data_out));
  EXPECT_EQ(kLargeSize, pixel_size_out);

  icon_mappings.clear();
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(page_url,
      &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(favicon_id, icon_mappings[0].icon_id);

  // Call SetFavicons() with no bitmap data. Check that the bitmaps and icon
  // mappings are deleted.
  favicon_bitmap_data.clear();
  backend_->SetFavicons(page_url, favicon_base::FAVICON, favicon_bitmap_data);

  EXPECT_FALSE(backend_->thumbnail_db_->GetFaviconBitmap(large_bitmap_id, NULL,
      NULL, NULL));
  icon_mappings.clear();
  EXPECT_FALSE(backend_->thumbnail_db_->GetIconMappingsForPageURL(page_url,
      &icon_mappings));

  // Notifications should have been broadcast for each call to SetFavicons().
  EXPECT_EQ(3, num_broadcasted_notifications());
}

// Test updating a single favicon bitmap's data via SetFavicons.
TEST_F(HistoryBackendTest, SetFaviconsReplaceBitmapData) {
  const GURL page_url("http://www.google.com/");
  const GURL icon_url("http://www.google.com/icon");

  std::vector<unsigned char> data_initial;
  data_initial.push_back('a');

  favicon_base::FaviconRawBitmapData bitmap_data_element;
  bitmap_data_element.bitmap_data =
      base::RefCountedBytes::TakeVector(&data_initial);
  bitmap_data_element.pixel_size = kSmallSize;
  bitmap_data_element.icon_url = icon_url;
  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  favicon_bitmap_data.push_back(bitmap_data_element);

  // Add bitmap to the database.
  backend_->SetFavicons(page_url, favicon_base::FAVICON, favicon_bitmap_data);

  favicon_base::FaviconID original_favicon_id =
      backend_->thumbnail_db_->GetFaviconIDForFaviconURL(
          icon_url, favicon_base::FAVICON, NULL);
  EXPECT_NE(0, original_favicon_id);
  FaviconBitmap original_favicon_bitmap;
  EXPECT_TRUE(
      GetOnlyFaviconBitmap(original_favicon_id, &original_favicon_bitmap));
  EXPECT_TRUE(BitmapDataEqual('a', original_favicon_bitmap.bitmap_data));

  EXPECT_EQ(1, num_broadcasted_notifications());

  // Call SetFavicons() with completely identical data.
  std::vector<unsigned char> updated_data;
  updated_data.push_back('a');
  favicon_bitmap_data[0].bitmap_data = new base::RefCountedBytes(updated_data);
  backend_->SetFavicons(page_url, favicon_base::FAVICON, favicon_bitmap_data);

  favicon_base::FaviconID updated_favicon_id =
      backend_->thumbnail_db_->GetFaviconIDForFaviconURL(
          icon_url, favicon_base::FAVICON, NULL);
  EXPECT_NE(0, updated_favicon_id);
  FaviconBitmap updated_favicon_bitmap;
  EXPECT_TRUE(
      GetOnlyFaviconBitmap(updated_favicon_id, &updated_favicon_bitmap));
  EXPECT_TRUE(BitmapDataEqual('a', updated_favicon_bitmap.bitmap_data));

  // Because the bitmap data is byte equivalent, no notifications should have
  // been broadcasted.
  EXPECT_EQ(1, num_broadcasted_notifications());

  // Call SetFavicons() with identical data but a different bitmap.
  updated_data[0] = 'b';
  favicon_bitmap_data[0].bitmap_data = new base::RefCountedBytes(updated_data);
  backend_->SetFavicons(page_url, favicon_base::FAVICON, favicon_bitmap_data);

  updated_favicon_id = backend_->thumbnail_db_->GetFaviconIDForFaviconURL(
      icon_url, favicon_base::FAVICON, NULL);
  EXPECT_NE(0, updated_favicon_id);
  EXPECT_TRUE(
      GetOnlyFaviconBitmap(updated_favicon_id, &updated_favicon_bitmap));
  EXPECT_TRUE(BitmapDataEqual('b', updated_favicon_bitmap.bitmap_data));

  // There should be no churn in FaviconIDs or FaviconBitmapIds even though
  // the bitmap data changed.
  EXPECT_EQ(original_favicon_bitmap.icon_id, updated_favicon_bitmap.icon_id);
  EXPECT_EQ(original_favicon_bitmap.bitmap_id,
            updated_favicon_bitmap.bitmap_id);

  // A notification should have been broadcasted as the favicon bitmap data has
  // changed.
  EXPECT_EQ(2, num_broadcasted_notifications());
}

// Test that if two pages share the same FaviconID, changing the favicon for
// one page does not affect the other.
TEST_F(HistoryBackendTest, SetFaviconsSameFaviconURLForTwoPages) {
  GURL icon_url("http://www.google.com/favicon.ico");
  GURL icon_url_new("http://www.google.com/favicon2.ico");
  GURL page_url1("http://www.google.com");
  GURL page_url2("http://www.google.ca");

  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  GenerateFaviconBitmapData(icon_url, GetEdgeSizesSmallAndLarge(),
                            &favicon_bitmap_data);

  backend_->SetFavicons(page_url1, favicon_base::FAVICON, favicon_bitmap_data);

  std::vector<GURL> icon_urls;
  icon_urls.push_back(icon_url);

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results;
  backend_->UpdateFaviconMappingsAndFetch(page_url2,
                                          icon_urls,
                                          favicon_base::FAVICON,
                                          GetEdgeSizesSmallAndLarge(),
                                          &bitmap_results);

  // Check that the same FaviconID is mapped to both page URLs.
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(
      page_url1, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  favicon_base::FaviconID favicon_id = icon_mappings[0].icon_id;
  EXPECT_NE(0, favicon_id);

  icon_mappings.clear();
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(
      page_url2, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(favicon_id, icon_mappings[0].icon_id);

  // Change the icon URL that |page_url1| is mapped to.
  GenerateFaviconBitmapData(icon_url_new, GetEdgeSizesSmall(),
                            &favicon_bitmap_data);
  backend_->SetFavicons(page_url1, favicon_base::FAVICON, favicon_bitmap_data);

  // |page_url1| should map to a new FaviconID and have valid bitmap data.
  icon_mappings.clear();
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(
      page_url1, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url_new, icon_mappings[0].icon_url);
  EXPECT_NE(favicon_id, icon_mappings[0].icon_id);

  std::vector<FaviconBitmap> favicon_bitmaps;
  EXPECT_TRUE(backend_->thumbnail_db_->GetFaviconBitmaps(
      icon_mappings[0].icon_id, &favicon_bitmaps));
  EXPECT_EQ(1u, favicon_bitmaps.size());

  // |page_url2| should still map to the same FaviconID and have valid bitmap
  // data.
  icon_mappings.clear();
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(
      page_url2, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(favicon_id, icon_mappings[0].icon_id);

  favicon_bitmaps.clear();
  EXPECT_TRUE(backend_->thumbnail_db_->GetFaviconBitmaps(favicon_id,
                                                         &favicon_bitmaps));
  EXPECT_EQ(2u, favicon_bitmaps.size());

  // A notification should have been broadcast for each call to SetFavicons()
  // and each call to UpdateFaviconMappingsAndFetch().
  EXPECT_EQ(3, num_broadcasted_notifications());
}

// Test that no notifications are broadcast as a result of calling
// UpdateFaviconMappingsAndFetch() for an icon URL which is already
// mapped to the passed in page URL.
TEST_F(HistoryBackendTest, UpdateFaviconMappingsAndFetchNoChange) {
  GURL page_url("http://www.google.com");
  GURL icon_url("http://www.google.com/favicon.ico");
  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  GenerateFaviconBitmapData(icon_url, GetEdgeSizesSmall(),
      &favicon_bitmap_data);

  backend_->SetFavicons(page_url, favicon_base::FAVICON, favicon_bitmap_data);

  favicon_base::FaviconID icon_id =
      backend_->thumbnail_db_->GetFaviconIDForFaviconURL(
          icon_url, favicon_base::FAVICON, NULL);
  EXPECT_NE(0, icon_id);
  EXPECT_EQ(1, num_broadcasted_notifications());

  std::vector<GURL> icon_urls;
  icon_urls.push_back(icon_url);

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results;
  backend_->UpdateFaviconMappingsAndFetch(page_url,
                                          icon_urls,
                                          favicon_base::FAVICON,
                                          GetEdgeSizesSmallAndLarge(),
                                          &bitmap_results);

  EXPECT_EQ(icon_id,
            backend_->thumbnail_db_->GetFaviconIDForFaviconURL(
                icon_url, favicon_base::FAVICON, NULL));

  // No notification should have been broadcast as no icon mapping, favicon,
  // or favicon bitmap was updated, added or removed.
  EXPECT_EQ(1, num_broadcasted_notifications());
}

// Test repeatedly calling MergeFavicon(). |page_url| is initially not known
// to the database.
TEST_F(HistoryBackendTest, MergeFaviconPageURLNotInDB) {
  GURL page_url("http://www.google.com");
  GURL icon_url("http:/www.google.com/favicon.ico");

  std::vector<unsigned char> data;
  data.push_back('a');
  scoped_refptr<base::RefCountedBytes> bitmap_data(
      new base::RefCountedBytes(data));

  backend_->MergeFavicon(
      page_url, icon_url, favicon_base::FAVICON, bitmap_data, kSmallSize);

  // |page_url| should now be mapped to |icon_url| and the favicon bitmap should
  // not be expired.
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(page_url,
      &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url, icon_mappings[0].icon_url);

  FaviconBitmap favicon_bitmap;
  EXPECT_TRUE(GetOnlyFaviconBitmap(icon_mappings[0].icon_id, &favicon_bitmap));
  EXPECT_NE(base::Time(), favicon_bitmap.last_updated);
  EXPECT_TRUE(BitmapDataEqual('a', favicon_bitmap.bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmap.pixel_size);

  data[0] = 'b';
  bitmap_data = new base::RefCountedBytes(data);
  backend_->MergeFavicon(
      page_url, icon_url, favicon_base::FAVICON, bitmap_data, kSmallSize);

  // |page_url| should still have a single favicon bitmap. The bitmap data
  // should be updated.
  icon_mappings.clear();
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(page_url,
      &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url, icon_mappings[0].icon_url);

  EXPECT_TRUE(GetOnlyFaviconBitmap(icon_mappings[0].icon_id, &favicon_bitmap));
  EXPECT_NE(base::Time(), favicon_bitmap.last_updated);
  EXPECT_TRUE(BitmapDataEqual('b', favicon_bitmap.bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmap.pixel_size);
}

// Test calling MergeFavicon() when |page_url| is known to the database.
TEST_F(HistoryBackendTest, MergeFaviconPageURLInDB) {
  GURL page_url("http://www.google.com");
  GURL icon_url1("http:/www.google.com/favicon.ico");
  GURL icon_url2("http://www.google.com/favicon2.ico");

  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  GenerateFaviconBitmapData(icon_url1, GetEdgeSizesSmall(),
                            &favicon_bitmap_data);

  backend_->SetFavicons(page_url, favicon_base::FAVICON, favicon_bitmap_data);

  // Test initial state.
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(page_url,
      &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url1, icon_mappings[0].icon_url);

  FaviconBitmap favicon_bitmap;
  EXPECT_TRUE(GetOnlyFaviconBitmap(icon_mappings[0].icon_id, &favicon_bitmap));
  EXPECT_NE(base::Time(), favicon_bitmap.last_updated);
  EXPECT_TRUE(BitmapDataEqual('a', favicon_bitmap.bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmap.pixel_size);

  EXPECT_EQ(1, num_broadcasted_notifications());

  // 1) Merge identical favicon bitmap.
  std::vector<unsigned char> data;
  data.push_back('a');
  scoped_refptr<base::RefCountedBytes> bitmap_data(
      new base::RefCountedBytes(data));
  backend_->MergeFavicon(
      page_url, icon_url1, favicon_base::FAVICON, bitmap_data, kSmallSize);

  // All the data should stay the same and no notifications should have been
  // sent.
  icon_mappings.clear();
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(page_url,
      &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url1, icon_mappings[0].icon_url);

  EXPECT_TRUE(GetOnlyFaviconBitmap(icon_mappings[0].icon_id, &favicon_bitmap));
  EXPECT_NE(base::Time(), favicon_bitmap.last_updated);
  EXPECT_TRUE(BitmapDataEqual('a', favicon_bitmap.bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmap.pixel_size);

  EXPECT_EQ(1, num_broadcasted_notifications());

  // 2) Merge favicon bitmap of the same size.
  data[0] = 'b';
  bitmap_data = new base::RefCountedBytes(data);
  backend_->MergeFavicon(
      page_url, icon_url1, favicon_base::FAVICON, bitmap_data, kSmallSize);

  // The small favicon bitmap at |icon_url1| should be overwritten.
  icon_mappings.clear();
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(page_url,
      &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url1, icon_mappings[0].icon_url);

  EXPECT_TRUE(GetOnlyFaviconBitmap(icon_mappings[0].icon_id, &favicon_bitmap));
  EXPECT_NE(base::Time(), favicon_bitmap.last_updated);
  EXPECT_TRUE(BitmapDataEqual('b', favicon_bitmap.bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmap.pixel_size);

  // 3) Merge favicon for the same icon URL, but a pixel size for which there is
  // no favicon bitmap.
  data[0] = 'c';
  bitmap_data = new base::RefCountedBytes(data);
  backend_->MergeFavicon(
      page_url, icon_url1, favicon_base::FAVICON, bitmap_data, kTinySize);

  // A new favicon bitmap should be created and the preexisting favicon bitmap
  // ('b') should be expired.
  icon_mappings.clear();
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(page_url,
      &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url1, icon_mappings[0].icon_url);

  std::vector<FaviconBitmap> favicon_bitmaps;
  EXPECT_TRUE(GetSortedFaviconBitmaps(icon_mappings[0].icon_id,
                                      &favicon_bitmaps));
  EXPECT_NE(base::Time(), favicon_bitmaps[0].last_updated);
  EXPECT_TRUE(BitmapDataEqual('c', favicon_bitmaps[0].bitmap_data));
  EXPECT_EQ(kTinySize, favicon_bitmaps[0].pixel_size);
  EXPECT_EQ(base::Time(), favicon_bitmaps[1].last_updated);
  EXPECT_TRUE(BitmapDataEqual('b', favicon_bitmaps[1].bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmaps[1].pixel_size);

  // 4) Merge favicon for an icon URL different from the icon URLs already
  // mapped to page URL.
  data[0] = 'd';
  bitmap_data = new base::RefCountedBytes(data);
  backend_->MergeFavicon(
      page_url, icon_url2, favicon_base::FAVICON, bitmap_data, kSmallSize);

  // The existing favicon bitmaps should be copied over to the newly created
  // favicon at |icon_url2|. |page_url| should solely be mapped to |icon_url2|.
  icon_mappings.clear();
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(page_url,
      &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url2, icon_mappings[0].icon_url);

  favicon_bitmaps.clear();
  EXPECT_TRUE(GetSortedFaviconBitmaps(icon_mappings[0].icon_id,
                                      &favicon_bitmaps));
  EXPECT_EQ(base::Time(), favicon_bitmaps[0].last_updated);
  EXPECT_TRUE(BitmapDataEqual('c', favicon_bitmaps[0].bitmap_data));
  EXPECT_EQ(kTinySize, favicon_bitmaps[0].pixel_size);
  // The favicon being merged should take precedence over the preexisting
  // favicon bitmaps.
  EXPECT_NE(base::Time(), favicon_bitmaps[1].last_updated);
  EXPECT_TRUE(BitmapDataEqual('d', favicon_bitmaps[1].bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmaps[1].pixel_size);

  // A notification should have been broadcast for each call to SetFavicons()
  // and MergeFavicon().
  EXPECT_EQ(4, num_broadcasted_notifications());
}

// Test calling MergeFavicon() when |icon_url| is known to the database but not
// mapped to |page_url|.
TEST_F(HistoryBackendTest, MergeFaviconIconURLMappedToDifferentPageURL) {
  GURL page_url1("http://www.google.com");
  GURL page_url2("http://news.google.com");
  GURL page_url3("http://maps.google.com");
  GURL icon_url("http:/www.google.com/favicon.ico");

  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  GenerateFaviconBitmapData(icon_url, GetEdgeSizesSmall(),
                            &favicon_bitmap_data);

  backend_->SetFavicons(page_url1, favicon_base::FAVICON, favicon_bitmap_data);

  // Test initial state.
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(page_url1,
      &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url, icon_mappings[0].icon_url);

  FaviconBitmap favicon_bitmap;
  EXPECT_TRUE(GetOnlyFaviconBitmap(icon_mappings[0].icon_id, &favicon_bitmap));
  EXPECT_NE(base::Time(), favicon_bitmap.last_updated);
  EXPECT_TRUE(BitmapDataEqual('a', favicon_bitmap.bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmap.pixel_size);

  // 1) Merge in an identical favicon bitmap data but for a different page URL.
  std::vector<unsigned char> data;
  data.push_back('a');
  scoped_refptr<base::RefCountedBytes> bitmap_data(
      new base::RefCountedBytes(data));

  backend_->MergeFavicon(
      page_url2, icon_url, favicon_base::FAVICON, bitmap_data, kSmallSize);

  favicon_base::FaviconID favicon_id =
      backend_->thumbnail_db_->GetFaviconIDForFaviconURL(
          icon_url, favicon_base::FAVICON, NULL);
  EXPECT_NE(0, favicon_id);

  EXPECT_TRUE(GetOnlyFaviconBitmap(favicon_id, &favicon_bitmap));
  EXPECT_NE(base::Time(), favicon_bitmap.last_updated);
  EXPECT_TRUE(BitmapDataEqual('a', favicon_bitmap.bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmap.pixel_size);

  // 2) Merging a favicon bitmap with different bitmap data for the same icon
  // URL should overwrite the small favicon bitmap at |icon_url|.
  bitmap_data->data()[0] = 'b';
  backend_->MergeFavicon(
      page_url3, icon_url, favicon_base::FAVICON, bitmap_data, kSmallSize);

  favicon_id = backend_->thumbnail_db_->GetFaviconIDForFaviconURL(
      icon_url, favicon_base::FAVICON, NULL);
  EXPECT_NE(0, favicon_id);

  EXPECT_TRUE(GetOnlyFaviconBitmap(favicon_id, &favicon_bitmap));
  EXPECT_NE(base::Time(), favicon_bitmap.last_updated);
  EXPECT_TRUE(BitmapDataEqual('b', favicon_bitmap.bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmap.pixel_size);

  // |icon_url| should be mapped to all three page URLs.
  icon_mappings.clear();
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(page_url1,
      &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(favicon_id, icon_mappings[0].icon_id);

  icon_mappings.clear();
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(page_url2,
      &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(favicon_id, icon_mappings[0].icon_id);

  icon_mappings.clear();
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(page_url3,
      &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(favicon_id, icon_mappings[0].icon_id);

  // A notification should have been broadcast for each call to SetFavicons()
  // and MergeFavicon().
  EXPECT_EQ(3, num_broadcasted_notifications());
}

// Test that MergeFavicon() does not add more than
// |kMaxFaviconBitmapsPerIconURL| to a favicon.
TEST_F(HistoryBackendTest, MergeFaviconMaxFaviconBitmapsPerIconURL) {
  GURL page_url("http://www.google.com");
  std::string icon_url_string("http://www.google.com/favicon.ico");
  size_t replace_index = icon_url_string.size() - 1;

  std::vector<unsigned char> data;
  data.push_back('a');
  scoped_refptr<base::RefCountedMemory> bitmap_data =
      base::RefCountedBytes::TakeVector(&data);

  int pixel_size = 1;
  for (size_t i = 0; i < kMaxFaviconBitmapsPerIconURL + 1; ++i) {
    icon_url_string[replace_index] = '0' + i;
    GURL icon_url(icon_url_string);

    backend_->MergeFavicon(page_url,
                           icon_url,
                           favicon_base::FAVICON,
                           bitmap_data,
                           gfx::Size(pixel_size, pixel_size));
    ++pixel_size;
  }

  // There should be a single favicon mapped to |page_url| with exactly
  // kMaxFaviconBitmapsPerIconURL favicon bitmaps.
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(page_url,
      &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  std::vector<FaviconBitmap> favicon_bitmaps;
  EXPECT_TRUE(backend_->thumbnail_db_->GetFaviconBitmaps(
      icon_mappings[0].icon_id, &favicon_bitmaps));
  EXPECT_EQ(kMaxFaviconBitmapsPerIconURL, favicon_bitmaps.size());
}

// Tests that the favicon set by MergeFavicon() shows up in the result of
// GetFaviconsForURL().
TEST_F(HistoryBackendTest, MergeFaviconShowsUpInGetFaviconsForURLResult) {
  GURL page_url("http://www.google.com");
  GURL icon_url("http://www.google.com/favicon.ico");
  GURL merged_icon_url("http://wwww.google.com/favicon2.ico");

  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  GenerateFaviconBitmapData(icon_url, GetEdgeSizesSmallAndLarge(),
                            &favicon_bitmap_data);

  // Set some preexisting favicons for |page_url|.
  backend_->SetFavicons(page_url, favicon_base::FAVICON, favicon_bitmap_data);

  // Merge small favicon.
  std::vector<unsigned char> data;
  data.push_back('c');
  scoped_refptr<base::RefCountedBytes> bitmap_data(
      new base::RefCountedBytes(data));
  backend_->MergeFavicon(page_url,
                         merged_icon_url,
                         favicon_base::FAVICON,
                         bitmap_data,
                         kSmallSize);

  // Request favicon bitmaps for both 1x and 2x to simulate request done by
  // BookmarkModel::GetFavicon().
  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results;
  backend_->GetFaviconsForURL(page_url,
                              favicon_base::FAVICON,
                              GetEdgeSizesSmallAndLarge(),
                              &bitmap_results);

  EXPECT_EQ(2u, bitmap_results.size());
  const favicon_base::FaviconRawBitmapResult& first_result = bitmap_results[0];
  const favicon_base::FaviconRawBitmapResult& result =
      (first_result.pixel_size == kSmallSize) ? first_result
                                              : bitmap_results[1];
  EXPECT_TRUE(BitmapDataEqual('c', result.bitmap_data));
}

// Tests GetFaviconsForURL with icon_types priority,
TEST_F(HistoryBackendTest, TestGetFaviconsForURLWithIconTypesPriority) {
  GURL page_url("http://www.google.com");
  GURL icon_url("http://www.google.com/favicon.ico");
  GURL touch_icon_url("http://wwww.google.com/touch_icon.ico");

  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  std::vector<int> favicon_size;
  favicon_size.push_back(16);
  favicon_size.push_back(32);
  GenerateFaviconBitmapData(icon_url, favicon_size, &favicon_bitmap_data);
  ASSERT_EQ(2u, favicon_bitmap_data.size());

  std::vector<favicon_base::FaviconRawBitmapData> touch_icon_bitmap_data;
  std::vector<int> touch_icon_size;
  touch_icon_size.push_back(64);
  GenerateFaviconBitmapData(icon_url, touch_icon_size, &touch_icon_bitmap_data);
  ASSERT_EQ(1u, touch_icon_bitmap_data.size());

  // Set some preexisting favicons for |page_url|.
  backend_->SetFavicons(page_url, favicon_base::FAVICON, favicon_bitmap_data);
  backend_->SetFavicons(
      page_url, favicon_base::TOUCH_ICON, touch_icon_bitmap_data);

  favicon_base::FaviconRawBitmapResult result;
  std::vector<int> icon_types;
  icon_types.push_back(favicon_base::FAVICON);
  icon_types.push_back(favicon_base::TOUCH_ICON);

  backend_->GetLargestFaviconForURL(page_url, icon_types, 16, &result);

  // Verify the result icon is 32x32 favicon.
  EXPECT_EQ(gfx::Size(32, 32), result.pixel_size);
  EXPECT_EQ(favicon_base::FAVICON, result.icon_type);

  // Change Minimal size to 32x32 and verify the 64x64 touch icon returned.
  backend_->GetLargestFaviconForURL(page_url, icon_types, 32, &result);
  EXPECT_EQ(gfx::Size(64, 64), result.pixel_size);
  EXPECT_EQ(favicon_base::TOUCH_ICON, result.icon_type);
}

// Test the the first types of icon is returned if its size equal to the
// second types icon.
TEST_F(HistoryBackendTest, TestGetFaviconsForURLReturnFavicon) {
  GURL page_url("http://www.google.com");
  GURL icon_url("http://www.google.com/favicon.ico");
  GURL touch_icon_url("http://wwww.google.com/touch_icon.ico");

  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  std::vector<int> favicon_size;
  favicon_size.push_back(16);
  favicon_size.push_back(32);
  GenerateFaviconBitmapData(icon_url, favicon_size, &favicon_bitmap_data);
  ASSERT_EQ(2u, favicon_bitmap_data.size());

  std::vector<favicon_base::FaviconRawBitmapData> touch_icon_bitmap_data;
  std::vector<int> touch_icon_size;
  touch_icon_size.push_back(32);
  GenerateFaviconBitmapData(icon_url, touch_icon_size, &touch_icon_bitmap_data);
  ASSERT_EQ(1u, touch_icon_bitmap_data.size());

  // Set some preexisting favicons for |page_url|.
  backend_->SetFavicons(page_url, favicon_base::FAVICON, favicon_bitmap_data);
  backend_->SetFavicons(
      page_url, favicon_base::TOUCH_ICON, touch_icon_bitmap_data);

  favicon_base::FaviconRawBitmapResult result;
  std::vector<int> icon_types;
  icon_types.push_back(favicon_base::FAVICON);
  icon_types.push_back(favicon_base::TOUCH_ICON);

  backend_->GetLargestFaviconForURL(page_url, icon_types, 16, &result);

  // Verify the result icon is 32x32 favicon.
  EXPECT_EQ(gfx::Size(32, 32), result.pixel_size);
  EXPECT_EQ(favicon_base::FAVICON, result.icon_type);

  // Change minimal size to 32x32 and verify the 32x32 favicon returned.
  favicon_base::FaviconRawBitmapResult result1;
  backend_->GetLargestFaviconForURL(page_url, icon_types, 32, &result1);
  EXPECT_EQ(gfx::Size(32, 32), result1.pixel_size);
  EXPECT_EQ(favicon_base::FAVICON, result1.icon_type);
}

// Test the favicon is returned if its size is smaller than minimal size,
// because it is only one available.
TEST_F(HistoryBackendTest, TestGetFaviconsForURLReturnFaviconEvenItSmaller) {
  GURL page_url("http://www.google.com");
  GURL icon_url("http://www.google.com/favicon.ico");

  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  std::vector<int> favicon_size;
  favicon_size.push_back(16);
  GenerateFaviconBitmapData(icon_url, favicon_size, &favicon_bitmap_data);
  ASSERT_EQ(1u, favicon_bitmap_data.size());

  // Set preexisting favicons for |page_url|.
  backend_->SetFavicons(page_url, favicon_base::FAVICON, favicon_bitmap_data);

  favicon_base::FaviconRawBitmapResult result;
  std::vector<int> icon_types;
  icon_types.push_back(favicon_base::FAVICON);
  icon_types.push_back(favicon_base::TOUCH_ICON);

  backend_->GetLargestFaviconForURL(page_url, icon_types, 32, &result);

  // Verify 16x16 icon is returned, even it small than minimal_size.
  EXPECT_EQ(gfx::Size(16, 16), result.pixel_size);
  EXPECT_EQ(favicon_base::FAVICON, result.icon_type);
}

// Test UpdateFaviconMapingsAndFetch() when multiple icon types are passed in.
TEST_F(HistoryBackendTest, UpdateFaviconMappingsAndFetchMultipleIconTypes) {
  GURL page_url1("http://www.google.com");
  GURL page_url2("http://news.google.com");
  GURL page_url3("http://mail.google.com");
  GURL icon_urla("http://www.google.com/favicon1.ico");
  GURL icon_urlb("http://www.google.com/favicon2.ico");
  GURL icon_urlc("http://www.google.com/favicon3.ico");

  // |page_url1| is mapped to |icon_urla| which if of type TOUCH_ICON.
  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  GenerateFaviconBitmapData(icon_urla, GetEdgeSizesSmall(),
      &favicon_bitmap_data);
  backend_->SetFavicons(
      page_url1, favicon_base::TOUCH_ICON, favicon_bitmap_data);

  // |page_url2| is mapped to |icon_urlb| and |icon_urlc| which are of type
  // TOUCH_PRECOMPOSED_ICON.
  GenerateFaviconBitmapData(icon_urlb, GetEdgeSizesSmall(), icon_urlc,
                            GetEdgeSizesSmall(), &favicon_bitmap_data);
  backend_->SetFavicons(
      page_url2, favicon_base::TOUCH_PRECOMPOSED_ICON, favicon_bitmap_data);

  std::vector<GURL> icon_urls;
  icon_urls.push_back(icon_urla);
  icon_urls.push_back(icon_urlb);
  icon_urls.push_back(icon_urlc);

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results;
  backend_->UpdateFaviconMappingsAndFetch(
      page_url3,
      icon_urls,
      (favicon_base::TOUCH_ICON | favicon_base::TOUCH_PRECOMPOSED_ICON),
      GetEdgeSizesSmallAndLarge(),
      &bitmap_results);

  // |page_url1| and |page_url2| should still be mapped to the same icon URLs.
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(page_url1,
      &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_urla, icon_mappings[0].icon_url);
  EXPECT_EQ(favicon_base::TOUCH_ICON, icon_mappings[0].icon_type);

  icon_mappings.clear();
  EXPECT_TRUE(GetSortedIconMappingsForPageURL(page_url2, &icon_mappings));
  EXPECT_EQ(2u, icon_mappings.size());
  EXPECT_EQ(icon_urlb, icon_mappings[0].icon_url);
  EXPECT_EQ(favicon_base::TOUCH_PRECOMPOSED_ICON, icon_mappings[0].icon_type);
  EXPECT_EQ(icon_urlc, icon_mappings[1].icon_url);
  EXPECT_EQ(favicon_base::TOUCH_PRECOMPOSED_ICON, icon_mappings[1].icon_type);

  // |page_url3| should be mapped only to |icon_urlb| and |icon_urlc| as
  // TOUCH_PRECOMPOSED_ICON is the largest IconType.
  icon_mappings.clear();
  EXPECT_TRUE(GetSortedIconMappingsForPageURL(page_url3, &icon_mappings));
  EXPECT_EQ(2u, icon_mappings.size());
  EXPECT_EQ(icon_urlb, icon_mappings[0].icon_url);
  EXPECT_EQ(favicon_base::TOUCH_PRECOMPOSED_ICON, icon_mappings[0].icon_type);
  EXPECT_EQ(icon_urlc, icon_mappings[1].icon_url);
  EXPECT_EQ(favicon_base::TOUCH_PRECOMPOSED_ICON, icon_mappings[1].icon_type);
}

// Test the results of GetFaviconsFromDB() when there are no found
// favicons.
TEST_F(HistoryBackendTest, GetFaviconsFromDBEmpty) {
  const GURL page_url("http://www.google.com/");

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results;
  EXPECT_FALSE(backend_->GetFaviconsFromDB(page_url,
                                           favicon_base::FAVICON,
                                           GetEdgeSizesSmallAndLarge(),
                                           &bitmap_results));
  EXPECT_TRUE(bitmap_results.empty());
}

// Test the results of GetFaviconsFromDB() when there are matching favicons
// but there are no associated favicon bitmaps.
TEST_F(HistoryBackendTest, GetFaviconsFromDBNoFaviconBitmaps) {
  const GURL page_url("http://www.google.com/");
  const GURL icon_url("http://www.google.com/icon1");

  favicon_base::FaviconID icon_id =
      backend_->thumbnail_db_->AddFavicon(icon_url, favicon_base::FAVICON);
  EXPECT_NE(0, icon_id);
  EXPECT_NE(0, backend_->thumbnail_db_->AddIconMapping(page_url, icon_id));

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results_out;
  EXPECT_FALSE(backend_->GetFaviconsFromDB(page_url,
                                           favicon_base::FAVICON,
                                           GetEdgeSizesSmallAndLarge(),
                                           &bitmap_results_out));
  EXPECT_TRUE(bitmap_results_out.empty());
}

// Test that GetFaviconsFromDB() returns results for the bitmaps which most
// closely match the passed in the desired pixel sizes.
TEST_F(HistoryBackendTest, GetFaviconsFromDBSelectClosestMatch) {
  const GURL page_url("http://www.google.com/");
  const GURL icon_url("http://www.google.com/icon1");

  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  GenerateFaviconBitmapData(icon_url, GetEdgeSizesTinySmallAndLarge(),
                            &favicon_bitmap_data);

  backend_->SetFavicons(page_url, favicon_base::FAVICON, favicon_bitmap_data);

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results_out;
  EXPECT_TRUE(backend_->GetFaviconsFromDB(page_url,
                                          favicon_base::FAVICON,
                                          GetEdgeSizesSmallAndLarge(),
                                          &bitmap_results_out));

  // The bitmap data for the small and large bitmaps should be returned as their
  // sizes match exactly.
  EXPECT_EQ(2u, bitmap_results_out.size());
  // No required order for results.
  if (bitmap_results_out[0].pixel_size == kLargeSize) {
    favicon_base::FaviconRawBitmapResult tmp_result = bitmap_results_out[0];
    bitmap_results_out[0] = bitmap_results_out[1];
    bitmap_results_out[1] = tmp_result;
  }

  EXPECT_FALSE(bitmap_results_out[0].expired);
  EXPECT_TRUE(BitmapDataEqual('b', bitmap_results_out[0].bitmap_data));
  EXPECT_EQ(kSmallSize, bitmap_results_out[0].pixel_size);
  EXPECT_EQ(icon_url, bitmap_results_out[0].icon_url);
  EXPECT_EQ(favicon_base::FAVICON, bitmap_results_out[0].icon_type);

  EXPECT_FALSE(bitmap_results_out[1].expired);
  EXPECT_TRUE(BitmapDataEqual('c', bitmap_results_out[1].bitmap_data));
  EXPECT_EQ(kLargeSize, bitmap_results_out[1].pixel_size);
  EXPECT_EQ(icon_url, bitmap_results_out[1].icon_url);
  EXPECT_EQ(favicon_base::FAVICON, bitmap_results_out[1].icon_type);
}

// Test that GetFaviconsFromDB() returns results from the icon URL whose
// bitmaps most closely match the passed in desired sizes.
TEST_F(HistoryBackendTest, GetFaviconsFromDBSingleIconURL) {
  const GURL page_url("http://www.google.com/");

  const GURL icon_url1("http://www.google.com/icon1");
  const GURL icon_url2("http://www.google.com/icon2");

  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  GenerateFaviconBitmapData(icon_url1, GetEdgeSizesSmall(), icon_url2,
                            GetEdgeSizesLarge(), &favicon_bitmap_data);

  backend_->SetFavicons(page_url, favicon_base::FAVICON, favicon_bitmap_data);

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results_out;
  EXPECT_TRUE(backend_->GetFaviconsFromDB(page_url,
                                          favicon_base::FAVICON,
                                          GetEdgeSizesSmallAndLarge(),
                                          &bitmap_results_out));

  // The results should have results for the icon URL with the large bitmap as
  // downscaling is preferred to upscaling.
  EXPECT_EQ(1u, bitmap_results_out.size());
  EXPECT_EQ(kLargeSize, bitmap_results_out[0].pixel_size);
  EXPECT_EQ(icon_url2, bitmap_results_out[0].icon_url);
}

// Test the results of GetFaviconsFromDB() when called with different
// |icon_types|.
TEST_F(HistoryBackendTest, GetFaviconsFromDBIconType) {
  const GURL page_url("http://www.google.com/");
  const GURL icon_url1("http://www.google.com/icon1.png");
  const GURL icon_url2("http://www.google.com/icon2.png");

  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  GenerateFaviconBitmapData(icon_url1, GetEdgeSizesSmall(),
      &favicon_bitmap_data);
  backend_->SetFavicons(page_url, favicon_base::FAVICON, favicon_bitmap_data);

  GenerateFaviconBitmapData(
      icon_url2, GetEdgeSizesSmall(), &favicon_bitmap_data);
  backend_->SetFavicons(
      page_url, favicon_base::TOUCH_ICON, favicon_bitmap_data);

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results_out;
  EXPECT_TRUE(backend_->GetFaviconsFromDB(page_url,
                                          favicon_base::FAVICON,
                                          GetEdgeSizesSmallAndLarge(),
                                          &bitmap_results_out));

  EXPECT_EQ(1u, bitmap_results_out.size());
  EXPECT_EQ(favicon_base::FAVICON, bitmap_results_out[0].icon_type);
  EXPECT_EQ(icon_url1, bitmap_results_out[0].icon_url);

  bitmap_results_out.clear();
  EXPECT_TRUE(backend_->GetFaviconsFromDB(page_url,
                                          favicon_base::TOUCH_ICON,
                                          GetEdgeSizesSmallAndLarge(),
                                          &bitmap_results_out));

  EXPECT_EQ(1u, bitmap_results_out.size());
  EXPECT_EQ(favicon_base::TOUCH_ICON, bitmap_results_out[0].icon_type);
  EXPECT_EQ(icon_url2, bitmap_results_out[0].icon_url);
}

// Test that GetFaviconsFromDB() correctly sets the expired flag for bitmap
// reults.
TEST_F(HistoryBackendTest, GetFaviconsFromDBExpired) {
  const GURL page_url("http://www.google.com/");
  const GURL icon_url("http://www.google.com/icon.png");

  std::vector<unsigned char> data;
  data.push_back('a');
  scoped_refptr<base::RefCountedBytes> bitmap_data(
      base::RefCountedBytes::TakeVector(&data));
  base::Time last_updated = base::Time::FromTimeT(0);
  favicon_base::FaviconID icon_id = backend_->thumbnail_db_->AddFavicon(
      icon_url, favicon_base::FAVICON, bitmap_data, last_updated, kSmallSize);
  EXPECT_NE(0, icon_id);
  EXPECT_NE(0, backend_->thumbnail_db_->AddIconMapping(page_url, icon_id));

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results_out;
  EXPECT_TRUE(backend_->GetFaviconsFromDB(page_url,
                                          favicon_base::FAVICON,
                                          GetEdgeSizesSmallAndLarge(),
                                          &bitmap_results_out));

  EXPECT_EQ(1u, bitmap_results_out.size());
  EXPECT_TRUE(bitmap_results_out[0].expired);
}

// Check that UpdateFaviconMappingsAndFetch() call back to the UI when there is
// no valid thumbnail database.
TEST_F(HistoryBackendTest, UpdateFaviconMappingsAndFetchNoDB) {
  // Make the thumbnail database invalid.
  backend_->thumbnail_db_.reset();

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results;

  backend_->UpdateFaviconMappingsAndFetch(GURL(),
                                          std::vector<GURL>(),
                                          favicon_base::FAVICON,
                                          GetEdgeSizesSmallAndLarge(),
                                          &bitmap_results);

  EXPECT_TRUE(bitmap_results.empty());
}

TEST_F(HistoryBackendTest, CloneFaviconIsRestrictedToSameDomain) {
  const GURL url("http://www.google.com/");
  const GURL same_domain_url("http://www.google.com/subdir/index.html");
  const GURL foreign_domain_url("http://www.not-google.com/");
  const GURL icon_url("http://www.google.com/icon.png");

  // Add a favicon
  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  GenerateFaviconBitmapData(icon_url, GetEdgeSizesSmall(),
      &favicon_bitmap_data);
  backend_->SetFavicons(url, favicon_base::FAVICON, favicon_bitmap_data);
  EXPECT_TRUE(backend_->thumbnail_db_->GetIconMappingsForPageURL(
      url, favicon_base::FAVICON, NULL));

  // Validate starting state.
  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results_out;
  EXPECT_TRUE(backend_->GetFaviconsFromDB(url,
                                          favicon_base::FAVICON,
                                          GetEdgeSizesSmallAndLarge(),
                                          &bitmap_results_out));
  EXPECT_FALSE(backend_->GetFaviconsFromDB(same_domain_url,
                                           favicon_base::FAVICON,
                                           GetEdgeSizesSmallAndLarge(),
                                           &bitmap_results_out));
  EXPECT_FALSE(backend_->GetFaviconsFromDB(foreign_domain_url,
                                           favicon_base::FAVICON,
                                           GetEdgeSizesSmallAndLarge(),
                                           &bitmap_results_out));

  // Same-domain cloning should work.
  backend_->CloneFavicons(url, same_domain_url);
  EXPECT_TRUE(backend_->GetFaviconsFromDB(same_domain_url,
                                          favicon_base::FAVICON,
                                          GetEdgeSizesSmallAndLarge(),
                                          &bitmap_results_out));

  // Foreign-domain cloning is forbidden.
  backend_->CloneFavicons(url, foreign_domain_url);
  EXPECT_FALSE(backend_->GetFaviconsFromDB(foreign_domain_url,
                                           favicon_base::FAVICON,
                                           GetEdgeSizesSmallAndLarge(),
                                           &bitmap_results_out));
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
  const char* apple = "http://www.apple.com/";

  // Clear all history.
  backend_->DeleteAllHistory();

  Time tested_time = Time::Now().LocalMidnight() +
                     base::TimeDelta::FromHours(4);
  base::TimeDelta half_an_hour = base::TimeDelta::FromMinutes(30);
  base::TimeDelta one_hour = base::TimeDelta::FromHours(1);
  base::TimeDelta one_day = base::TimeDelta::FromDays(1);

  const content::PageTransition kTypedTransition =
      content::PAGE_TRANSITION_TYPED;
  const content::PageTransition kKeywordGeneratedTransition =
      content::PAGE_TRANSITION_KEYWORD_GENERATED;

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
  AddRedirectChainWithTransitionAndTime(
      redirect_sequence, 0,
      kTypedTransition, tested_time);

  // Add a visit with a transition that will make sure that no segment gets
  // created for this page (so the subsequent entries will have different URLIDs
  // and SegmentIDs).
  redirect_sequence[0] = apple;
  AddRedirectChainWithTransitionAndTime(
      redirect_sequence, 0, kKeywordGeneratedTransition,
      tested_time - one_day + one_hour * 6);

  redirect_sequence[0] = yahoo;
  AddRedirectChainWithTransitionAndTime(
      redirect_sequence, 0, kTypedTransition,
      tested_time - one_day + half_an_hour);
  AddRedirectChainWithTransitionAndTime(
      redirect_sequence, 0, kTypedTransition,
      tested_time - one_day + half_an_hour * 2);

  redirect_sequence[0] = yahoo_sports;
  AddRedirectChainWithTransitionAndTime(
      redirect_sequence, 0, kTypedTransition,
      tested_time - one_day - half_an_hour * 2);
  AddRedirectChainWithTransitionAndTime(
      redirect_sequence, 0, kTypedTransition,
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

  VisitFilter filter;
  FilteredURLList filtered_list;
  // Time limit is |tested_time| +/- 45 min.
  base::TimeDelta three_quarters_of_an_hour = base::TimeDelta::FromMinutes(45);
  filter.SetFilterTime(tested_time);
  filter.SetFilterWidth(three_quarters_of_an_hour);
  backend_->QueryFilteredURLs(100, filter, false, &filtered_list);

  ASSERT_EQ(4U, filtered_list.size());
  EXPECT_EQ(std::string(google), filtered_list[0].url.spec());
  EXPECT_EQ(std::string(yahoo_sports_soccer), filtered_list[1].url.spec());
  EXPECT_EQ(std::string(yahoo), filtered_list[2].url.spec());
  EXPECT_EQ(std::string(yahoo_sports), filtered_list[3].url.spec());

  // Time limit is between |tested_time| and |tested_time| + 2 hours.
  filter.SetFilterTime(tested_time + one_hour);
  filter.SetFilterWidth(one_hour);
  backend_->QueryFilteredURLs(100, filter, false, &filtered_list);

  ASSERT_EQ(3U, filtered_list.size());
  EXPECT_EQ(std::string(google), filtered_list[0].url.spec());
  EXPECT_EQ(std::string(yahoo), filtered_list[1].url.spec());
  EXPECT_EQ(std::string(yahoo_sports), filtered_list[2].url.spec());

  // Time limit is between |tested_time| - 2 hours and |tested_time|.
  filter.SetFilterTime(tested_time - one_hour);
  filter.SetFilterWidth(one_hour);
  backend_->QueryFilteredURLs(100, filter, false, &filtered_list);

  ASSERT_EQ(3U, filtered_list.size());
  EXPECT_EQ(std::string(google), filtered_list[0].url.spec());
  EXPECT_EQ(std::string(yahoo_sports_soccer), filtered_list[1].url.spec());
  EXPECT_EQ(std::string(yahoo_sports), filtered_list[2].url.spec());

  filter.ClearFilters();
  base::Time::Exploded exploded_time;
  tested_time.LocalExplode(&exploded_time);

  // Today.
  filter.SetFilterTime(tested_time);
  filter.SetDayOfTheWeekFilter(static_cast<int>(exploded_time.day_of_week));
  backend_->QueryFilteredURLs(100, filter, false, &filtered_list);

  ASSERT_EQ(2U, filtered_list.size());
  EXPECT_EQ(std::string(google), filtered_list[0].url.spec());
  EXPECT_EQ(std::string(yahoo_sports_soccer), filtered_list[1].url.spec());

  // Today + time limit - only yahoo_sports_soccer should fit.
  filter.SetFilterTime(tested_time - base::TimeDelta::FromMinutes(40));
  filter.SetFilterWidth(base::TimeDelta::FromMinutes(20));
  backend_->QueryFilteredURLs(100, filter, false, &filtered_list);

  ASSERT_EQ(1U, filtered_list.size());
  EXPECT_EQ(std::string(yahoo_sports_soccer), filtered_list[0].url.spec());

  // Make sure we get debug data if we request it.
  filter.SetFilterTime(tested_time);
  filter.SetFilterWidth(one_hour * 2);
  backend_->QueryFilteredURLs(100, filter, true, &filtered_list);

  // If the SegmentID is used by QueryFilteredURLs when generating the debug
  // data instead of the URLID, the |total_visits| for the |yahoo_sports_soccer|
  // entry will be zero instead of 1.
  ASSERT_GE(filtered_list.size(), 2U);
  EXPECT_EQ(std::string(google), filtered_list[0].url.spec());
  EXPECT_EQ(std::string(yahoo_sports_soccer), filtered_list[1].url.spec());
  EXPECT_EQ(4U, filtered_list[0].extended_info.total_visits);
  EXPECT_EQ(1U, filtered_list[1].extended_info.total_visits);
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

  base::FilePath old_history_path, old_history;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &old_history_path));
  old_history_path = old_history_path.AppendASCII("History");
  old_history = old_history_path.AppendASCII("HistoryNoDuration");

  // Copy history database file to current directory so that it will be deleted
  // in Teardown.
  base::FilePath new_history_path(test_dir());
  base::DeleteFile(new_history_path, true);
  base::CreateDirectory(new_history_path);
  base::FilePath new_history_file =
      new_history_path.Append(chrome::kHistoryFilename);
  ASSERT_TRUE(base::CopyFile(old_history, new_history_file));

  backend_ = new HistoryBackend(
      new_history_path, new HistoryBackendTestDelegate(this), &history_client_);
  backend_->Init(std::string(), false);
  backend_->Closing();
  backend_ = NULL;

  // Now the history database should already be migrated.

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
}

TEST_F(HistoryBackendTest, AddPageNoVisitForBookmark) {
  ASSERT_TRUE(backend_.get());

  GURL url("http://www.google.com");
  base::string16 title(base::UTF8ToUTF16("Bookmark title"));
  backend_->AddPageNoVisitForBookmark(url, title);

  URLRow row;
  backend_->GetURL(url, &row);
  EXPECT_EQ(url, row.url());
  EXPECT_EQ(title, row.title());
  EXPECT_EQ(0, row.visit_count());

  backend_->DeleteURL(url);
  backend_->AddPageNoVisitForBookmark(url, base::string16());
  backend_->GetURL(url, &row);
  EXPECT_EQ(url, row.url());
  EXPECT_EQ(base::UTF8ToUTF16(url.spec()), row.title());
  EXPECT_EQ(0, row.visit_count());
}

TEST_F(HistoryBackendTest, ExpireHistoryForTimes) {
  ASSERT_TRUE(backend_.get());

  HistoryAddPageArgs args[10];
  for (size_t i = 0; i < arraysize(args); ++i) {
    args[i].url = GURL("http://example" +
                       std::string((i % 2 == 0 ? ".com" : ".net")));
    args[i].time = base::Time::FromInternalValue(i);
    backend_->AddPage(args[i]);
  }
  EXPECT_EQ(base::Time(), backend_->GetFirstRecordedTimeForTest());

  URLRow row;
  for (size_t i = 0; i < arraysize(args); ++i) {
    EXPECT_TRUE(backend_->GetURL(args[i].url, &row));
  }

  std::set<base::Time> times;
  times.insert(args[5].time);
  backend_->ExpireHistoryForTimes(times,
                                  base::Time::FromInternalValue(2),
                                  base::Time::FromInternalValue(8));

  EXPECT_EQ(base::Time::FromInternalValue(0),
            backend_->GetFirstRecordedTimeForTest());

  // Visits to http://example.com are untouched.
  VisitVector visit_vector;
  EXPECT_TRUE(backend_->GetVisitsForURL(
      backend_->db_->GetRowForURL(GURL("http://example.com"), NULL),
      &visit_vector));
  ASSERT_EQ(5u, visit_vector.size());
  EXPECT_EQ(base::Time::FromInternalValue(0), visit_vector[0].visit_time);
  EXPECT_EQ(base::Time::FromInternalValue(2), visit_vector[1].visit_time);
  EXPECT_EQ(base::Time::FromInternalValue(4), visit_vector[2].visit_time);
  EXPECT_EQ(base::Time::FromInternalValue(6), visit_vector[3].visit_time);
  EXPECT_EQ(base::Time::FromInternalValue(8), visit_vector[4].visit_time);

  // Visits to http://example.net between [2,8] are removed.
  visit_vector.clear();
  EXPECT_TRUE(backend_->GetVisitsForURL(
      backend_->db_->GetRowForURL(GURL("http://example.net"), NULL),
      &visit_vector));
  ASSERT_EQ(2u, visit_vector.size());
  EXPECT_EQ(base::Time::FromInternalValue(1), visit_vector[0].visit_time);
  EXPECT_EQ(base::Time::FromInternalValue(9), visit_vector[1].visit_time);

  EXPECT_EQ(base::Time::FromInternalValue(0),
            backend_->GetFirstRecordedTimeForTest());
}

TEST_F(HistoryBackendTest, ExpireHistory) {
  ASSERT_TRUE(backend_.get());
  // Since history operations are dependent on the local timezone, make all
  // entries relative to a fixed, local reference time.
  base::Time reference_time = base::Time::UnixEpoch().LocalMidnight() +
                              base::TimeDelta::FromHours(12);

  // Insert 4 entries into the database.
  HistoryAddPageArgs args[4];
  for (size_t i = 0; i < arraysize(args); ++i) {
    args[i].url = GURL("http://example" + base::IntToString(i) + ".com");
    args[i].time = reference_time + base::TimeDelta::FromDays(i);
    backend_->AddPage(args[i]);
  }

  URLRow url_rows[4];
  for (unsigned int i = 0; i < arraysize(args); ++i)
    ASSERT_TRUE(backend_->GetURL(args[i].url, &url_rows[i]));

  std::vector<ExpireHistoryArgs> expire_list;
  VisitVector visits;

  // Passing an empty map should be a no-op.
  backend_->ExpireHistory(expire_list);
  backend_->db()->GetAllVisitsInRange(base::Time(), base::Time(), 0, &visits);
  EXPECT_EQ(4U, visits.size());

  // Trying to delete an unknown URL with the time of the first visit should
  // also be a no-op.
  expire_list.resize(expire_list.size() + 1);
  expire_list[0].SetTimeRangeForOneDay(args[0].time);
  expire_list[0].urls.insert(GURL("http://google.does-not-exist"));
  backend_->ExpireHistory(expire_list);
  backend_->db()->GetAllVisitsInRange(base::Time(), base::Time(), 0, &visits);
  EXPECT_EQ(4U, visits.size());

  // Now add the first URL with the same time -- it should get deleted.
  expire_list.back().urls.insert(url_rows[0].url());
  backend_->ExpireHistory(expire_list);

  backend_->db()->GetAllVisitsInRange(base::Time(), base::Time(), 0, &visits);
  ASSERT_EQ(3U, visits.size());
  EXPECT_EQ(visits[0].url_id, url_rows[1].id());
  EXPECT_EQ(visits[1].url_id, url_rows[2].id());
  EXPECT_EQ(visits[2].url_id, url_rows[3].id());

  // The first recorded time should also get updated.
  EXPECT_EQ(backend_->GetFirstRecordedTimeForTest(), args[1].time);

  // Now delete the rest of the visits in one call.
  for (unsigned int i = 1; i < arraysize(args); ++i) {
    expire_list.resize(expire_list.size() + 1);
    expire_list[i].SetTimeRangeForOneDay(args[i].time);
    expire_list[i].urls.insert(args[i].url);
  }
  backend_->ExpireHistory(expire_list);

  backend_->db()->GetAllVisitsInRange(base::Time(), base::Time(), 0, &visits);
  ASSERT_EQ(0U, visits.size());
}

TEST_F(HistoryBackendTest, DeleteMatchingUrlsForKeyword) {
  // Set up urls and keyword_search_terms
  GURL url1("https://www.bing.com/?q=bar");
  URLRow url_info1(url1);
  url_info1.set_visit_count(0);
  url_info1.set_typed_count(0);
  url_info1.set_last_visit(Time());
  url_info1.set_hidden(false);
  const URLID url1_id = backend_->db()->AddURL(url_info1);
  EXPECT_NE(0, url1_id);

  KeywordID keyword_id = 1;
  base::string16 keyword = base::UTF8ToUTF16("bar");
  ASSERT_TRUE(backend_->db()->SetKeywordSearchTermsForURL(
      url1_id, keyword_id, keyword));

  GURL url2("https://www.google.com/?q=bar");
  URLRow url_info2(url2);
  url_info2.set_visit_count(0);
  url_info2.set_typed_count(0);
  url_info2.set_last_visit(Time());
  url_info2.set_hidden(false);
  const URLID url2_id = backend_->db()->AddURL(url_info2);
  EXPECT_NE(0, url2_id);

  KeywordID keyword_id2 = 2;
  ASSERT_TRUE(backend_->db()->SetKeywordSearchTermsForURL(
      url2_id, keyword_id2, keyword));

  // Add another visit to the same URL
  URLRow url_info3(url2);
  url_info3.set_visit_count(0);
  url_info3.set_typed_count(0);
  url_info3.set_last_visit(Time());
  url_info3.set_hidden(false);
  const URLID url3_id = backend_->db()->AddURL(url_info3);
  EXPECT_NE(0, url3_id);
  ASSERT_TRUE(backend_->db()->SetKeywordSearchTermsForURL(
      url3_id, keyword_id2, keyword));

  // Test that deletion works correctly
  backend_->DeleteMatchingURLsForKeyword(keyword_id2, keyword);

  // Test that rows 2 and 3 are deleted, while 1 is intact
  URLRow row;
  EXPECT_TRUE(backend_->db()->GetURLRow(url1_id, &row));
  EXPECT_EQ(url1.spec(), row.url().spec());
  EXPECT_FALSE(backend_->db()->GetURLRow(url2_id, &row));
  EXPECT_FALSE(backend_->db()->GetURLRow(url3_id, &row));

  // Test that corresponding keyword search terms are deleted for rows 2 & 3,
  // but not for row 1
  EXPECT_TRUE(backend_->db()->GetKeywordSearchTermRow(url1_id, NULL));
  EXPECT_FALSE(backend_->db()->GetKeywordSearchTermRow(url2_id, NULL));
  EXPECT_FALSE(backend_->db()->GetKeywordSearchTermRow(url3_id, NULL));
}

// Simple test that removes a bookmark. This test exercises the code paths in
// History that block till bookmark bar model is loaded.
TEST_F(HistoryBackendTest, RemoveNotification) {
  scoped_ptr<TestingProfile> profile(new TestingProfile());

  // Add a URL.
  GURL url("http://www.google.com");
  HistoryClientMock history_client;
  history_client.AddBookmark(url);
  scoped_ptr<HistoryService> service(
      new HistoryService(&history_client, profile.get()));
  EXPECT_TRUE(service->Init(profile->GetPath()));

  service->AddPage(
      url, base::Time::Now(), NULL, 1, GURL(), RedirectList(),
      content::PAGE_TRANSITION_TYPED, SOURCE_BROWSED, false);

  // This won't actually delete the URL, rather it'll empty out the visits.
  // This triggers blocking on the BookmarkModel.
  EXPECT_CALL(history_client, BlockUntilBookmarksLoaded());
  service->DeleteURL(url);
}

// Test DeleteFTSIndexDatabases deletes expected files.
TEST_F(HistoryBackendTest, DeleteFTSIndexDatabases) {
  ASSERT_TRUE(backend_.get());

  base::FilePath history_path(test_dir());
  base::FilePath db1(history_path.AppendASCII("History Index 2013-05"));
  base::FilePath db1_journal(db1.InsertBeforeExtensionASCII("-journal"));
  base::FilePath db1_wal(db1.InsertBeforeExtensionASCII("-wal"));
  base::FilePath db2_symlink(history_path.AppendASCII("History Index 2013-06"));
  base::FilePath db2_actual(history_path.AppendASCII("Underlying DB"));

  // Setup dummy index database files.
  const char* data = "Dummy";
  const size_t data_len = 5;
  ASSERT_TRUE(base::WriteFile(db1, data, data_len));
  ASSERT_TRUE(base::WriteFile(db1_journal, data, data_len));
  ASSERT_TRUE(base::WriteFile(db1_wal, data, data_len));
  ASSERT_TRUE(base::WriteFile(db2_actual, data, data_len));
#if defined(OS_POSIX)
  EXPECT_TRUE(base::CreateSymbolicLink(db2_actual, db2_symlink));
#endif

  // Delete all DTS index databases.
  backend_->DeleteFTSIndexDatabases();
  EXPECT_FALSE(base::PathExists(db1));
  EXPECT_FALSE(base::PathExists(db1_wal));
  EXPECT_FALSE(base::PathExists(db1_journal));
  EXPECT_FALSE(base::PathExists(db2_symlink));
  EXPECT_TRUE(base::PathExists(db2_actual));  // Symlinks shouldn't be followed.
}

// Common implementation for the two tests below, given that the only difference
// between them is the type of the notification sent out.
void InMemoryHistoryBackendTest::TestAddingAndChangingURLRows(
    int notification_type) {
  const char kTestTypedURLAlternativeTitle[] = "Google Search Again";
  const char kTestNonTypedURLAlternativeTitle[] = "Google News Again";

  // Notify the in-memory database that a typed and non-typed URLRow (which were
  // never before seen by the cache) have been modified.
  URLRow row1(CreateTestTypedURL());
  URLRow row2(CreateTestNonTypedURL());
  SimulateNotification(notification_type, &row1, &row2);

  // The in-memory database should only pick up the typed URL, and should ignore
  // the non-typed one. The typed URL should retain the ID that was present in
  // the notification.
  URLRow cached_row1, cached_row2;
  EXPECT_NE(0, mem_backend_->db()->GetRowForURL(row1.url(), &cached_row1));
  EXPECT_EQ(0, mem_backend_->db()->GetRowForURL(row2.url(), &cached_row2));
  EXPECT_EQ(row1.id(), cached_row1.id());

  // Try changing attributes (other than typed_count) for existing URLRows.
  row1.set_title(base::UTF8ToUTF16(kTestTypedURLAlternativeTitle));
  row2.set_title(base::UTF8ToUTF16(kTestNonTypedURLAlternativeTitle));
  SimulateNotification(notification_type, &row1, &row2);

  // URLRows that are cached by the in-memory database should be updated.
  EXPECT_NE(0, mem_backend_->db()->GetRowForURL(row1.url(), &cached_row1));
  EXPECT_EQ(0, mem_backend_->db()->GetRowForURL(row2.url(), &cached_row2));
  EXPECT_EQ(base::UTF8ToUTF16(kTestTypedURLAlternativeTitle),
            cached_row1.title());

  // Now decrease the typed count for the typed URLRow, and increase it for the
  // previously non-typed URLRow.
  row1.set_typed_count(0);
  row2.set_typed_count(2);
  SimulateNotification(notification_type, &row1, &row2);

  // The in-memory database should stop caching the first URLRow, and start
  // caching the second URLRow.
  EXPECT_EQ(0, mem_backend_->db()->GetRowForURL(row1.url(), &cached_row1));
  EXPECT_NE(0, mem_backend_->db()->GetRowForURL(row2.url(), &cached_row2));
  EXPECT_EQ(row2.id(), cached_row2.id());
  EXPECT_EQ(base::UTF8ToUTF16(kTestNonTypedURLAlternativeTitle),
            cached_row2.title());
}

TEST_F(InMemoryHistoryBackendTest, OnURLsModified) {
  TestAddingAndChangingURLRows(chrome::NOTIFICATION_HISTORY_URLS_MODIFIED);
}

TEST_F(InMemoryHistoryBackendTest, OnURLsVisisted) {
  TestAddingAndChangingURLRows(chrome::NOTIFICATION_HISTORY_URL_VISITED);
}

TEST_F(InMemoryHistoryBackendTest, OnURLsDeletedPiecewise) {
  // Add two typed and one non-typed URLRow to the in-memory database.
  URLRow row1(CreateTestTypedURL());
  URLRow row2(CreateAnotherTestTypedURL());
  URLRow row3(CreateTestNonTypedURL());
  SimulateNotification(chrome::NOTIFICATION_HISTORY_URLS_MODIFIED,
                       &row1, &row2, &row3);

  // Notify the in-memory database that the second typed URL and the non-typed
  // URL has been deleted.
  SimulateNotification(chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                       &row2, &row3);

  // Expect that the first typed URL remains intact, the second typed URL is
  // correctly removed, and the non-typed URL does not magically appear.
  URLRow cached_row1;
  EXPECT_NE(0, mem_backend_->db()->GetRowForURL(row1.url(), &cached_row1));
  EXPECT_EQ(0, mem_backend_->db()->GetRowForURL(row2.url(), NULL));
  EXPECT_EQ(0, mem_backend_->db()->GetRowForURL(row3.url(), NULL));
  EXPECT_EQ(row1.id(), cached_row1.id());
}

TEST_F(InMemoryHistoryBackendTest, OnURLsDeletedEnMasse) {
  // Add two typed and one non-typed URLRow to the in-memory database.
  URLRow row1(CreateTestTypedURL());
  URLRow row2(CreateAnotherTestTypedURL());
  URLRow row3(CreateTestNonTypedURL());
  SimulateNotification(chrome::NOTIFICATION_HISTORY_URLS_MODIFIED,
                       &row1, &row2, &row3);

  // Now notify the in-memory database that all history has been deleted.
  scoped_ptr<URLsDeletedDetails> details(new URLsDeletedDetails());
  details->all_history = true;
  BroadcastNotifications(chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                         details.PassAs<HistoryDetails>());

  // Expect that everything goes away.
  EXPECT_EQ(0, mem_backend_->db()->GetRowForURL(row1.url(), NULL));
  EXPECT_EQ(0, mem_backend_->db()->GetRowForURL(row2.url(), NULL));
  EXPECT_EQ(0, mem_backend_->db()->GetRowForURL(row3.url(), NULL));
}

void InMemoryHistoryBackendTest::PopulateTestURLsAndSearchTerms(
    URLRow* row1,
    URLRow* row2,
    const base::string16& term1,
    const base::string16& term2) {
  // Add a typed and a non-typed URLRow to the in-memory database. This time,
  // though, do it through the history backend...
  URLRows rows;
  rows.push_back(*row1);
  rows.push_back(*row2);
  backend_->AddPagesWithDetails(rows, history::SOURCE_BROWSED);
  backend_->db()->GetRowForURL(row1->url(), row1);  // Get effective IDs from
  backend_->db()->GetRowForURL(row2->url(), row2);  // the database.

  // ... so that we can also use that for adding the search terms. This way, we
  // not only test that the notifications involved are handled correctly, but
  // also that they are fired correctly (in the history backend).
  backend_->SetKeywordSearchTermsForURL(row1->url(), kTestKeywordId, term1);
  backend_->SetKeywordSearchTermsForURL(row2->url(), kTestKeywordId, term2);
}

TEST_F(InMemoryHistoryBackendTest, SetKeywordSearchTerms) {
  URLRow row1(CreateTestTypedURL());
  URLRow row2(CreateTestNonTypedURL());
  base::string16 term1(base::UTF8ToUTF16(kTestSearchTerm1));
  base::string16 term2(base::UTF8ToUTF16(kTestSearchTerm2));
  PopulateTestURLsAndSearchTerms(&row1, &row2, term1, term2);

  // Both URLs now have associated search terms, so the in-memory database
  // should cache both of them, regardless whether they have been typed or not.
  URLRow cached_row1, cached_row2;
  EXPECT_NE(0, mem_backend_->db()->GetRowForURL(row1.url(), &cached_row1));
  EXPECT_NE(0, mem_backend_->db()->GetRowForURL(row2.url(), &cached_row2));
  EXPECT_EQ(row1.id(), cached_row1.id());
  EXPECT_EQ(row2.id(), cached_row2.id());

  // Verify that lookups will actually return both search terms; and also check
  // at the low level that the rows are there.
  EXPECT_EQ(1u, GetNumberOfMatchingSearchTerms(kTestKeywordId, term1));
  EXPECT_EQ(1u, GetNumberOfMatchingSearchTerms(kTestKeywordId, term2));
  EXPECT_TRUE(mem_backend_->db()->GetKeywordSearchTermRow(row1.id(), NULL));
  EXPECT_TRUE(mem_backend_->db()->GetKeywordSearchTermRow(row2.id(), NULL));
}

TEST_F(InMemoryHistoryBackendTest, DeleteKeywordSearchTerms) {
  URLRow row1(CreateTestTypedURL());
  URLRow row2(CreateTestNonTypedURL());
  base::string16 term1(base::UTF8ToUTF16(kTestSearchTerm1));
  base::string16 term2(base::UTF8ToUTF16(kTestSearchTerm2));
  PopulateTestURLsAndSearchTerms(&row1, &row2, term1, term2);

  // Delete both search terms. This should be reflected in the in-memory DB.
  backend_->DeleteKeywordSearchTermForURL(row1.url());
  backend_->DeleteKeywordSearchTermForURL(row2.url());

  // The typed URL should remain intact.
  // Note: we do not need to guarantee anything about the non-typed URL.
  URLRow cached_row1;
  EXPECT_NE(0, mem_backend_->db()->GetRowForURL(row1.url(), &cached_row1));
  EXPECT_EQ(row1.id(), cached_row1.id());

  // Verify that the search terms are no longer returned as results, and also
  // check at the low level that they are gone for good.
  EXPECT_EQ(0u, GetNumberOfMatchingSearchTerms(kTestKeywordId, term1));
  EXPECT_EQ(0u, GetNumberOfMatchingSearchTerms(kTestKeywordId, term2));
  EXPECT_FALSE(mem_backend_->db()->GetKeywordSearchTermRow(row1.id(), NULL));
  EXPECT_FALSE(mem_backend_->db()->GetKeywordSearchTermRow(row2.id(), NULL));
}

TEST_F(InMemoryHistoryBackendTest, DeleteAllSearchTermsForKeyword) {
  URLRow row1(CreateTestTypedURL());
  URLRow row2(CreateTestNonTypedURL());
  base::string16 term1(base::UTF8ToUTF16(kTestSearchTerm1));
  base::string16 term2(base::UTF8ToUTF16(kTestSearchTerm2));
  PopulateTestURLsAndSearchTerms(&row1, &row2, term1, term2);

  // Delete all corresponding search terms from the in-memory database.
  KeywordID id = kTestKeywordId;
  mem_backend_->DeleteAllSearchTermsForKeyword(id);

  // The typed URL should remain intact.
  // Note: we do not need to guarantee anything about the non-typed URL.
  URLRow cached_row1;
  EXPECT_NE(0, mem_backend_->db()->GetRowForURL(row1.url(), &cached_row1));
  EXPECT_EQ(row1.id(), cached_row1.id());

  // Verify that the search terms are no longer returned as results, and also
  // check at the low level that they are gone for good.
  EXPECT_EQ(0u, GetNumberOfMatchingSearchTerms(kTestKeywordId, term1));
  EXPECT_EQ(0u, GetNumberOfMatchingSearchTerms(kTestKeywordId, term2));
  EXPECT_FALSE(mem_backend_->db()->GetKeywordSearchTermRow(row1.id(), NULL));
  EXPECT_FALSE(mem_backend_->db()->GetKeywordSearchTermRow(row2.id(), NULL));
}

TEST_F(InMemoryHistoryBackendTest, OnURLsDeletedWithSearchTerms) {
  URLRow row1(CreateTestTypedURL());
  URLRow row2(CreateTestNonTypedURL());
  base::string16 term1(base::UTF8ToUTF16(kTestSearchTerm1));
  base::string16 term2(base::UTF8ToUTF16(kTestSearchTerm2));
  PopulateTestURLsAndSearchTerms(&row1, &row2, term1, term2);

  // Notify the in-memory database that the second typed URL has been deleted.
  SimulateNotification(chrome::NOTIFICATION_HISTORY_URLS_DELETED, &row2);

  // Verify that the second term is no longer returned as result, and also check
  // at the low level that it is gone for good. The term corresponding to the
  // first URLRow should not be affected.
  EXPECT_EQ(1u, GetNumberOfMatchingSearchTerms(kTestKeywordId, term1));
  EXPECT_EQ(0u, GetNumberOfMatchingSearchTerms(kTestKeywordId, term2));
  EXPECT_TRUE(mem_backend_->db()->GetKeywordSearchTermRow(row1.id(), NULL));
  EXPECT_FALSE(mem_backend_->db()->GetKeywordSearchTermRow(row2.id(), NULL));
}

}  // namespace history
