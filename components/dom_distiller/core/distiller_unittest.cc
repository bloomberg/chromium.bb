// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/dom_distiller/core/article_distillation_update.h"
#include "components/dom_distiller/core/distiller.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::vector;
using std::string;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;

namespace {
const char kTitle[] = "Title";
const char kContent[] = "Content";
const char kURL[] = "http://a.com/";
const size_t kTotalImages = 2;
const char* kImageURLs[kTotalImages] = {"http://a.com/img1.jpg",
                                        "http://a.com/img2.jpg"};
const char* kImageData[kTotalImages] = {"abcde", "12345"};

const string GetImageName(int page_num, int image_num) {
  return base::IntToString(page_num) + "_" + base::IntToString(image_num);
}

scoped_ptr<base::ListValue> CreateDistilledValueReturnedFromJS(
    const string& title,
    const string& content,
    const vector<int>& image_indices,
    const string& next_page_url,
    const string& prev_page_url = "") {
  scoped_ptr<base::ListValue> list(new base::ListValue());

  list->AppendString(title);
  list->AppendString(content);
  list->AppendString(next_page_url);
  list->AppendString(prev_page_url);
  for (size_t i = 0; i < image_indices.size(); ++i) {
    list->AppendString(kImageURLs[image_indices[i]]);
  }
  return list.Pass();
}

// Return the sequence in which Distiller will distill pages.
// Note: ignores any delays due to fetching images etc.
vector<int> GetPagesInSequence(int start_page_num, int num_pages) {
  // Distiller prefers distilling past pages first. E.g. when distillation
  // starts on page 2 then pages are distilled in the order: 2, 1, 0, 3, 4.
  vector<int> page_nums;
  for (int page = start_page_num; page >= 0; --page)
    page_nums.push_back(page);
  for (int page = start_page_num + 1; page < num_pages; ++page)
    page_nums.push_back(page);
  return page_nums;
}

struct MultipageDistillerData {
 public:
  MultipageDistillerData() {}
  ~MultipageDistillerData() {}
  vector<string> page_urls;
  vector<string> content;
  vector<vector<int> > image_ids;
  // The Javascript values returned by mock distiller.
  ScopedVector<base::Value> distilled_values;

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipageDistillerData);
};

void VerifyIncrementalUpdatesMatch(
    const MultipageDistillerData* distiller_data,
    int num_pages_in_article,
    const vector<dom_distiller::ArticleDistillationUpdate>& incremental_updates,
    int start_page_num) {
  vector<int> page_seq =
      GetPagesInSequence(start_page_num, num_pages_in_article);
  // Updates should contain a list of pages. Pages in an update should be in
  // the correct ascending page order regardless of |start_page_num|.
  // E.g. if distillation starts at page 2 of a 3 page article, the updates
  // will be [[2], [1, 2], [1, 2, 3]]. This example assumes that image fetches
  // do not delay distillation of a page. There can be scenarios when image
  // fetch delays distillation of a page (E.g. 1 is delayed due to image
  // fetches so updates can be in this order [[2], [2,3], [1,2,3]].
  for (size_t update_count = 0; update_count < incremental_updates.size();
       ++update_count) {
    const dom_distiller::ArticleDistillationUpdate& update =
        incremental_updates[update_count];
    EXPECT_EQ(update_count + 1, update.GetPagesSize());

    vector<int> expected_page_nums_in_update(
        page_seq.begin(), page_seq.begin() + update.GetPagesSize());
    std::sort(expected_page_nums_in_update.begin(),
              expected_page_nums_in_update.end());

    // If we already got the first page then there is no previous page.
    EXPECT_EQ((expected_page_nums_in_update[0] != 0), update.HasPrevPage());

    // if we already got the last page then there is no next page.
    EXPECT_EQ(
        (*expected_page_nums_in_update.rbegin()) != num_pages_in_article - 1,
        update.HasNextPage());
    for (size_t j = 0; j < update.GetPagesSize(); ++j) {
      int actual_page_num = expected_page_nums_in_update[j];
      EXPECT_EQ(distiller_data->page_urls[actual_page_num],
                update.GetDistilledPage(j).url());
      EXPECT_EQ(distiller_data->content[actual_page_num],
                update.GetDistilledPage(j).html());
    }
  }
}

