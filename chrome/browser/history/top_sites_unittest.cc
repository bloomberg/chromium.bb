// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_marshaling.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/history/top_sites_backend.h"
#include "chrome/browser/history/top_sites_cache.h"
#include "chrome/browser/history/top_sites_database.h"
#include "chrome/browser/ui/webui/most_visited_handler.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/testing_profile.h"
#include "chrome/tools/profiles/thumbnail-inl.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace history {

namespace {

// Used by WaitForHistory, see it for details.
class WaitForHistoryTask : public HistoryDBTask {
 public:
  WaitForHistoryTask() {}

  virtual bool RunOnDBThread(HistoryBackend* backend, HistoryDatabase* db) {
    return true;
  }

  virtual void DoneRunOnMainThread() {
    MessageLoop::current()->Quit();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WaitForHistoryTask);
};

// Used for querying top sites. Either runs sequentially, or runs a nested
// nested message loop until the response is complete. The later is used when
// TopSites is queried before it finishes loading.
class TopSitesQuerier {
 public:
  TopSitesQuerier() : number_of_callbacks_(0), waiting_(false) {}

  // Queries top sites. If |wait| is true a nested message loop is run until the
  // callback is notified.
  void QueryTopSites(TopSites* top_sites, bool wait) {
    int start_number_of_callbacks = number_of_callbacks_;
    top_sites->GetMostVisitedURLs(
        &consumer_,
        NewCallback(this, &TopSitesQuerier::OnTopSitesAvailable));
    if (wait && start_number_of_callbacks == number_of_callbacks_) {
      waiting_ = true;
      MessageLoop::current()->Run();
    }
  }

  void CancelRequest() {
    consumer_.CancelAllRequests();
  }

  void set_urls(const MostVisitedURLList& urls) { urls_ = urls; }
  const MostVisitedURLList& urls() const { return urls_; }

  int number_of_callbacks() const { return number_of_callbacks_; }

 private:
  // Callback for TopSites::GetMostVisitedURLs.
  void OnTopSitesAvailable(const history::MostVisitedURLList& data) {
    urls_ = data;
    number_of_callbacks_++;
    if (waiting_) {
      MessageLoop::current()->Quit();
      waiting_ = false;
    }
  }

  CancelableRequestConsumer consumer_;
  MostVisitedURLList urls_;
  int number_of_callbacks_;
  bool waiting_;

  DISALLOW_COPY_AND_ASSIGN(TopSitesQuerier);
};

// Extracts the data from |t1| into a SkBitmap. This is intended for usage of
// thumbnail data, which is stored as jpgs.
SkBitmap ExtractThumbnail(const RefCountedBytes& t1) {
  scoped_ptr<SkBitmap> image(gfx::JPEGCodec::Decode(t1.front(),
                                                    t1.data.size()));
  return image.get() ? *image : SkBitmap();
}

// Returns true if t1 and t2 contain the same data.
bool ThumbnailsAreEqual(RefCountedBytes* t1, RefCountedBytes* t2) {
  if (!t1 || !t2)
    return false;
  if (t1->data.size() != t2->data.size())
    return false;
  return std::equal(t1->data.begin(),
                    t1->data.end(),
                    t2->data.begin());
}

}  // namespace

