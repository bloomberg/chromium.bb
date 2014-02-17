// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "components/dom_distiller/core/distiller.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::vector;
using std::string;
using::testing::Invoke;
using::testing::Return;
using::testing::_;

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

}  // namespace

namespace dom_distiller {

class TestDistillerURLFetcher : public DistillerURLFetcher {
 public:
  TestDistillerURLFetcher() : DistillerURLFetcher(NULL) {
    responses_[kImageURLs[0]] = string(kImageData[0]);
    responses_[kImageURLs[1]] = string(kImageData[1]);
  }

  void CallCallback(string url, const URLFetcherCallback& callback) {
    callback.Run(responses_[url]);
  }

  virtual void FetchURL(const string& url,
                        const URLFetcherCallback& callback) OVERRIDE {
    ASSERT_TRUE(base::MessageLoop::current());
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&TestDistillerURLFetcher::CallCallback,
                   base::Unretained(this), url, callback));
  }

  std::map<string, string> responses_;
};


class TestDistillerURLFetcherFactory : public DistillerURLFetcherFactory {
 public:
  TestDistillerURLFetcherFactory() : DistillerURLFetcherFactory(NULL) {}
  virtual ~TestDistillerURLFetcherFactory() {}
  virtual DistillerURLFetcher* CreateDistillerURLFetcher() const OVERRIDE {
    return new TestDistillerURLFetcher();
  }
};


class MockDistillerPage : public DistillerPage {
 public:
  MOCK_METHOD0(InitImpl, void());
  MOCK_METHOD1(LoadURLImpl, void(const GURL& gurl));
  MOCK_METHOD1(ExecuteJavaScriptImpl, void(const string& script));

  explicit MockDistillerPage(DistillerPage::Delegate* delegate)
      : DistillerPage(delegate) {}
};


class MockDistillerPageFactory : public DistillerPageFactory {
 public:
  MOCK_CONST_METHOD1(
      CreateDistillerPageMock,
      DistillerPage*(DistillerPage::Delegate* delegate));

  virtual scoped_ptr<DistillerPage> CreateDistillerPage(
      DistillerPage::Delegate* delegate) const OVERRIDE {
    return scoped_ptr<DistillerPage>(CreateDistillerPageMock(delegate));
  }
};

class DistillerTest : public testing::Test {
 public:
  virtual ~DistillerTest() {}
  void OnDistillPageDone(scoped_ptr<DistilledArticleProto> proto) {
    article_proto_ = proto.Pass();
  }

 protected:
  scoped_ptr<DistillerImpl> distiller_;
  scoped_ptr<DistilledArticleProto> article_proto_;
  MockDistillerPageFactory page_factory_;
  TestDistillerURLFetcherFactory url_fetcher_factory_;
};

ACTION_P3(DistillerPageOnExecuteJavaScriptDone, distiller_page, url, list) {
  distiller_page->OnExecuteJavaScriptDone(url, list);
}

ACTION_P2(CreateMockDistillerPage, list, kurl) {
  DistillerPage::Delegate* delegate = arg0;
  MockDistillerPage* distiller_page = new MockDistillerPage(delegate);
  EXPECT_CALL(*distiller_page, InitImpl());
  EXPECT_CALL(*distiller_page, LoadURLImpl(kurl))
      .WillOnce(testing::InvokeWithoutArgs(distiller_page,
                                           &DistillerPage::OnLoadURLDone));
  EXPECT_CALL(*distiller_page, ExecuteJavaScriptImpl(_)).WillOnce(
      DistillerPageOnExecuteJavaScriptDone(distiller_page, kurl, list));
  return distiller_page;
}