scoped_ptr<MultipageDistillerData> CreateMultipageDistillerDataWithoutImages(
    size_t pages_size) {
  scoped_ptr<MultipageDistillerData> result(new MultipageDistillerData());
  string url_prefix = "http://a.com/";
  for (size_t page_num = 0; page_num < pages_size; ++page_num) {
    result->page_urls.push_back(url_prefix + base::IntToString(page_num));
    result->content.push_back("Content for page:" +
                              base::IntToString(page_num));
    result->image_ids.push_back(vector<int>());
    string next_page_url = (page_num + 1 < pages_size)
                               ? url_prefix + base::IntToString(page_num + 1)
                               : "";
    string prev_page_url =
        (page_num > 0) ? result->page_urls[page_num - 1] : "";
    scoped_ptr<base::ListValue> distilled_value =
        CreateDistilledValueReturnedFromJS(kTitle,
                                           result->content[page_num],
                                           result->image_ids[page_num],
                                           next_page_url,
                                           prev_page_url);
    result->distilled_values.push_back(distilled_value.release());
  }
  return result.Pass();
}

void VerifyArticleProtoMatchesMultipageData(
    const dom_distiller::DistilledArticleProto* article_proto,
    const MultipageDistillerData* distiller_data,
    size_t pages_size) {
  EXPECT_EQ(pages_size, static_cast<size_t>(article_proto->pages_size()));
  EXPECT_EQ(kTitle, article_proto->title());
  for (size_t page_num = 0; page_num < pages_size; ++page_num) {
    const dom_distiller::DistilledPageProto& page =
        article_proto->pages(page_num);
    EXPECT_EQ(distiller_data->content[page_num], page.html());
    EXPECT_EQ(distiller_data->page_urls[page_num], page.url());
    EXPECT_EQ(distiller_data->image_ids[page_num].size(),
              static_cast<size_t>(page.image_size()));
    const vector<int>& image_ids_for_page = distiller_data->image_ids[page_num];
    for (size_t img_num = 0; img_num < image_ids_for_page.size(); ++img_num) {
      EXPECT_EQ(kImageData[image_ids_for_page[img_num]],
                page.image(img_num).data());
      EXPECT_EQ(GetImageName(page_num + 1, img_num),
                page.image(img_num).name());
    }
  }
}

}  // namespace

namespace dom_distiller {

class TestDistillerURLFetcher : public DistillerURLFetcher {
 public:
  explicit TestDistillerURLFetcher(bool delay_fetch)
      : DistillerURLFetcher(NULL), delay_fetch_(delay_fetch) {
    responses_[kImageURLs[0]] = string(kImageData[0]);
    responses_[kImageURLs[1]] = string(kImageData[1]);
  }

  virtual void FetchURL(const string& url,
                        const URLFetcherCallback& callback) OVERRIDE {
    DCHECK(callback_.is_null());
    url_ = url;
    callback_ = callback;
    if (!delay_fetch_) {
      PostCallbackTask();
    }
  }

  void PostCallbackTask() {
    ASSERT_TRUE(base::MessageLoop::current());
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback_, responses_[url_]));
  }

 private:
  std::map<string, string> responses_;
  string url_;
  URLFetcherCallback callback_;
  bool delay_fetch_;
};

class TestDistillerURLFetcherFactory : public DistillerURLFetcherFactory {
 public:
  TestDistillerURLFetcherFactory() : DistillerURLFetcherFactory(NULL) {}

  virtual ~TestDistillerURLFetcherFactory() {}
  virtual DistillerURLFetcher* CreateDistillerURLFetcher() const OVERRIDE {
    return new TestDistillerURLFetcher(false);
  }
};

class MockDistillerURLFetcherFactory : public DistillerURLFetcherFactory {
 public:
  MockDistillerURLFetcherFactory()
      : DistillerURLFetcherFactory(NULL) {}
  virtual ~MockDistillerURLFetcherFactory() {}

  MOCK_CONST_METHOD0(CreateDistillerURLFetcher, DistillerURLFetcher*());
};

class MockDistillerPage : public DistillerPage {
 public:
  MOCK_METHOD0(InitImpl, void());
  MOCK_METHOD1(LoadURLImpl, void(const GURL& gurl));
  MOCK_METHOD1(ExecuteJavaScriptImpl, void(const string& script));