class TopSitesTest : public testing::Test {
 public:
  TopSitesTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB, &message_loop_),
        original_command_line_(*CommandLine::ForCurrentProcess()) {
  }

  virtual void SetUp() {
    profile_.reset(new TestingProfile);
    if (CreateHistoryAndTopSites()) {
      profile_->CreateHistoryService(false, false);
      profile_->CreateTopSites();
      profile_->BlockUntilTopSitesLoaded();
    }
  }

  virtual void TearDown() {
    profile_.reset();
    *CommandLine::ForCurrentProcess() = original_command_line_;
  }

  // Returns true if history and top sites should be created in SetUp.
  virtual bool CreateHistoryAndTopSites() {
    return true;
  }

  // Gets the thumbnail for |url| from TopSites.
  SkBitmap GetThumbnail(const GURL& url) {
    scoped_refptr<RefCountedBytes> data;
    return top_sites()->GetPageThumbnail(url, &data) ?
        ExtractThumbnail(*data.get()) : SkBitmap();
  }

  // Creates a bitmap of the specified color.
  SkBitmap CreateBitmap(SkColor color) {
    SkBitmap thumbnail;
    thumbnail.setConfig(SkBitmap::kARGB_8888_Config, 4, 4);
    thumbnail.allocPixels();
    thumbnail.eraseColor(color);
    return thumbnail;
  }

  // Forces top sites to load top sites from history, then recreates top sites.
  // Recreating top sites makes sure the changes from history are saved and
  // loaded from the db.
  void RefreshTopSitesAndRecreate() {
    StartQueryForMostVisited();
    WaitForHistory();
    RecreateTopSitesAndBlock();
  }

  // Blocks the caller until history processes a task. This is useful if you
  // need to wait until you know history has processed a task.
  void WaitForHistory() {
    history_service()->ScheduleDBTask(new WaitForHistoryTask(), &consumer_);
    MessageLoop::current()->Run();
  }

  // Waits for top sites to finish processing a task. This is useful if you need
  // to wait until top sites finishes processing a task.
  void WaitForTopSites() {
    top_sites()->backend_->DoEmptyRequest(
        &consumer_,
        NewCallback(this, &TopSitesTest::QuitCallback));
    MessageLoop::current()->Run();
  }

  TopSites* top_sites() { return profile_->GetTopSites(); }
  CancelableRequestConsumer* consumer() { return &consumer_; }
  TestingProfile* profile() {return profile_.get();}
  HistoryService* history_service() {
    return profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  }

  MostVisitedURLList GetPrepopulatePages() {
    return TopSites::GetPrepopulatePages();
  }

  // Returns true if the TopSitesQuerier contains the prepopulate data starting
  // at |start_index|.
  void ContainsPrepopulatePages(const TopSitesQuerier& querier,
                                size_t start_index) {
    MostVisitedURLList prepopulate_urls = GetPrepopulatePages();
    ASSERT_LE(start_index + prepopulate_urls.size(), querier.urls().size());
    for (size_t i = 0; i < prepopulate_urls.size(); ++i) {
      EXPECT_EQ(prepopulate_urls[i].url.spec(),
                querier.urls()[start_index + i].url.spec()) << " @ index " <<
          i;
    }
  }

  // Used for callbacks from history.
  void EmptyCallback() {
  }

  // Quit the current message loop when invoked. Useful when running a nested
  // message loop.
  void QuitCallback(TopSitesBackend::Handle handle) {
    MessageLoop::current()->Quit();
  }

  // Adds a page to history.
  void AddPageToHistory(const GURL& url) {
    RedirectList redirects;
    redirects.push_back(url);
    history_service()->AddPage(
        url, static_cast<void*>(this), 0, GURL(), PageTransition::TYPED,
        redirects, history::SOURCE_BROWSED, false);
  }

  // Adds a page to history.
  void AddPageToHistory(const GURL& url, const string16& title) {
    RedirectList redirects;
    redirects.push_back(url);
    history_service()->AddPage(
        url, static_cast<void*>(this), 0, GURL(), PageTransition::TYPED,
        redirects, history::SOURCE_BROWSED, false);
    history_service()->SetPageTitle(url, title);
  }

  // Adds a page to history.
  void AddPageToHistory(const GURL& url,
                        const string16& title,
                        const history::RedirectList& redirects,
                        base::Time time) {
    history_service()->AddPage(
        url, time, static_cast<void*>(this), 0, GURL(), PageTransition::TYPED,
        redirects, history::SOURCE_BROWSED, false);
    history_service()->SetPageTitle(url, title);
  }

  // Delets a url.
  void DeleteURL(const GURL& url) {
    history_service()->DeleteURL(url);
  }

  // Returns true if the thumbnail equals the specified bytes.
  bool ThumbnailEqualsBytes(const SkBitmap& image, RefCountedBytes* bytes) {
    scoped_refptr<RefCountedBytes> encoded_image;
    EncodeBitmap(image, &encoded_image);
    return ThumbnailsAreEqual(encoded_image, bytes);
  }

  // Recreates top sites. This forces top sites to reread from the db.
  void RecreateTopSitesAndBlock() {
    // Recreate TopSites and wait for it to load.
    profile()->CreateTopSites();
    // As history already loaded we have to fake this call.
    profile()->BlockUntilTopSitesLoaded();
  }

  // Wrappers that allow private TopSites functions to be called from the
  // individual tests without making them all be friends.
  GURL GetCanonicalURL(const GURL& url) {
    return top_sites()->cache_->GetCanonicalURL(url);
  }

  void SetTopSites(const MostVisitedURLList& new_top_sites) {
    top_sites()->SetTopSites(new_top_sites);
  }

  void StartQueryForMostVisited() {
    top_sites()->StartQueryForMostVisited();
  }

  bool EncodeBitmap(const SkBitmap& image,
                    scoped_refptr<RefCountedBytes>* bytes) {
    return TopSites::EncodeBitmap(image, bytes);
  }

  void SetLastNumUrlsChanged(size_t value) {
    top_sites()->last_num_urls_changed_ = value;
  }

  size_t last_num_urls_changed() { return top_sites()->last_num_urls_changed_; }

  base::TimeDelta GetUpdateDelay() {
    return top_sites()->GetUpdateDelay();
  }

  bool IsTopSitesLoaded() { return top_sites()->loaded_; }

  bool AddPrepopulatedPages(MostVisitedURLList* urls) {
    return TopSites::AddPrepopulatedPages(urls);
  }

 private:
  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  BrowserThread db_thread_;
  scoped_ptr<TestingProfile> profile_;
  CancelableRequestConsumer consumer_;
  CommandLine original_command_line_;

  DISALLOW_COPY_AND_ASSIGN(TopSitesTest);
};  // Class TopSitesTest

class TopSitesMigrationTest : public TopSitesTest {
 public:
  TopSitesMigrationTest() {}

  virtual void SetUp() {
    TopSitesTest::SetUp();

    FilePath data_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
    data_path = data_path.AppendASCII("top_sites");

    // Set up history and thumbnails as they would be before migration.
    ASSERT_NO_FATAL_FAILURE(
        ExecuteSQL(data_path.AppendASCII("history.19.sql"),
                   profile()->GetPath().Append(chrome::kHistoryFilename)));
    ASSERT_NO_FATAL_FAILURE(
        ExecuteSQL(data_path.AppendASCII("thumbnails.3.sql"),
                   profile()->GetPath().Append(chrome::kThumbnailsFilename)));

    profile()->CreateHistoryService(false, false);
    profile()->CreateTopSites();
    profile()->BlockUntilTopSitesLoaded();
  }

  // Returns true if history and top sites should be created in SetUp.
  virtual bool CreateHistoryAndTopSites() {
    return false;
  }

 protected:
  // Assertions for the migration test. This is extracted into a standalone
  // method so that it can be invoked twice.
  void MigrationAssertions() {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);

    // We shouldn't have gotten a callback.
    EXPECT_EQ(1, querier.number_of_callbacks());

    // The data we loaded should contain google and yahoo.
    ASSERT_EQ(2u + GetPrepopulatePages().size(), querier.urls().size());
    EXPECT_EQ(GURL("http://google.com/"), querier.urls()[0].url);
    EXPECT_EQ(GURL("http://yahoo.com/"), querier.urls()[1].url);
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 2));

    SkBitmap goog_thumbnail = GetThumbnail(GURL("http://google.com/"));
    EXPECT_EQ(1, goog_thumbnail.width());

    SkBitmap yahoo_thumbnail = GetThumbnail(GURL("http://yahoo.com/"));
    EXPECT_EQ(2, yahoo_thumbnail.width());

    // Favicon assertions are handled in ThumbnailDatabase.
  }

 private:
  // Executes the sql from the file |sql_path| in the database at |db_path|.
  void ExecuteSQL(const FilePath& sql_path,
                  const FilePath& db_path) {
    std::string sql;
    ASSERT_TRUE(file_util::ReadFileToString(sql_path, &sql));

    // Replace the 'last_visit_time', 'visit_time', 'time_slot' values in this
    // SQL with the current time.
    int64 now = base::Time::Now().ToInternalValue();
    std::vector<std::string> sql_time;
    sql_time.push_back(StringPrintf("%" PRId64, now));  // last_visit_time
    sql_time.push_back(StringPrintf("%" PRId64, now));  // visit_time
    sql_time.push_back(StringPrintf("%" PRId64, now));  // time_slot
    sql = ReplaceStringPlaceholders(sql, sql_time, NULL);

    sql::Connection connection;
    ASSERT_TRUE(connection.Open(db_path));
    ASSERT_TRUE(connection.Execute(sql.c_str()));
  }

  DISALLOW_COPY_AND_ASSIGN(TopSitesMigrationTest);
};