ACTION_P4(CreateMockDistillerPages, lists, kurls, num_pages, start_page_num) {
  DistillerPage::Delegate* delegate = arg0;
  MockDistillerPage* distiller_page = new MockDistillerPage(delegate);
  EXPECT_CALL(*distiller_page, InitImpl());
  {
    testing::InSequence s;
    // Distiller prefers distilling past pages first. E.g. when distillation
    // starts on page 2 then pages are distilled in the order: 2, 1, 0, 3, 4.
    vector<int> page_nums;
    for (int page = start_page_num; page >= 0; --page)
      page_nums.push_back(page);
    for (int page = start_page_num + 1; page < num_pages; ++page)
      page_nums.push_back(page);

    for (size_t page_num = 0; page_num < page_nums.size(); ++page_num) {
      int page = page_nums[page_num];
      GURL url = GURL(kurls[page]);
      EXPECT_CALL(*distiller_page, LoadURLImpl(url))
          .WillOnce(testing::InvokeWithoutArgs(distiller_page,
                                               &DistillerPage::OnLoadURLDone));
      EXPECT_CALL(*distiller_page, ExecuteJavaScriptImpl(_))
          .WillOnce(DistillerPageOnExecuteJavaScriptDone(
              distiller_page, url, lists[page].get()));
    }
  }
  return distiller_page;
}

TEST_F(DistillerTest, DistillPage) {
  base::MessageLoopForUI loop;
  scoped_ptr<base::ListValue> list =
      CreateDistilledValueReturnedFromJS(kTitle, kContent, vector<int>(), "");
  EXPECT_CALL(page_factory_, CreateDistillerPageMock(_))
      .WillOnce(CreateMockDistillerPage(list.get(), GURL(kURL)));
  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  distiller_->DistillPage(
      GURL(kURL),
      base::Bind(&DistillerTest::OnDistillPageDone, base::Unretained(this)));
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
  EXPECT_CALL(page_factory_,
              CreateDistillerPageMock(_)).WillOnce(
                  CreateMockDistillerPage(list.get(), GURL(kURL)));
  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  distiller_->DistillPage(
      GURL(kURL),
      base::Bind(&DistillerTest::OnDistillPageDone, base::Unretained(this)));
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
  const int kNumPages = 8;
  vector<int> image_indices[kNumPages];
  string content[kNumPages];
  string page_urls[kNumPages];
  scoped_ptr<base::ListValue> list[kNumPages];

  int next_image_number = 0;

  for (int page_num = 0; page_num < kNumPages; ++page_num) {
    // Each page has different number of images.
    int tot_images = (page_num + kTotalImages) % (kTotalImages + 1);
    for (int img_num = 0; img_num < tot_images; img_num++) {
      image_indices[page_num].push_back(next_image_number);
      next_image_number = (next_image_number + 1) % kTotalImages;
    }

    page_urls[page_num] = "http://a.com/" + base::IntToString(page_num);
    content[page_num] = "Content for page:" + base::IntToString(page_num);
  }
  for (int i = 0; i < kNumPages; ++i) {
    string next_page_url = "";
    if (i + 1 < kNumPages)
      next_page_url = page_urls[i + 1];

    list[i] = CreateDistilledValueReturnedFromJS(
        kTitle, content[i], image_indices[i], next_page_url);
  }

  EXPECT_CALL(page_factory_, CreateDistillerPageMock(_))
      .WillOnce(CreateMockDistillerPages(list, page_urls, kNumPages, 0));

  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  distiller_->DistillPage(
      GURL(page_urls[0]),
      base::Bind(&DistillerTest::OnDistillPageDone, base::Unretained(this)));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  EXPECT_EQ(article_proto_->pages_size(), kNumPages);
  for (int page_num = 0; page_num < kNumPages; ++page_num) {
    const DistilledPageProto& page = article_proto_->pages(page_num);
    EXPECT_EQ(content[page_num], page.html());
    EXPECT_EQ(page_urls[page_num], page.url());
    EXPECT_EQ(image_indices[page_num].size(),
              static_cast<size_t>(page.image_size()));
    for (size_t img_num = 0; img_num < image_indices[page_num].size();
         ++img_num) {
      EXPECT_EQ(kImageData[image_indices[page_num][img_num]],
                page.image(img_num).data());
      EXPECT_EQ(GetImageName(page_num + 1, img_num),
                page.image(img_num).name());
    }
  }
}