  MockDistillerPage() {}
};

class MockDistillerPageFactory : public DistillerPageFactory {
 public:
  MOCK_CONST_METHOD0(CreateDistillerPageMock, DistillerPage*());

  virtual scoped_ptr<DistillerPage> CreateDistillerPage() const OVERRIDE {
    return scoped_ptr<DistillerPage>(CreateDistillerPageMock());
  }
};

class DistillerTest : public testing::Test {
 public:
  virtual ~DistillerTest() {}
  void OnDistillArticleDone(scoped_ptr<DistilledArticleProto> proto) {
    article_proto_ = proto.Pass();
  }

  void OnDistillArticleUpdate(const ArticleDistillationUpdate& article_update) {
    in_sequence_updates_.push_back(article_update);
  }

  void DistillPage(const std::string& url) {
    distiller_->DistillPage(GURL(url),
                            base::Bind(&DistillerTest::OnDistillArticleDone,
                                       base::Unretained(this)),
                            base::Bind(&DistillerTest::OnDistillArticleUpdate,
                                       base::Unretained(this)));
  }

 protected:
  scoped_ptr<DistillerImpl> distiller_;
  scoped_ptr<DistilledArticleProto> article_proto_;
  std::vector<ArticleDistillationUpdate> in_sequence_updates_;
  MockDistillerPageFactory page_factory_;
  TestDistillerURLFetcherFactory url_fetcher_factory_;
};

ACTION_P3(DistillerPageOnExecuteJavaScriptDone, distiller_page, url, list) {
  distiller_page->OnExecuteJavaScriptDone(url, list);
}

ACTION_P2(CreateMockDistillerPage, list, kurl) {
  MockDistillerPage* distiller_page = new MockDistillerPage();
  EXPECT_CALL(*distiller_page, InitImpl());
  EXPECT_CALL(*distiller_page, LoadURLImpl(kurl))
      .WillOnce(testing::InvokeWithoutArgs(distiller_page,
                                           &DistillerPage::OnLoadURLDone));
  EXPECT_CALL(*distiller_page, ExecuteJavaScriptImpl(_)).WillOnce(
      DistillerPageOnExecuteJavaScriptDone(distiller_page, kurl, list));
  return distiller_page;
}

ACTION_P2(CreateMockDistillerPageWithPendingJSCallback,
          distiller_page_ptr,
          kurl) {
  MockDistillerPage* distiller_page = new MockDistillerPage();
  *distiller_page_ptr = distiller_page;
  EXPECT_CALL(*distiller_page, InitImpl());
  EXPECT_CALL(*distiller_page, LoadURLImpl(kurl))
      .WillOnce(testing::InvokeWithoutArgs(distiller_page,
                                           &DistillerPage::OnLoadURLDone));
  EXPECT_CALL(*distiller_page, ExecuteJavaScriptImpl(_));
  return distiller_page;
}

ACTION_P3(CreateMockDistillerPages,
          distiller_data,
          pages_size,
          start_page_num) {
  MockDistillerPage* distiller_page = new MockDistillerPage();
  EXPECT_CALL(*distiller_page, InitImpl());
  {
    testing::InSequence s;
    vector<int> page_nums = GetPagesInSequence(start_page_num, pages_size);
    for (size_t page_num = 0; page_num < pages_size; ++page_num) {
      int page = page_nums[page_num];
      GURL url = GURL(distiller_data->page_urls[page]);
      EXPECT_CALL(*distiller_page, LoadURLImpl(url))
          .WillOnce(testing::InvokeWithoutArgs(distiller_page,
                                               &DistillerPage::OnLoadURLDone));
      EXPECT_CALL(*distiller_page, ExecuteJavaScriptImpl(_))
          .WillOnce(DistillerPageOnExecuteJavaScriptDone(
              distiller_page, url, distiller_data->distilled_values[page]));
    }
  }
  return distiller_page;
}

TEST_F(DistillerTest, DistillPage) {
  base::MessageLoopForUI loop;
  scoped_ptr<base::ListValue> list =
      CreateDistilledValueReturnedFromJS(kTitle, kContent, vector<int>(), "");
  EXPECT_CALL(page_factory_, CreateDistillerPageMock())
      .WillOnce(CreateMockDistillerPage(list.get(), GURL(kURL)));
  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  DistillPage(kURL);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  EXPECT_EQ(article_proto_->pages_size(), 1);
  const DistilledPageProto& first_page = article_proto_->pages(0);
  EXPECT_EQ(kContent, first_page.html());
  EXPECT_EQ(kURL, first_page.url());
}

