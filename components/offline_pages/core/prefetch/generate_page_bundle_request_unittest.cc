// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/generate_page_bundle_request.h"

#include "base/test/mock_callback.h"
#include "components/offline_pages/core/prefetch/prefetch_request_test_base.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/prefetch_utils.h"
#include "components/offline_pages/core/prefetch/proto/offline_pages.pb.h"
#include "components/offline_pages/core/prefetch/proto/operation.pb.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::SaveArg;

namespace offline_pages {

namespace {
const char kTestURL[] = "http://example.com";
const char kTestURL2[] = "http://example.com/2";
const char kTestURL3[] = "http://example.com/3";
const char kTestURL4[] = "http://example.com/4";
const char kTestUserAgent[] = "Test User Agent";
const char kTestGCMID[] = "Test GCM ID";
const int kTestMaxBundleSize = 100000;
const char kTestBodyName[] = "body_name";
const int64_t kTestBodyLength = 12345678LL;
}  // namespace

class OperationBuilder {
 public:
  virtual ~OperationBuilder() {}

  // Builds the opereation with an Any value and returns it in binary serialized
  // format.
  virtual std::string BuildFromAny(const std::string& any_type_url,
                                   const std::string& any_value) = 0;

  // Builds the opereation with with an Any value filled with page bundle data
  // and returns it in binary serialized format.
  std::string BuildFromPageBundle(const proto::PageBundle& bundle) {
    std::string bundle_data;
    EXPECT_TRUE(bundle.SerializeToString(&bundle_data));
    return BuildFromAny(kPageBundleTypeURL, bundle_data);
  }

 protected:
  // Helper function to build the operation based on |is_done| that controls
  // where Any value goes.
  std::string BuildOperation(bool is_done,
                             const std::string& any_type_url,
                             const std::string& any_value) {
    proto::Operation operation;
    operation.set_done(is_done);
    proto::Any* any =
        is_done ? operation.mutable_response() : operation.mutable_metadata();
    any->set_type_url(any_type_url);
    any->set_value(any_value);
    std::string data;
    EXPECT_TRUE(operation.SerializeToString(&data));
    return data;
  }
};

class DoneOperationBuilder : public OperationBuilder {
 public:
  ~DoneOperationBuilder() override {}

  std::string BuildFromAny(const std::string& any_type_url,
                           const std::string& any_value) override {
    return BuildOperation(true, any_type_url, any_value);
  }
};

class PendingOperationBuilder : public OperationBuilder {
 public:
  ~PendingOperationBuilder() override {}

  std::string BuildFromAny(const std::string& any_type_url,
                           const std::string& any_value) override {
    return BuildOperation(false, any_type_url, any_value);
  }
};

class GeneratePageBundleRequestTest : public PrefetchRequestTestBase {
 public:
  // Sends GeneratePageBundleRequest for single page with |page_url| and
  // gets back the data in |http_response|.
  PrefetchRequestStatus GeneratePage(const std::string& page_url,
                                     const std::string& http_response);

  // Sends GeneratePageBundleRequest for multiple pages and  gets back the data
  // in |http_response|.
  PrefetchRequestStatus GenerateMultiplePages(
      const std::vector<std::string>& page_urls,
      const std::string& http_response);

  const std::vector<RenderPageInfo>& pages() const { return pages_; }

 private:
  std::vector<RenderPageInfo> pages_;
};

PrefetchRequestStatus GeneratePageBundleRequestTest::GeneratePage(
    const std::string& page_url,
    const std::string& http_response) {
  std::vector<std::string> page_urls;
  page_urls.push_back(page_url);
  return GenerateMultiplePages(page_urls, http_response);
}

PrefetchRequestStatus GeneratePageBundleRequestTest::GenerateMultiplePages(
    const std::vector<std::string>& page_urls,
    const std::string& http_response) {
  base::MockCallback<GeneratePageBundleRequest::FinishedCallback> callback;
  std::unique_ptr<GeneratePageBundleRequest> fetcher(
      new GeneratePageBundleRequest(kTestUserAgent, kTestGCMID,
                                    kTestMaxBundleSize, page_urls,
                                    request_context(), callback.Get()));

  PrefetchRequestStatus status;
  pages_.clear();
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status), SaveArg<1>(&pages_)));
  RespondWithData(http_response);

  return status;
}