TEST_F(DistillerTest, DistillLinkLoop) {
  base::MessageLoopForUI loop;
  // Create a loop, the next page is same as the current page. This could
  // happen if javascript misparses a next page link.
  scoped_ptr<base::ListValue> list =
      CreateDistilledValueReturnedFromJS(kTitle, kContent, vector<int>(), kURL);
  EXPECT_CALL(page_factory_, CreateDistillerPageMock(_))
      .WillOnce(CreateMockDistillerPage(list.get(), GURL(kURL)));
  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  distiller_->DistillPage(
      GURL(kURL),
      base::Bind(&DistillerTest::OnDistillPageDone, base::Unretained(this)));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  EXPECT_EQ(article_proto_->pages_size(), 1);
}

TEST_F(DistillerTest, CheckMaxPageLimit) {
  base::MessageLoopForUI loop;
  const size_t kMaxPagesInArticle = 10;
  string page_urls[kMaxPagesInArticle];
  scoped_ptr<base::ListValue> list[kMaxPagesInArticle];

  // Note: Next page url of the last page of article is set. So distiller will
  // try to do kMaxPagesInArticle + 1 calls if the max article limit does not
  // work.
  string url_prefix = "http://a.com/";
  for (size_t page_num = 0; page_num < kMaxPagesInArticle; ++page_num) {
    page_urls[page_num] = url_prefix + base::IntToString(page_num + 1);
    string content = "Content for page:" + base::IntToString(page_num);
    string next_page_url = url_prefix + base::IntToString(page_num + 2);
    list[page_num] = CreateDistilledValueReturnedFromJS(
        kTitle, content, vector<int>(), next_page_url);
  }

  EXPECT_CALL(page_factory_, CreateDistillerPageMock(_))
      .WillOnce(CreateMockDistillerPages(
          list, page_urls, static_cast<int>(kMaxPagesInArticle), 0));

  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));

  distiller_->SetMaxNumPagesInArticle(kMaxPagesInArticle);

  distiller_->Init();
  distiller_->DistillPage(
      GURL(page_urls[0]),
      base::Bind(&DistillerTest::OnDistillPageDone, base::Unretained(this)));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  EXPECT_EQ(kMaxPagesInArticle,
            static_cast<size_t>(article_proto_->pages_size()));

  // Now check if distilling an article with exactly the page limit works by
  // resetting the next page url of the last page of the article.
  list[kMaxPagesInArticle - 1] =
      CreateDistilledValueReturnedFromJS(kTitle, "Content", vector<int>(), "");
  EXPECT_CALL(page_factory_, CreateDistillerPageMock(_))
      .WillOnce(CreateMockDistillerPages(
          list, page_urls, static_cast<int>(kMaxPagesInArticle), 0));

  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->SetMaxNumPagesInArticle(kMaxPagesInArticle);

  distiller_->Init();
  distiller_->DistillPage(
      GURL(page_urls[0]),
      base::Bind(&DistillerTest::OnDistillPageDone, base::Unretained(this)));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  EXPECT_EQ(kMaxPagesInArticle,
            static_cast<size_t>(article_proto_->pages_size()));

  // Now check if distilling an article with exactly the page limit works by
  // resetting the next page url of the last page of the article.
  list[kMaxPagesInArticle - 1] =
      CreateDistilledValueReturnedFromJS(kTitle, "Content", vector<int>(), "");
  EXPECT_CALL(page_factory_, CreateDistillerPageMock(_))
      .WillOnce(CreateMockDistillerPages(
          list, page_urls, static_cast<int>(kMaxPagesInArticle), 0));

  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->SetMaxNumPagesInArticle(kMaxPagesInArticle);

  distiller_->Init();
  distiller_->DistillPage(
      GURL(page_urls[0]),
      base::Bind(&DistillerTest::OnDistillPageDone, base::Unretained(this)));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  EXPECT_EQ(kMaxPagesInArticle,
            static_cast<size_t>(article_proto_->pages_size()));
}