TEST_F(DistillerTest, DistillPageWithImages) {
  base::MessageLoopForUI loop;
  vector<int> image_indices;
  image_indices.push_back(0);
  image_indices.push_back(1);
  scoped_ptr<base::ListValue> list =
      CreateDistilledValueReturnedFromJS(kTitle, kContent, image_indices, "");
  EXPECT_CALL(page_factory_, CreateDistillerPageMock())
      .WillOnce(CreateMockDistillerPage(list.get(), GURL(kURL)));
  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  DistillPage(kURL);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  EXPECT_EQ(article_proto_->pages_size(), 1);
  const DistilledPageProto& first_page = article_proto_->pages(0);
  EXPECT_EQ(kContent, first_page.html());
  EXPECT_EQ(kURL, first_page.url());
  EXPECT_EQ(2, first_page.image_size());
  EXPECT_EQ(kImageData[0], first_page.image(0).data());
  EXPECT_EQ(GetImageName(1, 0), first_page.image(0).name());
  EXPECT_EQ(kImageData[1], first_page.image(1).data());
  EXPECT_EQ(GetImageName(1, 1), first_page.image(1).name());
}

TEST_F(DistillerTest, DistillMultiplePages) {
  base::MessageLoopForUI loop;
  const size_t kNumPages = 8;
  scoped_ptr<MultipageDistillerData> distiller_data =
      CreateMultipageDistillerDataWithoutImages(kNumPages);

  // Add images.
  int next_image_number = 0;
  for (size_t page_num = 0; page_num < kNumPages; ++page_num) {
    // Each page has different number of images.
    size_t tot_images = (page_num + kTotalImages) % (kTotalImages + 1);
    vector<int> image_indices;
    for (size_t img_num = 0; img_num < tot_images; img_num++) {
      image_indices.push_back(next_image_number);
      next_image_number = (next_image_number + 1) % kTotalImages;
    }
    distiller_data->image_ids.push_back(image_indices);
  }

  EXPECT_CALL(page_factory_, CreateDistillerPageMock())
      .WillOnce(CreateMockDistillerPages(distiller_data.get(), kNumPages, 0));

  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  DistillPage(distiller_data->page_urls[0]);
  base::MessageLoop::current()->RunUntilIdle();
  VerifyArticleProtoMatchesMultipageData(
      article_proto_.get(), distiller_data.get(), kNumPages);
}

TEST_F(DistillerTest, DistillLinkLoop) {
  base::MessageLoopForUI loop;
  // Create a loop, the next page is same as the current page. This could
  // happen if javascript misparses a next page link.
  scoped_ptr<base::ListValue> list =
      CreateDistilledValueReturnedFromJS(kTitle, kContent, vector<int>(), kURL);
  EXPECT_CALL(page_factory_, CreateDistillerPageMock())
      .WillOnce(CreateMockDistillerPage(list.get(), GURL(kURL)));
  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  DistillPage(kURL);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  EXPECT_EQ(article_proto_->pages_size(), 1);
}

TEST_F(DistillerTest, CheckMaxPageLimitExtraPage) {
  base::MessageLoopForUI loop;
  const size_t kMaxPagesInArticle = 10;
  scoped_ptr<MultipageDistillerData> distiller_data =
      CreateMultipageDistillerDataWithoutImages(kMaxPagesInArticle);

  // Note: Next page url of the last page of article is set. So distiller will
  // try to do kMaxPagesInArticle + 1 calls if the max article limit does not
  // work.
  scoped_ptr<base::ListValue> last_page_data =
      CreateDistilledValueReturnedFromJS(
          kTitle,
          distiller_data->content[kMaxPagesInArticle - 1],
          vector<int>(),
          "",
          distiller_data->page_urls[kMaxPagesInArticle - 2]);

  distiller_data->distilled_values.pop_back();
  distiller_data->distilled_values.push_back(last_page_data.release());

  EXPECT_CALL(page_factory_, CreateDistillerPageMock()).WillOnce(
      CreateMockDistillerPages(distiller_data.get(), kMaxPagesInArticle, 0));
  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));

  distiller_->SetMaxNumPagesInArticle(kMaxPagesInArticle);

  distiller_->Init();
  DistillPage(distiller_data->page_urls[0]);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  EXPECT_EQ(kMaxPagesInArticle,
            static_cast<size_t>(article_proto_->pages_size()));
}