TEST_F(GeneratePageBundleRequestTest, FailedToParse) {
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
            GeneratePage(kTestURL, "Some data"));
  EXPECT_TRUE(pages().empty());
}

TEST_F(GeneratePageBundleRequestTest, NoResultInDoneOperation) {
  proto::Operation operation;
  operation.set_done(true);
  std::string data;
  EXPECT_TRUE(operation.SerializeToString(&data));
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
            GeneratePage(kTestURL, data));
  EXPECT_TRUE(pages().empty());
}

TEST_F(GeneratePageBundleRequestTest, ErrorCodeInDoneOperation) {
  proto::Operation operation;
  operation.set_done(true);
  operation.mutable_error()->set_code(1);
  std::string data;
  EXPECT_TRUE(operation.SerializeToString(&data));
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
            GeneratePage(kTestURL, data));
  EXPECT_TRUE(pages().empty());
}

template <typename T>
class GeneratePageBundleRequestOperationTest
    : public GeneratePageBundleRequestTest {
 public:
  std::string BuildFromAny(const std::string& any_type_url,
                           const std::string& any_value) {
    return builder_.BuildFromAny(any_type_url, any_value);
  }

  std::string BuildFromPageBundle(const proto::PageBundle& bundle) {
    return builder_.BuildFromPageBundle(bundle);
  }

 private:
  T builder_;
};

typedef testing::Types<DoneOperationBuilder, PendingOperationBuilder> MyTypes;
TYPED_TEST_CASE(GeneratePageBundleRequestOperationTest, MyTypes);

TYPED_TEST(GeneratePageBundleRequestOperationTest, InvalidTypeUrl) {
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
            this->GeneratePage(kTestURL, this->BuildFromAny("foo", "")));
  EXPECT_TRUE(this->pages().empty());
}

TYPED_TEST(GeneratePageBundleRequestOperationTest, InvalidValue) {
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
            this->GeneratePage(kTestURL,
                               this->BuildFromAny(kPageBundleTypeURL, "foo")));
  EXPECT_TRUE(this->pages().empty());
}

TYPED_TEST(GeneratePageBundleRequestOperationTest, EmptyPageBundle) {
  proto::PageBundle bundle;
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
            this->GeneratePage(kTestURL, this->BuildFromPageBundle(bundle)));
  EXPECT_TRUE(this->pages().empty());
}

TYPED_TEST(GeneratePageBundleRequestOperationTest, EmptyArchive) {
  proto::PageBundle bundle;
  bundle.add_archives();
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
            this->GeneratePage(kTestURL, this->BuildFromPageBundle(bundle)));
  EXPECT_TRUE(this->pages().empty());
}

TYPED_TEST(GeneratePageBundleRequestOperationTest, NoPageInfo) {
  proto::PageBundle bundle;
  proto::Archive* archive = bundle.add_archives();
  archive->set_body_name(kTestBodyName);
  archive->set_body_length(kTestBodyLength);
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
            this->GeneratePage(kTestURL, this->BuildFromPageBundle(bundle)));
  EXPECT_TRUE(this->pages().empty());
}

TYPED_TEST(GeneratePageBundleRequestOperationTest, MissingPageInfoUrl) {
  proto::PageBundle bundle;
  proto::Archive* archive = bundle.add_archives();
  proto::PageInfo* page_info = archive->add_page_infos();
  page_info->set_redirect_url(kTestURL);
  EXPECT_EQ(PrefetchRequestStatus::SHOULD_RETRY_WITH_BACKOFF,
            this->GeneratePage(kTestURL, this->BuildFromPageBundle(bundle)));
  EXPECT_TRUE(this->pages().empty());
}