TEST_F(DistillerTest, SinglePageDistillationFailure) {
  base::MessageLoopForUI loop;
  // To simulate failure return a null value.
  scoped_ptr<base::Value> nullValue(base::Value::CreateNullValue());
  EXPECT_CALL(page_factory_, CreateDistillerPageMock(_))
      .WillOnce(CreateMockDistillerPage(nullValue.get(), GURL(kURL)));
  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  distiller_->DistillPage(
      GURL(kURL),
      base::Bind(&DistillerTest::OnDistillPageDone, base::Unretained(this)));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ("", article_proto_->title());
  EXPECT_EQ(0, article_proto_->pages_size());
}

TEST_F(DistillerTest, MultiplePagesDistillationFailure) {
  base::MessageLoopForUI loop;
  const int kNumPages = 8;
  string content[kNumPages];
  string page_urls[kNumPages];
  scoped_ptr<base::Value> distilled_values[kNumPages];
  // The page number of the failed page.
  int failed_page_num = 3;
  string url_prefix = "http://a.com/";
  for (int page_num = 0; page_num < kNumPages; ++page_num) {
    page_urls[page_num] = url_prefix + base::IntToString(page_num);
    content[page_num] = "Content for page:" + base::IntToString(page_num);
    string next_page_url = url_prefix + base::IntToString(page_num + 1);
    if (page_num != failed_page_num) {
      distilled_values[page_num] = CreateDistilledValueReturnedFromJS(
          kTitle, content[page_num], vector<int>(), next_page_url);
    } else {
      distilled_values[page_num].reset(base::Value::CreateNullValue());
    }
  }

  // Expect only calls till the failed page number.
  EXPECT_CALL(page_factory_, CreateDistillerPageMock(_))
      .WillOnce(CreateMockDistillerPages(
          distilled_values, page_urls, failed_page_num + 1, 0));

  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  distiller_->DistillPage(
      GURL(page_urls[0]),
      base::Bind(&DistillerTest::OnDistillPageDone, base::Unretained(this)));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  EXPECT_EQ(article_proto_->pages_size(), failed_page_num);
  for (int page_num = 0; page_num < failed_page_num; ++page_num) {
    const DistilledPageProto& page = article_proto_->pages(page_num);
    EXPECT_EQ(content[page_num], page.html());
    EXPECT_EQ(page_urls[page_num], page.url());
  }
}

TEST_F(DistillerTest, DistillPreviousPage) {
  base::MessageLoopForUI loop;
  const int kNumPages = 8;
  string content[kNumPages];
  string page_urls[kNumPages];
  scoped_ptr<base::Value> distilled_values[kNumPages];

  // The page number of the article on which distillation starts.
  int start_page_number = 3;
  string url_prefix = "http://a.com/";
  for (int page_no = 0; page_no < kNumPages; ++page_no) {
    page_urls[page_no] = url_prefix + base::IntToString(page_no);
    content[page_no] = "Content for page:" + base::IntToString(page_no);
    string next_page_url = (page_no + 1 < kNumPages)
                               ? url_prefix + base::IntToString(page_no + 1)
                               : "";
    string prev_page_url = (page_no > 0) ? page_urls[page_no - 1] : "";
    distilled_values[page_no] = CreateDistilledValueReturnedFromJS(
        kTitle, content[page_no], vector<int>(), next_page_url, prev_page_url);
  }

  EXPECT_CALL(page_factory_, CreateDistillerPageMock(_))
      .WillOnce(CreateMockDistillerPages(
          distilled_values, page_urls, kNumPages, start_page_number));

  distiller_.reset(new DistillerImpl(page_factory_, url_fetcher_factory_));
  distiller_->Init();
  distiller_->DistillPage(
      GURL(page_urls[start_page_number]),
      base::Bind(&DistillerTest::OnDistillPageDone, base::Unretained(this)));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(kTitle, article_proto_->title());
  EXPECT_EQ(kNumPages, article_proto_->pages_size());
  for (int page_no = 0; page_no < kNumPages; ++page_no) {
    const DistilledPageProto& page = article_proto_->pages(page_no);
    EXPECT_EQ(content[page_no], page.html());
    EXPECT_EQ(page_urls[page_no], page.url());
  }
}

}  // namespace dom_distiller