TEST_F(DistillerTest, CheckMaxPageLimitExactLimit) {
  base::MessageLoopForUI loop;
  const size_t kMaxPagesInArticle = 10;
  scoped_ptr<MultipageDistillerData> distiller_data =
      CreateMultipageDistillerDataWithoutImages(kMaxPagesInArticle);

  EXPECT_CALL(page_factory_, CreateDistillerPageMock()).WillOnce(
      CreateMockDistillerPages(distiller_data.get(), kMaxPagesInArticle, 0));
  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));

  // Check if distilling an article with exactly the page limit works.
  distiller_->SetMaxNumPagesInArticle(kMaxPagesInArticle);

  distiller_->Init();

  DistillPage(distiller_data->page_urls[0]);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  EXPECT_EQ(kMaxPagesInArticle,
            static_cast<size_t>(article_proto_->pages_size()));
}

TEST_F(DistillerTest, SinglePageDistillationFailure) {
  base::MessageLoopForUI loop;
  // To simulate failure return a null value.
  scoped_ptr<base::Value> nullValue(base::Value::CreateNullValue());
  EXPECT_CALL(page_factory_, CreateDistillerPageMock())
      .WillOnce(CreateMockDistillerPage(nullValue.get(), GURL(kURL)));
  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  DistillPage(kURL);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ("", article_proto_->title());
  EXPECT_EQ(0, article_proto_->pages_size());
}

TEST_F(DistillerTest, MultiplePagesDistillationFailure) {
  base::MessageLoopForUI loop;
  const size_t kNumPages = 8;
  scoped_ptr<MultipageDistillerData> distiller_data =
      CreateMultipageDistillerDataWithoutImages(kNumPages);

  // The page number of the failed page.
  size_t failed_page_num = 3;
  // reset distilled data of the failed page.
  distiller_data->distilled_values.erase(
      distiller_data->distilled_values.begin() + failed_page_num);
  distiller_data->distilled_values.insert(
      distiller_data->distilled_values.begin() + failed_page_num,
      base::Value::CreateNullValue());
  // Expect only calls till the failed page number.
  EXPECT_CALL(page_factory_, CreateDistillerPageMock()).WillOnce(
      CreateMockDistillerPages(distiller_data.get(), failed_page_num + 1, 0));

  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  DistillPage(distiller_data->page_urls[0]);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  VerifyArticleProtoMatchesMultipageData(
      article_proto_.get(), distiller_data.get(), failed_page_num);
}

TEST_F(DistillerTest, DistillPreviousPage) {
  base::MessageLoopForUI loop;
  const size_t kNumPages = 8;

  // The page number of the article on which distillation starts.
  int start_page_num = 3;
  scoped_ptr<MultipageDistillerData> distiller_data =
      CreateMultipageDistillerDataWithoutImages(kNumPages);

  EXPECT_CALL(page_factory_, CreateDistillerPageMock())
      .WillOnce(CreateMockDistillerPages(
          distiller_data.get(), kNumPages, start_page_num));

  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  DistillPage(distiller_data->page_urls[start_page_num]);
  base::MessageLoop::current()->RunUntilIdle();
  VerifyArticleProtoMatchesMultipageData(
      article_proto_.get(), distiller_data.get(), kNumPages);
}

TEST_F(DistillerTest, IncrementalUpdates) {
  base::MessageLoopForUI loop;
  const size_t kNumPages = 8;

  // The page number of the article on which distillation starts.
  int start_page_num = 3;
  scoped_ptr<MultipageDistillerData> distiller_data =
      CreateMultipageDistillerDataWithoutImages(kNumPages);

  EXPECT_CALL(page_factory_, CreateDistillerPageMock())
      .WillOnce(CreateMockDistillerPages(
          distiller_data.get(), kNumPages, start_page_num));

  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  DistillPage(distiller_data->page_urls[start_page_num]);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  EXPECT_EQ(kNumPages, static_cast<size_t>(article_proto_->pages_size()));
  EXPECT_EQ(kNumPages, in_sequence_updates_.size());

  VerifyIncrementalUpdatesMatch(
      distiller_data.get(), kNumPages, in_sequence_updates_, start_page_num);
}

