// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/string_util.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_download.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "chrome/common/net/test_url_fetcher_factory.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/testing_browser_process_test.h"
#include "chrome/test/testing_profile.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "webkit/glue/form_data.h"

using webkit_glue::FormData;
using WebKit::WebInputElement;

namespace {

class MockAutoFillMetrics : public AutoFillMetrics {
 public:
  MockAutoFillMetrics() {}
  MOCK_CONST_METHOD1(Log, void(ServerQueryMetric metric));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutoFillMetrics);
};

}  // namespace

// This tests AutofillDownloadManager. AutofillDownloadTestHelper implements
// AutofillDownloadManager::Observer and creates an instance of
// AutofillDownloadManager. Then it records responses to different initiated
// requests, which are verified later. To mock network requests
// TestURLFetcherFactory is used, which creates URLFetchers that do not
// go over the wire, but allow calling back HTTP responses directly.
// The responses in test are out of order and verify: successful query request,
// successful upload request, failed upload request.
class AutofillDownloadTestHelper : public AutofillDownloadManager::Observer {
 public:
  AutofillDownloadTestHelper()
      : download_manager(&profile) {
    download_manager.SetObserver(this);
  }
  ~AutofillDownloadTestHelper() {
    download_manager.SetObserver(NULL);
  }

  void LimitCache(size_t cache_size) {
    download_manager.set_max_form_cache_size(cache_size);
  }

  // AutofillDownloadManager::Observer overridables:
  virtual void OnLoadedAutofillHeuristics(
      const std::string& heuristic_xml) {
    ResponseData response;
    response.response = heuristic_xml;
    response.type_of_response = QUERY_SUCCESSFULL;
    responses_.push_back(response);
  };
  virtual void OnUploadedAutofillHeuristics(const std::string& form_signature) {
    ResponseData response;
    response.type_of_response = UPLOAD_SUCCESSFULL;
    responses_.push_back(response);
  }
  virtual void OnHeuristicsRequestError(
      const std::string& form_signature,
      AutofillDownloadManager::AutofillRequestType request_type,
      int http_error) {
    ResponseData response;
    response.signature = form_signature;
    response.error = http_error;
    response.type_of_response =
        request_type == AutofillDownloadManager::REQUEST_QUERY ?
            REQUEST_QUERY_FAILED : REQUEST_UPLOAD_FAILED;
    responses_.push_back(response);
  }

  enum TYPE_OF_RESPONSE {
    QUERY_SUCCESSFULL,
    UPLOAD_SUCCESSFULL,
    REQUEST_QUERY_FAILED,
    REQUEST_UPLOAD_FAILED,
  };

  struct ResponseData {
    TYPE_OF_RESPONSE type_of_response;
    int error;
    std::string signature;
    std::string response;
    ResponseData() : type_of_response(REQUEST_QUERY_FAILED), error(0) {
    }
  };
  std::list<AutofillDownloadTestHelper::ResponseData> responses_;

  TestingProfile profile;
  AutofillDownloadManager download_manager;
};

typedef TestingBrowserProcessTest AutoFillDownloadTest;

