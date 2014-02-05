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
      const string& next_page_url) {
    scoped_ptr<base::ListValue> list(new base::ListValue());

    list->AppendString(title);
    list->AppendString(content);
    list->AppendString(next_page_url);
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

ACTION_P3(CreateMockDistillerPages, lists, kurls, num_pages) {
  DistillerPage::Delegate* delegate = arg0;
  MockDistillerPage* distiller_page = new MockDistillerPage(delegate);
  EXPECT_CALL(*distiller_page, InitImpl());
  {
    testing::InSequence s;

    for (int page = 0; page < num_pages; ++page) {
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
      .WillOnce(CreateMockDistillerPages(list, page_urls, kNumPages));

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

}  // namespace dom_distiller