// Helper function for appending a URL to a vector of "most visited" URLs,
// using the default values for everything but the URL.
static void AppendMostVisitedURL(std::vector<MostVisitedURL>* list,
                                 const GURL& url) {
  MostVisitedURL mv;
  mv.url = url;
  mv.redirects.push_back(url);
  list->push_back(mv);
}

// Same as AppendMostVisitedURL except that it adds a redirect from the first
// URL to the second.
static void AppendMostVisitedURLWithRedirect(
    std::vector<MostVisitedURL>* list,
    const GURL& redirect_source, const GURL& redirect_dest) {
  MostVisitedURL mv;
  mv.url = redirect_dest;
  mv.redirects.push_back(redirect_source);
  mv.redirects.push_back(redirect_dest);
  list->push_back(mv);
}

// Tests GetCanonicalURL.
TEST_F(TopSitesTest, GetCanonicalURL) {
  // Have two chains:
  //   google.com -> www.google.com
  //   news.google.com (no redirects)
  GURL news("http://news.google.com/");
  GURL source("http://google.com/");
  GURL dest("http://www.google.com/");

  std::vector<MostVisitedURL> most_visited;
  AppendMostVisitedURLWithRedirect(&most_visited, source, dest);
  AppendMostVisitedURL(&most_visited, news);
  SetTopSites(most_visited);

  // Random URLs not in the database are returned unchanged.
  GURL result = GetCanonicalURL(GURL("http://fark.com/"));
  EXPECT_EQ(GURL("http://fark.com/"), result);

  // Easy case, there are no redirects and the exact URL is stored.
  result = GetCanonicalURL(news);
  EXPECT_EQ(news, result);

  // The URL in question is the source URL in a redirect list.
  result = GetCanonicalURL(source);
  EXPECT_EQ(dest, result);

  // The URL in question is the destination of a redirect.
  result = GetCanonicalURL(dest);
  EXPECT_EQ(dest, result);
}

// Tests DiffMostVisited.
TEST_F(TopSitesTest, DiffMostVisited) {
  GURL stays_the_same("http://staysthesame/");
  GURL gets_added_1("http://getsadded1/");
  GURL gets_added_2("http://getsadded2/");
  GURL gets_deleted_1("http://getsdeleted2/");
  GURL gets_moved_1("http://getsmoved1/");

  std::vector<MostVisitedURL> old_list;
  AppendMostVisitedURL(&old_list, stays_the_same);  // 0  (unchanged)
  AppendMostVisitedURL(&old_list, gets_deleted_1);  // 1  (deleted)
  AppendMostVisitedURL(&old_list, gets_moved_1);    // 2  (moved to 3)

  std::vector<MostVisitedURL> new_list;
  AppendMostVisitedURL(&new_list, stays_the_same);  // 0  (unchanged)
  AppendMostVisitedURL(&new_list, gets_added_1);    // 1  (added)
  AppendMostVisitedURL(&new_list, gets_added_2);    // 2  (added)
  AppendMostVisitedURL(&new_list, gets_moved_1);    // 3  (moved from 2)

  history::TopSitesDelta delta;
  history::TopSites::DiffMostVisited(old_list, new_list, &delta);

  ASSERT_EQ(2u, delta.added.size());
  ASSERT_TRUE(gets_added_1 == delta.added[0].url.url);
  ASSERT_EQ(1, delta.added[0].rank);
  ASSERT_TRUE(gets_added_2 == delta.added[1].url.url);
  ASSERT_EQ(2, delta.added[1].rank);

  ASSERT_EQ(1u, delta.deleted.size());
  ASSERT_TRUE(gets_deleted_1 == delta.deleted[0].url);

  ASSERT_EQ(1u, delta.moved.size());
  ASSERT_TRUE(gets_moved_1 == delta.moved[0].url.url);
  ASSERT_EQ(3, delta.moved[0].rank);
}

// Tests SetPageThumbnail.
TEST_F(TopSitesTest, SetPageThumbnail) {
  GURL url1a("http://google.com/");
  GURL url1b("http://www.google.com/");
  GURL url2("http://images.google.com/");
  GURL invalid_url("chrome://favicon/http://google.com/");

  std::vector<MostVisitedURL> list;
  AppendMostVisitedURL(&list, url2);

  MostVisitedURL mv;
  mv.url = url1b;
  mv.redirects.push_back(url1a);
  mv.redirects.push_back(url1b);
  list.push_back(mv);

  // Save our most visited data containing that one site.
  SetTopSites(list);

  // Create a dummy thumbnail.
  SkBitmap thumbnail(CreateBitmap(SK_ColorWHITE));

  base::Time now = base::Time::Now();
  ThumbnailScore low_score(1.0, true, true, now);
  ThumbnailScore medium_score(0.5, true, true, now);
  ThumbnailScore high_score(0.0, true, true, now);

  // Setting the thumbnail for invalid pages should fail.
  EXPECT_FALSE(top_sites()->SetPageThumbnail(invalid_url,
                                             thumbnail, medium_score));

  // Setting the thumbnail for url2 should succeed, lower scores shouldn't
  // replace it, higher scores should.
  EXPECT_TRUE(top_sites()->SetPageThumbnail(url2, thumbnail, medium_score));
  EXPECT_FALSE(top_sites()->SetPageThumbnail(url2, thumbnail, low_score));
  EXPECT_TRUE(top_sites()->SetPageThumbnail(url2, thumbnail, high_score));

  // Set on the redirect source should succeed. It should be replacable by
  // the same score on the redirect destination, which in turn should not
  // be replaced by the source again.
  EXPECT_TRUE(top_sites()->SetPageThumbnail(url1a, thumbnail, medium_score));
  EXPECT_TRUE(top_sites()->SetPageThumbnail(url1b, thumbnail, medium_score));
  EXPECT_FALSE(top_sites()->SetPageThumbnail(url1a, thumbnail, medium_score));
}