TEST_F(DistillerTest, IncrementalUpdatesDoNotDeleteFinalArticle) {
  base::MessageLoopForUI loop;
  const size_t kNumPages = 8;
  int start_page_num = 3;
  scoped_ptr<MultipageDistillerData> distiller_data =
      CreateMultipageDistillerDataWithoutImages(kNumPages);

  EXPECT_CALL(page_factory_, CreateDistillerPageMock())
      .WillOnce(CreateMockDistillerPages(
          distiller_data.get(), kNumPages, start_page_num));

  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  DistillPage(distiller_data->page_urls[start_page_num]);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kNumPages, in_sequence_updates_.size());

  in_sequence_updates_.clear();

  // Should still be able to access article and pages.
  VerifyArticleProtoMatchesMultipageData(
      article_proto_.get(), distiller_data.get(), kNumPages);
}

TEST_F(DistillerTest, DeletingArticleDoesNotInterfereWithUpdates) {
  base::MessageLoopForUI loop;
  const size_t kNumPages = 8;
  scoped_ptr<MultipageDistillerData> distiller_data =
      CreateMultipageDistillerDataWithoutImages(kNumPages);
  // The page number of the article on which distillation starts.
  int start_page_num = 3;

  EXPECT_CALL(page_factory_, CreateDistillerPageMock())
      .WillOnce(CreateMockDistillerPages(
          distiller_data.get(), kNumPages, start_page_num));

  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  DistillPage(distiller_data->page_urls[start_page_num]);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kNumPages, in_sequence_updates_.size());
  EXPECT_EQ(kTitle, article_proto_->title());
  EXPECT_EQ(kNumPages, static_cast<size_t>(article_proto_->pages_size()));

  // Delete the article.
  article_proto_.reset();
  VerifyIncrementalUpdatesMatch(
      distiller_data.get(), kNumPages, in_sequence_updates_, start_page_num);
}

TEST_F(DistillerTest, CancelWithDelayedImageFetchCallback) {
  base::MessageLoopForUI loop;
  vector<int> image_indices;
  image_indices.push_back(0);
  scoped_ptr<base::ListValue> distilled_value =
      CreateDistilledValueReturnedFromJS(kTitle, kContent, image_indices, "");
  EXPECT_CALL(page_factory_, CreateDistillerPageMock())
      .WillOnce(CreateMockDistillerPage(distilled_value.get(), GURL(kURL)));

  TestDistillerURLFetcher* delayed_fetcher = new TestDistillerURLFetcher(true);
  MockDistillerURLFetcherFactory url_fetcher_factory;
  EXPECT_CALL(url_fetcher_factory, CreateDistillerURLFetcher())
      .WillOnce(Return(delayed_fetcher));
  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory));
  distiller_->Init();
  DistillPage(kURL);
  base::MessageLoop::current()->RunUntilIdle();

  // Post callback from the url fetcher and then delete the distiller.
  delayed_fetcher->PostCallbackTask();
  distiller_.reset();

  base::MessageLoop::current()->RunUntilIdle();
}

TEST_F(DistillerTest, CancelWithDelayedJSCallback) {
  base::MessageLoopForUI loop;
  scoped_ptr<base::ListValue> distilled_value =
      CreateDistilledValueReturnedFromJS(kTitle, kContent, vector<int>(), "");
  MockDistillerPage* distiller_page = NULL;
  EXPECT_CALL(page_factory_, CreateDistillerPageMock())
      .WillOnce(CreateMockDistillerPageWithPendingJSCallback(&distiller_page,
                                                             GURL(kURL)));
  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  DistillPage(kURL);
  base::MessageLoop::current()->RunUntilIdle();

  ASSERT_TRUE(distiller_page);
  // Post the task to execute javascript and then delete the distiller.
  distiller_page->OnExecuteJavaScriptDone(GURL(kURL), distilled_value.get());
  distiller_.reset();

  base::MessageLoop::current()->RunUntilIdle();
}

}  // namespace dom_distiller