TYPED_TEST(GeneratePageBundleRequestOperationTest, SinglePage) {
  proto::PageBundle bundle;
  proto::Archive* archive = bundle.add_archives();
  archive->set_body_name(kTestBodyName);
  archive->set_body_length(kTestBodyLength);
  proto::PageInfo* page_info = archive->add_page_infos();
  page_info->set_url(kTestURL);
  page_info->set_redirect_url(kTestURL2);
  page_info->mutable_status()->set_code(proto::OK);
  page_info->set_transformation(proto::NO_TRANSFORMATION);
  int64_t ms_since_epoch = base::Time::Now().ToJavaTime();
  page_info->mutable_render_time()->set_seconds(ms_since_epoch / 1000);
  page_info->mutable_render_time()->set_nanos((ms_since_epoch % 1000) *
                                              1000000);
  EXPECT_EQ(PrefetchRequestStatus::SUCCESS,
            this->GeneratePage(kTestURL, this->BuildFromPageBundle(bundle)));
  ASSERT_EQ(1u, this->pages().size());
  EXPECT_EQ(kTestURL, this->pages().back().url);
  EXPECT_EQ(kTestURL2, this->pages().back().redirect_url);
  EXPECT_EQ(RenderStatus::RENDERED, this->pages().back().status);
  EXPECT_EQ(kTestBodyName, this->pages().back().body_name);
  EXPECT_EQ(kTestBodyLength, this->pages().back().body_length);
  EXPECT_EQ(ms_since_epoch, this->pages().back().render_time.ToJavaTime());
}

TYPED_TEST(GeneratePageBundleRequestOperationTest, MultiplePages) {
  proto::PageBundle bundle;

  // Adds a page that is still being rendered.
  proto::Archive* archive = bundle.add_archives();
  proto::PageInfo* page_info = archive->add_page_infos();
  page_info->set_url(kTestURL);
  page_info->mutable_status()->set_code(proto::NOT_FOUND);

  // Adds a page that failed to render due to bundle size limits.
  archive = bundle.add_archives();
  page_info = archive->add_page_infos();
  page_info->set_url(kTestURL2);
  page_info->mutable_status()->set_code(proto::FAILED_PRECONDITION);

  // Adds a page that failed to render for any other reason.
  archive = bundle.add_archives();
  page_info = archive->add_page_infos();
  page_info->set_url(kTestURL3);
  page_info->mutable_status()->set_code(proto::UNKNOWN);

  // Adds a page that was rendered successfully.
  archive = bundle.add_archives();
  archive->set_body_name(kTestBodyName);
  archive->set_body_length(kTestBodyLength);
  page_info = archive->add_page_infos();
  page_info->set_url(kTestURL4);
  page_info->mutable_status()->set_code(proto::OK);
  page_info->set_transformation(proto::NO_TRANSFORMATION);
  int64_t ms_since_epoch = base::Time::Now().ToJavaTime();
  page_info->mutable_render_time()->set_seconds(ms_since_epoch / 1000);
  page_info->mutable_render_time()->set_nanos((ms_since_epoch % 1000) *
                                              1000000);

  std::vector<std::string> page_urls = {kTestURL, kTestURL2, kTestURL3};
  EXPECT_EQ(PrefetchRequestStatus::SUCCESS,
            this->GenerateMultiplePages(page_urls,
                                        this->BuildFromPageBundle(bundle)));
  ASSERT_EQ(4u, this->pages().size());
  EXPECT_EQ(kTestURL, this->pages().at(0).url);
  EXPECT_EQ(RenderStatus::PENDING, this->pages().at(0).status);
  EXPECT_EQ(kTestURL2, this->pages().at(1).url);
  EXPECT_EQ(RenderStatus::EXCEEDED_LIMIT, this->pages().at(1).status);
  EXPECT_EQ(kTestURL3, this->pages().at(2).url);
  EXPECT_EQ(RenderStatus::FAILED, this->pages().at(2).status);
  EXPECT_EQ(kTestURL4, this->pages().at(3).url);
  EXPECT_EQ(RenderStatus::RENDERED, this->pages().at(3).status);
  EXPECT_EQ(kTestBodyName, this->pages().at(3).body_name);
  EXPECT_EQ(kTestBodyLength, this->pages().at(3).body_length);
  EXPECT_EQ(ms_since_epoch, this->pages().at(3).render_time.ToJavaTime());
}

}  // namespace offline_pages