// Makes sure a thumbnail is correctly removed when the page is removed.
TEST_F(TopSitesTest, ThumbnailRemoved) {
  GURL url("http://google.com/");

  // Configure top sites with 'google.com'.
  std::vector<MostVisitedURL> list;
  AppendMostVisitedURL(&list, url);
  SetTopSites(list);

  // Create a dummy thumbnail.
  SkBitmap thumbnail(CreateBitmap(SK_ColorRED));

  base::Time now = base::Time::Now();
  ThumbnailScore low_score(1.0, true, true, now);
  ThumbnailScore medium_score(0.5, true, true, now);
  ThumbnailScore high_score(0.0, true, true, now);

  // Set the thumbnail.
  EXPECT_TRUE(top_sites()->SetPageThumbnail(url, thumbnail, medium_score));

  // Make sure the thumbnail was actually set.
  scoped_refptr<RefCountedBytes> result;
  EXPECT_TRUE(top_sites()->GetPageThumbnail(url, &result));
  EXPECT_TRUE(ThumbnailEqualsBytes(thumbnail, result.get()));

  // Reset the thumbnails and make sure we don't get it back.
  SetTopSites(MostVisitedURLList());
  EXPECT_FALSE(top_sites()->GetPageThumbnail(url, &result));
}

// Tests GetPageThumbnail.
TEST_F(TopSitesTest, GetPageThumbnail) {
  MostVisitedURLList url_list;
  MostVisitedURL url1;
  url1.url = GURL("http://asdf.com");
  url1.redirects.push_back(url1.url);
  url_list.push_back(url1);

  MostVisitedURL url2;
  url2.url = GURL("http://gmail.com");
  url2.redirects.push_back(url2.url);
  url2.redirects.push_back(GURL("http://mail.google.com"));
  url_list.push_back(url2);

  SetTopSites(url_list);

  // Create a dummy thumbnail.
  SkBitmap thumbnail(CreateBitmap(SK_ColorWHITE));
  ThumbnailScore score(0.5, true, true, base::Time::Now());

  scoped_refptr<RefCountedBytes> result;
  EXPECT_TRUE(top_sites()->SetPageThumbnail(url1.url, thumbnail, score));
  EXPECT_TRUE(top_sites()->GetPageThumbnail(url1.url, &result));

  EXPECT_TRUE(top_sites()->SetPageThumbnail(GURL("http://gmail.com"),
                                           thumbnail, score));
  EXPECT_TRUE(top_sites()->GetPageThumbnail(GURL("http://gmail.com"),
                                            &result));
  // Get a thumbnail via a redirect.
  EXPECT_TRUE(top_sites()->GetPageThumbnail(GURL("http://mail.google.com"),
                                            &result));

  EXPECT_TRUE(top_sites()->SetPageThumbnail(GURL("http://mail.google.com"),
                                           thumbnail, score));
  EXPECT_TRUE(top_sites()->GetPageThumbnail(url2.url, &result));

  EXPECT_TRUE(ThumbnailEqualsBytes(thumbnail, result.get()));
}

// Tests GetMostVisitedURLs.
TEST_F(TopSitesTest, GetMostVisited) {
  GURL news("http://news.google.com/");
  GURL google("http://google.com/");

  AddPageToHistory(news);
  AddPageToHistory(google);

  StartQueryForMostVisited();
  WaitForHistory();

  TopSitesQuerier querier;
  querier.QueryTopSites(top_sites(), false);

  ASSERT_EQ(1, querier.number_of_callbacks());

  // 2 extra prepopulated URLs.
  ASSERT_EQ(2u + GetPrepopulatePages().size(), querier.urls().size());
  EXPECT_EQ(news, querier.urls()[0].url);
  EXPECT_EQ(google, querier.urls()[1].url);
  ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 2));
}

// Makes sure changes done to top sites get mirrored to the db.
TEST_F(TopSitesTest, SaveToDB) {
  MostVisitedURL url;
  GURL asdf_url("http://asdf.com");
  string16 asdf_title(ASCIIToUTF16("ASDF"));
  GURL google_url("http://google.com");
  string16 google_title(ASCIIToUTF16("Google"));
  GURL news_url("http://news.google.com");
  string16 news_title(ASCIIToUTF16("Google News"));

  // Add asdf_url to history.
  AddPageToHistory(asdf_url, asdf_title);

  // Make TopSites reread from the db.
  StartQueryForMostVisited();
  WaitForHistory();

  // Add a thumbnail.
  SkBitmap tmp_bitmap(CreateBitmap(SK_ColorBLUE));
  ASSERT_TRUE(top_sites()->SetPageThumbnail(asdf_url, tmp_bitmap,
                                            ThumbnailScore()));

  RecreateTopSitesAndBlock();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);
    ASSERT_EQ(1u + GetPrepopulatePages().size(), querier.urls().size());
    EXPECT_EQ(asdf_url, querier.urls()[0].url);
    EXPECT_EQ(asdf_title, querier.urls()[0].title);
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 1));

    scoped_refptr<RefCountedBytes> read_data;
    EXPECT_TRUE(top_sites()->GetPageThumbnail(asdf_url, &read_data));
    EXPECT_TRUE(ThumbnailEqualsBytes(tmp_bitmap, read_data.get()));
  }

  MostVisitedURL url2;
  url2.url = google_url;
  url2.title = google_title;
  url2.redirects.push_back(url2.url);

  AddPageToHistory(url2.url, url2.title);

  // Add new thumbnail at rank 0 and shift the other result to 1.
  ASSERT_TRUE(top_sites()->SetPageThumbnail(google_url,
                                            tmp_bitmap,
                                            ThumbnailScore()));

  // Make TopSites reread from the db.
  RefreshTopSitesAndRecreate();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);
    ASSERT_EQ(2u + GetPrepopulatePages().size(), querier.urls().size());
    EXPECT_EQ(asdf_url, querier.urls()[0].url);
    EXPECT_EQ(asdf_title, querier.urls()[0].title);
    EXPECT_EQ(google_url, querier.urls()[1].url);
    EXPECT_EQ(google_title, querier.urls()[1].title);
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 2));
  }
}