TEST_F(AutoFillDownloadTest, QueryAndUploadTest) {
  MessageLoopForUI message_loop;
  // Create and register factory.
  AutofillDownloadTestHelper helper;
  TestURLFetcherFactory factory;
  URLFetcher::set_factory(&factory);

  FormData form;
  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("username"),
                                               ASCIIToUTF16("username"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                                               ASCIIToUTF16("firstname"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                                               ASCIIToUTF16("lastname"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("email"),
                                               ASCIIToUTF16("email"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("email2"),
                                               ASCIIToUTF16("email2"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("password"),
                                               ASCIIToUTF16("password"),
                                               string16(),
                                               ASCIIToUTF16("password"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit"),
                                               0,
                                               false));

  FormStructure *form_structure = new FormStructure(form);
  ScopedVector<FormStructure> form_structures;
  form_structures.push_back(form_structure);

  form.fields.clear();
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("address"),
                                               ASCIIToUTF16("address"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("address2"),
                                               ASCIIToUTF16("address2"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("city"),
                                               ASCIIToUTF16("city"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit"),
                                               0,
                                               false));
  form_structure = new FormStructure(form);
  form_structures.push_back(form_structure);

  // Request with id 0.
  MockAutoFillMetrics mock_metric_logger;
  EXPECT_CALL(mock_metric_logger, Log(AutoFillMetrics::QUERY_SENT)).Times(1);
  EXPECT_TRUE(helper.download_manager.StartQueryRequest(form_structures,
                                                        mock_metric_logger));
  // Set upload to 100% so requests happen.
  helper.download_manager.SetPositiveUploadRate(1.0);
  helper.download_manager.SetNegativeUploadRate(1.0);
  // Request with id 1.
  EXPECT_TRUE(helper.download_manager.StartUploadRequest(*(form_structures[0]),
                                                         true));
  // Request with id 2.
  EXPECT_TRUE(helper.download_manager.StartUploadRequest(*(form_structures[1]),
                                                         false));

  const char *responses[] = {
    "<autofillqueryresponse>"
      "<field autofilltype=\"0\" />"
      "<field autofilltype=\"3\" />"
      "<field autofilltype=\"5\" />"
      "<field autofilltype=\"9\" />"
      "<field autofilltype=\"0\" />"
      "<field autofilltype=\"30\" />"
      "<field autofilltype=\"31\" />"
      "<field autofilltype=\"33\" />"
    "</autofillqueryresponse>",
    "<autofilluploadresponse positiveuploadrate=\"0.5\" "
    "negativeuploadrate=\"0.3\"/>",
    "<html></html>",
  };

  // Return them out of sequence.
  TestURLFetcher* fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(fetcher);
  fetcher->delegate()->OnURLFetchComplete(fetcher, GURL(),
                                          net::URLRequestStatus(),
                                          200, ResponseCookies(),
                                          std::string(responses[1]));
  // After that upload rates would be adjusted to 0.5/0.3
  EXPECT_DOUBLE_EQ(0.5, helper.download_manager.GetPositiveUploadRate());
  EXPECT_DOUBLE_EQ(0.3, helper.download_manager.GetNegativeUploadRate());

  fetcher = factory.GetFetcherByID(2);
  ASSERT_TRUE(fetcher);
  fetcher->delegate()->OnURLFetchComplete(fetcher, GURL(),
                                          net::URLRequestStatus(),
                                          404, ResponseCookies(),
                                          std::string(responses[2]));
  fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->delegate()->OnURLFetchComplete(fetcher, GURL(),
                                          net::URLRequestStatus(),
                                          200, ResponseCookies(),
                                          std::string(responses[0]));
  EXPECT_EQ(static_cast<size_t>(3), helper.responses_.size());

  EXPECT_EQ(AutofillDownloadTestHelper::UPLOAD_SUCCESSFULL,
            helper.responses_.front().type_of_response);
  EXPECT_EQ(0, helper.responses_.front().error);
  EXPECT_EQ(std::string(), helper.responses_.front().signature);
  // Expected response on non-query request is an empty string.
  EXPECT_EQ(std::string(), helper.responses_.front().response);
  helper.responses_.pop_front();

  EXPECT_EQ(AutofillDownloadTestHelper::REQUEST_UPLOAD_FAILED,
            helper.responses_.front().type_of_response);
  EXPECT_EQ(404, helper.responses_.front().error);
  EXPECT_EQ(form_structures[1]->FormSignature(),
            helper.responses_.front().signature);
  // Expected response on non-query request is an empty string.
  EXPECT_EQ(std::string(), helper.responses_.front().response);
  helper.responses_.pop_front();

  EXPECT_EQ(helper.responses_.front().type_of_response,
            AutofillDownloadTestHelper::QUERY_SUCCESSFULL);
  EXPECT_EQ(0, helper.responses_.front().error);
  EXPECT_EQ(std::string(), helper.responses_.front().signature);
  EXPECT_EQ(responses[0], helper.responses_.front().response);
  helper.responses_.pop_front();

  // Set upload to 0% so no new requests happen.
  helper.download_manager.SetPositiveUploadRate(0.0);
  helper.download_manager.SetNegativeUploadRate(0.0);
  // No actual requests for the next two calls, as we set upload rate to 0%.
  EXPECT_FALSE(helper.download_manager.StartUploadRequest(*(form_structures[0]),
                                                         true));
  EXPECT_FALSE(helper.download_manager.StartUploadRequest(*(form_structures[1]),
                                                         false));
  fetcher = factory.GetFetcherByID(3);
  EXPECT_EQ(NULL, fetcher);

  // Modify form structures to miss the cache.
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Address line 2"),
                                               ASCIIToUTF16("address2"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form_structure = new FormStructure(form);
  form_structures.push_back(form_structure);

  // Request with id 3.
  EXPECT_CALL(mock_metric_logger, Log(AutoFillMetrics::QUERY_SENT)).Times(1);
  EXPECT_TRUE(helper.download_manager.StartQueryRequest(form_structures,
                                                        mock_metric_logger));
  fetcher = factory.GetFetcherByID(3);
  ASSERT_TRUE(fetcher);
  fetcher->set_backoff_delay(
      base::TimeDelta::FromMilliseconds(TestTimeouts::action_max_timeout_ms()));
  fetcher->delegate()->OnURLFetchComplete(fetcher, GURL(),
                                          net::URLRequestStatus(),
                                          500, ResponseCookies(),
                                          std::string(responses[0]));
  EXPECT_EQ(AutofillDownloadTestHelper::REQUEST_QUERY_FAILED,
            helper.responses_.front().type_of_response);
  EXPECT_EQ(500, helper.responses_.front().error);
  // Expected response on non-query request is an empty string.
  EXPECT_EQ(std::string(), helper.responses_.front().response);
  helper.responses_.pop_front();

  // Query requests should be ignored for the next 10 seconds.
  EXPECT_CALL(mock_metric_logger, Log(AutoFillMetrics::QUERY_SENT)).Times(0);
  EXPECT_FALSE(helper.download_manager.StartQueryRequest(form_structures,
                                                         mock_metric_logger));
  fetcher = factory.GetFetcherByID(4);
  EXPECT_EQ(NULL, fetcher);

  // Set upload to 100% so requests happen.
  helper.download_manager.SetPositiveUploadRate(1.0);
  // Request with id 4.
  EXPECT_TRUE(helper.download_manager.StartUploadRequest(*(form_structures[0]),
              true));
  fetcher = factory.GetFetcherByID(4);
  ASSERT_TRUE(fetcher);
  fetcher->set_backoff_delay(
      base::TimeDelta::FromMilliseconds(TestTimeouts::action_max_timeout_ms()));
  fetcher->delegate()->OnURLFetchComplete(fetcher, GURL(),
                                          net::URLRequestStatus(),
                                          503, ResponseCookies(),
                                          std::string(responses[2]));
  EXPECT_EQ(AutofillDownloadTestHelper::REQUEST_UPLOAD_FAILED,
            helper.responses_.front().type_of_response);
  EXPECT_EQ(503, helper.responses_.front().error);
  helper.responses_.pop_front();

  // Upload requests should be ignored for the next 10 seconds.
  EXPECT_FALSE(helper.download_manager.StartUploadRequest(*(form_structures[0]),
              true));
  fetcher = factory.GetFetcherByID(5);
  EXPECT_EQ(NULL, fetcher);

  // Make sure consumer of URLFetcher does the right thing.
  URLFetcher::set_factory(NULL);
}

TEST_F(AutoFillDownloadTest, CacheQueryTest) {
  MessageLoopForUI message_loop;
  AutofillDownloadTestHelper helper;
  // Create and register factory.
  TestURLFetcherFactory factory;
  URLFetcher::set_factory(&factory);

  FormData form;
  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("username"),
                                               ASCIIToUTF16("username"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                                               ASCIIToUTF16("firstname"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                                               ASCIIToUTF16("lastname"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  FormStructure *form_structure = new FormStructure(form);
  ScopedVector<FormStructure> form_structures0;
  form_structures0.push_back(form_structure);

  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("email"),
                                               ASCIIToUTF16("email"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  // Slightly different form - so different request.
  form_structure = new FormStructure(form);
  ScopedVector<FormStructure> form_structures1;
  form_structures1.push_back(form_structure);

  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("email2"),
                                               ASCIIToUTF16("email2"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  // Slightly different form - so different request.
  form_structure = new FormStructure(form);
  ScopedVector<FormStructure> form_structures2;
  form_structures2.push_back(form_structure);

  // Limit cache to two forms.
  helper.LimitCache(2);

  const char *responses[] = {
    "<autofillqueryresponse>"
      "<field autofilltype=\"0\" />"
      "<field autofilltype=\"3\" />"
      "<field autofilltype=\"5\" />"
    "</autofillqueryresponse>",
    "<autofillqueryresponse>"
      "<field autofilltype=\"0\" />"
      "<field autofilltype=\"3\" />"
      "<field autofilltype=\"5\" />"
      "<field autofilltype=\"9\" />"
    "</autofillqueryresponse>",
    "<autofillqueryresponse>"
      "<field autofilltype=\"0\" />"
      "<field autofilltype=\"3\" />"
      "<field autofilltype=\"5\" />"
      "<field autofilltype=\"9\" />"
      "<field autofilltype=\"0\" />"
    "</autofillqueryresponse>",
  };

  // Request with id 0.
  MockAutoFillMetrics mock_metric_logger;
  EXPECT_CALL(mock_metric_logger, Log(AutoFillMetrics::QUERY_SENT)).Times(1);
  EXPECT_TRUE(helper.download_manager.StartQueryRequest(form_structures0,
                                                        mock_metric_logger));
  // No responses yet
  EXPECT_EQ(static_cast<size_t>(0), helper.responses_.size());

  TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->delegate()->OnURLFetchComplete(fetcher, GURL(),
                                          net::URLRequestStatus(),
                                          200, ResponseCookies(),
                                          std::string(responses[0]));
  ASSERT_EQ(static_cast<size_t>(1), helper.responses_.size());
  EXPECT_EQ(responses[0], helper.responses_.front().response);

  helper.responses_.clear();

  // No actual request - should be a cache hit.
  EXPECT_CALL(mock_metric_logger, Log(AutoFillMetrics::QUERY_SENT)).Times(1);
  EXPECT_TRUE(helper.download_manager.StartQueryRequest(form_structures0,
                                                        mock_metric_logger));
  // Data is available immediately from cache - no over-the-wire trip.
  ASSERT_EQ(static_cast<size_t>(1), helper.responses_.size());
  EXPECT_EQ(responses[0], helper.responses_.front().response);
  helper.responses_.clear();

  // Request with id 1.
  EXPECT_CALL(mock_metric_logger, Log(AutoFillMetrics::QUERY_SENT)).Times(1);
  EXPECT_TRUE(helper.download_manager.StartQueryRequest(form_structures1,
                                                        mock_metric_logger));
  // No responses yet
  EXPECT_EQ(static_cast<size_t>(0), helper.responses_.size());

  fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(fetcher);
  fetcher->delegate()->OnURLFetchComplete(fetcher, GURL(),
                                          net::URLRequestStatus(),
                                          200, ResponseCookies(),
                                          std::string(responses[1]));
  ASSERT_EQ(static_cast<size_t>(1), helper.responses_.size());
  EXPECT_EQ(responses[1], helper.responses_.front().response);

  helper.responses_.clear();

  // Request with id 2.
  EXPECT_CALL(mock_metric_logger, Log(AutoFillMetrics::QUERY_SENT)).Times(1);
  EXPECT_TRUE(helper.download_manager.StartQueryRequest(form_structures2,
                                                        mock_metric_logger));

  fetcher = factory.GetFetcherByID(2);
  ASSERT_TRUE(fetcher);
  fetcher->delegate()->OnURLFetchComplete(fetcher, GURL(),
                                          net::URLRequestStatus(),
                                          200, ResponseCookies(),
                                          std::string(responses[2]));
  ASSERT_EQ(static_cast<size_t>(1), helper.responses_.size());
  EXPECT_EQ(responses[2], helper.responses_.front().response);

  helper.responses_.clear();

  // No actual requests - should be a cache hit.
  EXPECT_CALL(mock_metric_logger, Log(AutoFillMetrics::QUERY_SENT)).Times(1);
  EXPECT_TRUE(helper.download_manager.StartQueryRequest(form_structures1,
                                                        mock_metric_logger));

  EXPECT_CALL(mock_metric_logger, Log(AutoFillMetrics::QUERY_SENT)).Times(1);
  EXPECT_TRUE(helper.download_manager.StartQueryRequest(form_structures2,
                                                        mock_metric_logger));

  ASSERT_EQ(static_cast<size_t>(2), helper.responses_.size());
  EXPECT_EQ(responses[1], helper.responses_.front().response);
  EXPECT_EQ(responses[2], helper.responses_.back().response);
  helper.responses_.clear();

  // The first structure should've expired.
  // Request with id 3.
  EXPECT_CALL(mock_metric_logger, Log(AutoFillMetrics::QUERY_SENT)).Times(1);
  EXPECT_TRUE(helper.download_manager.StartQueryRequest(form_structures0,
                                                        mock_metric_logger));
  // No responses yet
  EXPECT_EQ(static_cast<size_t>(0), helper.responses_.size());

  fetcher = factory.GetFetcherByID(3);
  ASSERT_TRUE(fetcher);
  fetcher->delegate()->OnURLFetchComplete(fetcher, GURL(),
                                          net::URLRequestStatus(),
                                          200, ResponseCookies(),
                                          std::string(responses[0]));
  ASSERT_EQ(static_cast<size_t>(1), helper.responses_.size());
  EXPECT_EQ(responses[0], helper.responses_.front().response);

  // Make sure consumer of URLFetcher does the right thing.
  URLFetcher::set_factory(NULL);
}