// More permutations of saving to db.
TEST_F(TopSitesTest, RealDatabase) {
  MostVisitedURL url;
  GURL asdf_url("http://asdf.com");
  string16 asdf_title(ASCIIToUTF16("ASDF"));
  GURL google1_url("http://google.com");
  GURL google2_url("http://google.com/redirect");
  GURL google3_url("http://www.google.com");
  string16 google_title(ASCIIToUTF16("Google"));
  GURL news_url("http://news.google.com");
  string16 news_title(ASCIIToUTF16("Google News"));

  url.url = asdf_url;
  url.title = asdf_title;
  url.redirects.push_back(url.url);
  SkBitmap asdf_thumbnail(CreateBitmap(SK_ColorRED));
  ASSERT_TRUE(top_sites()->SetPageThumbnail(
                  asdf_url, asdf_thumbnail, ThumbnailScore()));

  base::Time add_time(base::Time::Now());
  AddPageToHistory(url.url, url.title, url.redirects, add_time);

  RefreshTopSitesAndRecreate();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);

    ASSERT_EQ(1u + GetPrepopulatePages().size(), querier.urls().size());
    EXPECT_EQ(asdf_url, querier.urls()[0].url);
    EXPECT_EQ(asdf_title, querier.urls()[0].title);
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 1));

    scoped_refptr<RefCountedBytes> read_data;
    EXPECT_TRUE(top_sites()->GetPageThumbnail(asdf_url, &read_data));
    EXPECT_TRUE(ThumbnailEqualsBytes(asdf_thumbnail, read_data.get()));
  }

  MostVisitedURL url2;
  url2.url = google3_url;
  url2.title = google_title;
  url2.redirects.push_back(google1_url);
  url2.redirects.push_back(google2_url);
  url2.redirects.push_back(google3_url);

  AddPageToHistory(google3_url, url2.title, url2.redirects,
                   add_time - base::TimeDelta::FromMinutes(1));
  // Add google twice so that it becomes the first visited site.
  AddPageToHistory(google3_url, url2.title, url2.redirects,
                   add_time - base::TimeDelta::FromMinutes(2));

  SkBitmap google_thumbnail(CreateBitmap(SK_ColorBLUE));
  ASSERT_TRUE(top_sites()->SetPageThumbnail(
                  url2.url, google_thumbnail, ThumbnailScore()));

  RefreshTopSitesAndRecreate();

  {
    scoped_refptr<RefCountedBytes> read_data;
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);

    ASSERT_EQ(2u + GetPrepopulatePages().size(), querier.urls().size());
    EXPECT_EQ(google1_url, querier.urls()[0].url);
    EXPECT_EQ(google_title, querier.urls()[0].title);
    ASSERT_EQ(3u, querier.urls()[0].redirects.size());
    EXPECT_TRUE(top_sites()->GetPageThumbnail(google3_url, &read_data));
    EXPECT_TRUE(ThumbnailEqualsBytes(google_thumbnail, read_data.get()));

    EXPECT_EQ(asdf_url, querier.urls()[1].url);
    EXPECT_EQ(asdf_title, querier.urls()[1].title);
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 2));
  }

  SkBitmap weewar_bitmap(CreateBitmap(SK_ColorYELLOW));

  base::Time thumbnail_time(base::Time::Now());
  ThumbnailScore low_score(1.0, true, true, thumbnail_time);
  ThumbnailScore medium_score(0.5, true, true, thumbnail_time);
  ThumbnailScore high_score(0.0, true, true, thumbnail_time);

  // 1. Set to weewar. (Writes the thumbnail to the DB.)
  EXPECT_TRUE(top_sites()->SetPageThumbnail(google3_url,
                                            weewar_bitmap,
                                            medium_score));
  RefreshTopSitesAndRecreate();
  {
    scoped_refptr<RefCountedBytes> read_data;
    EXPECT_TRUE(top_sites()->GetPageThumbnail(google3_url, &read_data));
    EXPECT_TRUE(ThumbnailEqualsBytes(weewar_bitmap, read_data.get()));
  }

  SkBitmap google_bitmap(CreateBitmap(SK_ColorGREEN));

  // 2. Set to google - low score.
  EXPECT_FALSE(top_sites()->SetPageThumbnail(google3_url,
                                             google_bitmap,
                                             low_score));

  // 3. Set to google - high score.
  EXPECT_TRUE(top_sites()->SetPageThumbnail(google1_url,
                                            google_bitmap,
                                            high_score));

  // Check that the thumbnail was updated.
  RefreshTopSitesAndRecreate();
  {
    scoped_refptr<RefCountedBytes> read_data;
    EXPECT_TRUE(top_sites()->GetPageThumbnail(google3_url, &read_data));
    EXPECT_FALSE(ThumbnailEqualsBytes(weewar_bitmap, read_data.get()));
    EXPECT_TRUE(ThumbnailEqualsBytes(google_bitmap, read_data.get()));
  }
}

TEST_F(TopSitesTest, DeleteNotifications) {
  GURL google1_url("http://google.com");
  GURL google2_url("http://google.com/redirect");
  GURL google3_url("http://www.google.com");
  string16 google_title(ASCIIToUTF16("Google"));
  GURL news_url("http://news.google.com");
  string16 news_title(ASCIIToUTF16("Google News"));

  AddPageToHistory(google1_url, google_title);
  AddPageToHistory(news_url, news_title);

  RefreshTopSitesAndRecreate();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);

    ASSERT_EQ(4u, querier.urls().size());
  }

  DeleteURL(news_url);

  // Wait for history to process the deletion.
  WaitForHistory();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);

    ASSERT_EQ(1u + GetPrepopulatePages().size(), querier.urls().size());
    EXPECT_EQ(google_title, querier.urls()[0].title);
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 1));
  }

  // Now reload. This verifies topsites actually wrote the deletion to disk.
  RefreshTopSitesAndRecreate();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);

    ASSERT_EQ(1u + GetPrepopulatePages().size(), querier.urls().size());
    EXPECT_EQ(google_title, querier.urls()[0].title);
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 1));
  }

  DeleteURL(google1_url);

  // Wait for history to process the deletion.
  WaitForHistory();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);

    ASSERT_EQ(GetPrepopulatePages().size(), querier.urls().size());
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 0));
  }

  // Now reload. This verifies topsites actually wrote the deletion to disk.
  RefreshTopSitesAndRecreate();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);

    ASSERT_EQ(GetPrepopulatePages().size(), querier.urls().size());
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 0));
  }
}

TEST_F(TopSitesTest, PinnedURLsDeleted) {
  GURL google1_url("http://google.com");
  GURL google2_url("http://google.com/redirect");
  GURL google3_url("http://www.google.com");
  string16 google_title(ASCIIToUTF16("Google"));
  GURL news_url("http://news.google.com");
  string16 news_title(ASCIIToUTF16("Google News"));

  AddPageToHistory(google1_url, google_title);
  AddPageToHistory(news_url, news_title);

  RefreshTopSitesAndRecreate();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);

    // 2 extra prepopulated URLs.
    ASSERT_EQ(4u, querier.urls().size());
  }

  top_sites()->AddPinnedURL(news_url, 3);
  EXPECT_TRUE(top_sites()->IsURLPinned(news_url));

  DeleteURL(news_url);
  WaitForHistory();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);

    // 2 extra prepopulated URLs.
    ASSERT_EQ(3u, querier.urls().size());
    EXPECT_FALSE(top_sites()->IsURLPinned(news_url));
  }

  history_service()->ExpireHistoryBetween(
      std::set<GURL>(), base::Time(), base::Time(),
      consumer(), NewCallback(static_cast<TopSitesTest*>(this),
                              &TopSitesTest::EmptyCallback)),
  WaitForHistory();

  {
    TopSitesQuerier querier;
    querier.QueryTopSites(top_sites(), false);

    // 2 extra prepopulated URLs.
    ASSERT_EQ(GetPrepopulatePages().size(), querier.urls().size());
    EXPECT_FALSE(top_sites()->IsURLPinned(google1_url));
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier, 0));
  }
}

// Makes sure GetUpdateDelay is updated appropriately.
TEST_F(TopSitesTest, GetUpdateDelay) {
  SetLastNumUrlsChanged(0);
  EXPECT_EQ(30, GetUpdateDelay().InSeconds());

  MostVisitedURLList url_list;
  url_list.resize(20);
  GURL tmp_url(GURL("http://x"));
  for (size_t i = 0; i < url_list.size(); ++i) {
    url_list[i].url = tmp_url;
    url_list[i].redirects.push_back(tmp_url);
  }
  SetTopSites(url_list);
  EXPECT_EQ(20u, last_num_urls_changed());
  SetLastNumUrlsChanged(0);
  EXPECT_EQ(60, GetUpdateDelay().InMinutes());

  SetLastNumUrlsChanged(3);
  EXPECT_EQ(52, GetUpdateDelay().InMinutes());

  SetLastNumUrlsChanged(20);
  EXPECT_EQ(1, GetUpdateDelay().InMinutes());
}

TEST_F(TopSitesMigrationTest, Migrate) {
  EXPECT_TRUE(IsTopSitesLoaded());

  // Make sure the data was migrated to top sites.
  ASSERT_NO_FATAL_FAILURE(MigrationAssertions());

  // We need to wait for top sites and history to finish processing requests.
  WaitForTopSites();
  WaitForHistory();

  // Make sure there is no longer a Thumbnails file on disk.
  ASSERT_FALSE(file_util::PathExists(
                   profile()->GetPath().Append(chrome::kThumbnailsFilename)));

  // Recreate top sites and make sure everything is still there.
  profile()->CreateHistoryService(false, false);
  RecreateTopSitesAndBlock();

  ASSERT_NO_FATAL_FAILURE(MigrationAssertions());
}

// Verifies that callbacks are notified correctly if requested before top sites
// has loaded.
TEST_F(TopSitesTest, NotifyCallbacksWhenLoaded) {
  // Recreate top sites. It won't be loaded now.
  profile()->CreateTopSites();

  EXPECT_FALSE(IsTopSitesLoaded());

  TopSitesQuerier querier1;
  TopSitesQuerier querier2;
  TopSitesQuerier querier3;

  // Starts the queries.
  querier1.QueryTopSites(top_sites(), false);
  querier2.QueryTopSites(top_sites(), false);
  querier3.QueryTopSites(top_sites(), false);

  // We shouldn't have gotten a callback.
  EXPECT_EQ(0, querier1.number_of_callbacks());
  EXPECT_EQ(0, querier2.number_of_callbacks());
  EXPECT_EQ(0, querier3.number_of_callbacks());

  // Wait for loading to complete.
  profile()->BlockUntilTopSitesLoaded();

  // Now we should have gotten the callbacks.
  EXPECT_EQ(1, querier1.number_of_callbacks());
  EXPECT_EQ(GetPrepopulatePages().size(), querier1.urls().size());
  EXPECT_EQ(1, querier2.number_of_callbacks());
  EXPECT_EQ(GetPrepopulatePages().size(), querier2.urls().size());
  EXPECT_EQ(1, querier3.number_of_callbacks());
  EXPECT_EQ(GetPrepopulatePages().size(), querier3.urls().size());

  // Reset the top sites.
  MostVisitedURLList pages;
  MostVisitedURL url;
  url.url = GURL("http://1.com/");
  url.redirects.push_back(url.url);
  pages.push_back(url);
  url.url = GURL("http://2.com/");
  url.redirects.push_back(url.url);
  pages.push_back(url);
  SetTopSites(pages);

  // Recreate top sites. It won't be loaded now.
  profile()->CreateTopSites();

  EXPECT_FALSE(IsTopSitesLoaded());

  TopSitesQuerier querier4;

  // Query again.
  querier4.QueryTopSites(top_sites(), false);

  // We shouldn't have gotten a callback.
  EXPECT_EQ(0, querier4.number_of_callbacks());

  // Wait for loading to complete.
  profile()->BlockUntilTopSitesLoaded();

  // Now we should have gotten the callbacks.
  EXPECT_EQ(1, querier4.number_of_callbacks());
  ASSERT_EQ(2u + GetPrepopulatePages().size(), querier4.urls().size());

  EXPECT_EQ("http://1.com/", querier4.urls()[0].url.spec());
  EXPECT_EQ("http://2.com/", querier4.urls()[1].url.spec());
  ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier4, 2));

  // Reset the top sites again, this time don't reload.
  url.url = GURL("http://3.com/");
  url.redirects.push_back(url.url);
  pages.push_back(url);
  SetTopSites(pages);

  // Query again.
  TopSitesQuerier querier5;
  querier5.QueryTopSites(top_sites(), true);

  EXPECT_EQ(1, querier5.number_of_callbacks());

  ASSERT_EQ(3u + GetPrepopulatePages().size(), querier5.urls().size());
  EXPECT_EQ("http://1.com/", querier5.urls()[0].url.spec());
  EXPECT_EQ("http://2.com/", querier5.urls()[1].url.spec());
  EXPECT_EQ("http://3.com/", querier5.urls()[2].url.spec());
  ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(querier5, 3));
}

// Makes sure canceled requests are not notified.
TEST_F(TopSitesTest, CancelingRequestsForTopSites) {
  // Recreate top sites. It won't be loaded now.
  profile()->CreateTopSites();

  EXPECT_FALSE(IsTopSitesLoaded());

  TopSitesQuerier querier1;
  TopSitesQuerier querier2;

  // Starts the queries.
  querier1.QueryTopSites(top_sites(), false);
  querier2.QueryTopSites(top_sites(), false);

  // We shouldn't have gotten a callback.
  EXPECT_EQ(0, querier1.number_of_callbacks());
  EXPECT_EQ(0, querier2.number_of_callbacks());

  querier2.CancelRequest();

  // Wait for loading to complete.
  profile()->BlockUntilTopSitesLoaded();

  // The first callback should succeed.
  EXPECT_EQ(1, querier1.number_of_callbacks());
  EXPECT_EQ(GetPrepopulatePages().size(), querier1.urls().size());

  // And the canceled callback should not be notified.
  EXPECT_EQ(0, querier2.number_of_callbacks());
}

// Makes sure temporary thumbnails are copied over correctly.
TEST_F(TopSitesTest, AddTemporaryThumbnail) {
  GURL unknown_url("http://news.google.com/");
  GURL invalid_url("chrome://thumb/http://google.com/");
  GURL url1a("http://google.com/");
  GURL url1b("http://www.google.com/");

  // Create a dummy thumbnail.
  SkBitmap thumbnail(CreateBitmap(SK_ColorRED));

  ThumbnailScore medium_score(0.5, true, true, base::Time::Now());

  // Don't store thumbnails for Javascript URLs.
  EXPECT_FALSE(top_sites()->SetPageThumbnail(invalid_url,
                                             thumbnail,
                                             medium_score));
  // Store thumbnails for unknown (but valid) URLs temporarily - calls
  // AddTemporaryThumbnail.
  EXPECT_TRUE(top_sites()->SetPageThumbnail(unknown_url,
                                            thumbnail,
                                            medium_score));

  // We shouldn't get the thumnail back though (the url isn't in to sites yet).
  scoped_refptr<RefCountedBytes> out;
  EXPECT_FALSE(top_sites()->GetPageThumbnail(unknown_url, &out));

  std::vector<MostVisitedURL> list;

  MostVisitedURL mv;
  mv.url = unknown_url;
  mv.redirects.push_back(mv.url);
  mv.redirects.push_back(url1a);
  mv.redirects.push_back(url1b);
  list.push_back(mv);

  // Update URLs. This should result in using thumbnail.
  SetTopSites(list);

  ASSERT_TRUE(top_sites()->GetPageThumbnail(unknown_url, &out));
  EXPECT_TRUE(ThumbnailEqualsBytes(thumbnail, out.get()));
}

// Tests variations of blacklisting.
TEST_F(TopSitesTest, Blacklisting) {
  MostVisitedURLList pages;
  MostVisitedURL url, url1;
  url.url = GURL("http://bbc.com/");
  url.redirects.push_back(url.url);
  pages.push_back(url);
  url1.url = GURL("http://google.com/");
  url1.redirects.push_back(url1.url);
  pages.push_back(url1);

  SetTopSites(pages);
  EXPECT_FALSE(top_sites()->IsBlacklisted(GURL("http://bbc.com/")));

  // Blacklist google.com.
  top_sites()->AddBlacklistedURL(GURL("http://google.com/"));

  GURL prepopulate_url = GetPrepopulatePages()[0].url;

  EXPECT_TRUE(top_sites()->HasBlacklistedItems());
  EXPECT_TRUE(top_sites()->IsBlacklisted(GURL("http://google.com/")));
  EXPECT_FALSE(top_sites()->IsBlacklisted(GURL("http://bbc.com/")));
  EXPECT_FALSE(top_sites()->IsBlacklisted(prepopulate_url));

  // Make sure the blacklisted site isn't returned in the results.
  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);
    ASSERT_EQ(1u + GetPrepopulatePages().size(), q.urls().size());
    EXPECT_EQ("http://bbc.com/", q.urls()[0].url.spec());
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(q, 1));
  }

  // Recreate top sites and make sure blacklisted url was correctly read.
  RecreateTopSitesAndBlock();
  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);
    ASSERT_EQ(1u + GetPrepopulatePages().size(), q.urls().size());
    EXPECT_EQ("http://bbc.com/", q.urls()[0].url.spec());
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(q, 1));
  }

  // Blacklist one of the prepopulate urls.
  top_sites()->AddBlacklistedURL(prepopulate_url);
  EXPECT_TRUE(top_sites()->HasBlacklistedItems());

  // Make sure the blacklisted prepopulate url isn't returned.
  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);
    ASSERT_EQ(1u + GetPrepopulatePages().size() - 1, q.urls().size());
    EXPECT_EQ("http://bbc.com/", q.urls()[0].url.spec());
    for (size_t i = 1; i < q.urls().size(); ++i)
      EXPECT_NE(prepopulate_url.spec(), q.urls()[i].url.spec());
  }

  // Mark google as no longer blacklisted.
  top_sites()->RemoveBlacklistedURL(GURL("http://google.com/"));
  EXPECT_TRUE(top_sites()->HasBlacklistedItems());
  EXPECT_FALSE(top_sites()->IsBlacklisted(GURL("http://google.com/")));

  // Make sure google is returned now.
  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);
    ASSERT_EQ(2u + GetPrepopulatePages().size() - 1, q.urls().size());
    EXPECT_EQ("http://bbc.com/", q.urls()[0].url.spec());
    EXPECT_EQ("http://google.com/", q.urls()[1].url.spec());
    EXPECT_NE(prepopulate_url.spec(), q.urls()[2].url.spec());
  }

  // Remove all blacklisted sites.
  top_sites()->ClearBlacklistedURLs();
  EXPECT_FALSE(top_sites()->HasBlacklistedItems());

  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);
    ASSERT_EQ(2u + GetPrepopulatePages().size(), q.urls().size());
    EXPECT_EQ("http://bbc.com/", q.urls()[0].url.spec());
    EXPECT_EQ("http://google.com/", q.urls()[1].url.spec());
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(q, 2));
  }
}

// Tests variations of pinning/unpinning urls.
TEST_F(TopSitesTest, PinnedURLs) {
  MostVisitedURLList pages;
  MostVisitedURL url, url1;
  url.url = GURL("http://bbc.com/");
  url.redirects.push_back(url.url);
  pages.push_back(url);
  url1.url = GURL("http://google.com/");
  url1.redirects.push_back(url1.url);
  pages.push_back(url1);

  SetTopSites(pages);

  EXPECT_FALSE(top_sites()->IsURLPinned(GURL("http://bbc.com/")));

  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);
    ASSERT_EQ(2u + GetPrepopulatePages().size(), q.urls().size());
    EXPECT_EQ("http://bbc.com/", q.urls()[0].url.spec());
    EXPECT_EQ("http://google.com/", q.urls()[1].url.spec());
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(q, 2));
  }

  top_sites()->AddPinnedURL(GURL("http://google.com/"), 3);
  EXPECT_FALSE(top_sites()->IsURLPinned(GURL("http://bbc.com/")));
  EXPECT_FALSE(top_sites()->IsURLPinned(GetPrepopulatePages()[0].url));

  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);
    ASSERT_EQ(4u, q.urls().size());
    EXPECT_EQ("http://bbc.com/", q.urls()[0].url.spec());
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(q, 1));
    EXPECT_EQ("http://google.com/", q.urls()[3].url.spec());
  }

  top_sites()->RemovePinnedURL(GURL("http://google.com/"));
  EXPECT_FALSE(top_sites()->IsURLPinned(GURL("http://google.com/")));
  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);

    ASSERT_EQ(2u + GetPrepopulatePages().size(), q.urls().size());
    EXPECT_EQ("http://bbc.com/", q.urls()[0].url.spec());
    EXPECT_EQ("http://google.com/", q.urls()[1].url.spec());
    ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(q, 2));
  }

  GURL prepopulate_url = GetPrepopulatePages()[0].url;
  top_sites()->AddPinnedURL(GURL("http://bbc.com"), 1);
  top_sites()->AddPinnedURL(prepopulate_url, 0);
  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);

    ASSERT_EQ(3u + GetPrepopulatePages().size() - 1, q.urls().size());
    EXPECT_EQ(prepopulate_url, q.urls()[0].url);
    EXPECT_EQ("http://bbc.com/", q.urls()[1].url.spec());
    EXPECT_EQ("http://google.com/", q.urls()[2].url.spec());
    if (GetPrepopulatePages().size() > 1)
      EXPECT_EQ(GetPrepopulatePages()[1].url, q.urls()[3].url);
  }

  // Recreate and make sure state remains the same.
  RecreateTopSitesAndBlock();
  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);

    ASSERT_EQ(3u + GetPrepopulatePages().size() - 1, q.urls().size());
    EXPECT_EQ(prepopulate_url, q.urls()[0].url);
    EXPECT_EQ("http://bbc.com/", q.urls()[1].url.spec());
    EXPECT_EQ("http://google.com/", q.urls()[2].url.spec());
    if (GetPrepopulatePages().size() > 1)
      EXPECT_EQ(GetPrepopulatePages()[1].url, q.urls()[3].url);
  }
}

// Tests blacklisting and pinning.
TEST_F(TopSitesTest, BlacklistingAndPinnedURLs) {
  MostVisitedURLList prepopulate_urls = GetPrepopulatePages();
  if (prepopulate_urls.size() < 2)
    return;

  top_sites()->AddPinnedURL(prepopulate_urls[0].url, 1);
  top_sites()->AddBlacklistedURL(prepopulate_urls[1].url);

  {
    TopSitesQuerier q;
    q.QueryTopSites(top_sites(), true);

    ASSERT_LE(2u, q.urls().size());
    EXPECT_EQ(GURL(), q.urls()[0].url);
    EXPECT_EQ(prepopulate_urls[0].url, q.urls()[1].url);
  }
}

// Makes sure prepopulated pages exist.
TEST_F(TopSitesTest, AddPrepopulatedPages) {
  TopSitesQuerier q;
  q.QueryTopSites(top_sites(), true);
  EXPECT_EQ(GetPrepopulatePages().size(), q.urls().size());
  ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(q, 0));

  MostVisitedURLList pages = q.urls();
  EXPECT_FALSE(AddPrepopulatedPages(&pages));

  EXPECT_EQ(GetPrepopulatePages().size(), pages.size());
  q.set_urls(pages);
  ASSERT_NO_FATAL_FAILURE(ContainsPrepopulatePages(q, 0));
}

// Makes sure creating top sites before history is created works.
TEST_F(TopSitesTest, CreateTopSitesThenHistory) {
  profile()->DestroyTopSites();
  profile()->DestroyHistoryService();

  // Remove the TopSites file. This forces TopSites to wait until history loads
  // before TopSites is considered loaded.
  file_util::Delete(profile()->GetPath().Append(chrome::kTopSitesFilename),
                    false);

  // Create TopSites, but not History.
  profile()->CreateTopSites();
  WaitForTopSites();
  EXPECT_FALSE(IsTopSitesLoaded());

  // Load history, which should make TopSites finish loading too.
  profile()->CreateHistoryService(false, false);
  profile()->BlockUntilTopSitesLoaded();
  EXPECT_TRUE(IsTopSitesLoaded());
}

}  // namespace history
