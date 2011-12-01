// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_download.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/test/base/test_url_request_context_getter.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "content/test/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "webkit/glue/form_data.h"

using content::BrowserThread;
using webkit_glue::FormData;
using webkit_glue::FormField;
using WebKit::WebInputElement;

namespace {

class MockAutofillMetrics : public AutofillMetrics {
 public:
  MockAutofillMetrics() {}
  MOCK_CONST_METHOD1(LogServerQueryMetric, void(ServerQueryMetric metric));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillMetrics);
};

// Call |fetcher->OnURLFetchComplete()| as the URLFetcher would when
// a response is received.  Params allow caller to set fake status.
void FakeOnURLFetchComplete(TestURLFetcher* fetcher,
                            int response_code,
                            const std::string& response_body) {
  fetcher->set_url(GURL());
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(response_code);
  fetcher->SetResponseString(response_body);

  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

}  // namespace

// This tests AutofillDownloadManager. AutofillDownloadTest implements
// AutofillDownloadManager::Observer and creates an instance of
// AutofillDownloadManager. Then it records responses to different initiated
// requests, which are verified later. To mock network requests
// TestURLFetcherFactory is used, which creates URLFetchers that do not
// go over the wire, but allow calling back HTTP responses directly.
// The responses in test are out of order and verify: successful query request,
// successful upload request, failed upload request.
class AutofillDownloadTest : public AutofillDownloadManager::Observer,
                             public testing::Test {
 public:
  AutofillDownloadTest()
      : download_manager_(&profile_, this),
        io_thread_(BrowserThread::IO) {
  }

  virtual void SetUp() {
    io_thread_.StartIOThread();
    profile_.CreateRequestContext();
  }

  virtual void TearDown() {
    profile_.ResetRequestContext();
    io_thread_.Stop();
  }

  void LimitCache(size_t cache_size) {
    download_manager_.set_max_form_cache_size(cache_size);
  }

  // AutofillDownloadManager::Observer implementation.
  virtual void OnLoadedServerPredictions(
      const std::string& response_xml) OVERRIDE {
    ResponseData response;
    response.response = response_xml;
    response.type_of_response = QUERY_SUCCESSFULL;
    responses_.push_back(response);
  }

  virtual void OnUploadedPossibleFieldTypes() OVERRIDE {
    ResponseData response;
    response.type_of_response = UPLOAD_SUCCESSFULL;
    responses_.push_back(response);
  }

  virtual void OnServerRequestError(
      const std::string& form_signature,
      AutofillDownloadManager::AutofillRequestType request_type,
      int http_error) OVERRIDE {
    ResponseData response;
    response.signature = form_signature;
    response.error = http_error;
    response.type_of_response =
        request_type == AutofillDownloadManager::REQUEST_QUERY ?
            REQUEST_QUERY_FAILED : REQUEST_UPLOAD_FAILED;
    responses_.push_back(response);
  }

  enum ResponseType {
    QUERY_SUCCESSFULL,
    UPLOAD_SUCCESSFULL,
    REQUEST_QUERY_FAILED,
    REQUEST_UPLOAD_FAILED,
  };

  struct ResponseData {
    ResponseType type_of_response;
    int error;
    std::string signature;
    std::string response;

    ResponseData() : type_of_response(REQUEST_QUERY_FAILED), error(0) {}
  };
  std::list<ResponseData> responses_;

  TestingProfile profile_;
  AutofillDownloadManager download_manager_;

 private:
  // The profile's request context must be released on the IO thread.
  content::TestBrowserThread io_thread_;
};

TEST_F(AutofillDownloadTest, QueryAndUploadTest) {
  MessageLoopForUI message_loop;
  // Create and register factory.
  TestURLFetcherFactory factory;

  FormData form;
  form.method = ASCIIToUTF16("post");

  FormField field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = ASCIIToUTF16("text");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  field.form_control_type = ASCIIToUTF16("text");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.form_control_type = ASCIIToUTF16("text");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = ASCIIToUTF16("text");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("email2");
  field.name = ASCIIToUTF16("email2");
  field.form_control_type = ASCIIToUTF16("text");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = ASCIIToUTF16("password");
  form.fields.push_back(field);

  field.label = string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = ASCIIToUTF16("submit");
  form.fields.push_back(field);

  FormStructure *form_structure = new FormStructure(form);
  ScopedVector<FormStructure> form_structures;
  form_structures.push_back(form_structure);

  form.fields.clear();

  field.label = ASCIIToUTF16("address");
  field.name = ASCIIToUTF16("address");
  field.form_control_type = ASCIIToUTF16("text");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("address2");
  field.name = ASCIIToUTF16("address2");
  field.form_control_type = ASCIIToUTF16("text");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("city");
  field.name = ASCIIToUTF16("city");
  field.form_control_type = ASCIIToUTF16("text");
  form.fields.push_back(field);

  field.label = string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = ASCIIToUTF16("submit");
  form.fields.push_back(field);

  form_structure = new FormStructure(form);
  form_structures.push_back(form_structure);

  // Request with id 0.
  MockAutofillMetrics mock_metric_logger;
  EXPECT_CALL(mock_metric_logger,
              LogServerQueryMetric(AutofillMetrics::QUERY_SENT)).Times(1);
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures.get(),
                                                  mock_metric_logger));
  // Set upload to 100% so requests happen.
  download_manager_.SetPositiveUploadRate(1.0);
  download_manager_.SetNegativeUploadRate(1.0);
  // Request with id 1.
  EXPECT_TRUE(download_manager_.StartUploadRequest(
      *(form_structures[0]), true, FieldTypeSet()));
  // Request with id 2.
  EXPECT_TRUE(download_manager_.StartUploadRequest(
      *(form_structures[1]), false, FieldTypeSet()));

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
  FakeOnURLFetchComplete(fetcher, 200, std::string(responses[1]));

  // After that upload rates would be adjusted to 0.5/0.3
  EXPECT_DOUBLE_EQ(0.5, download_manager_.GetPositiveUploadRate());
  EXPECT_DOUBLE_EQ(0.3, download_manager_.GetNegativeUploadRate());

  fetcher = factory.GetFetcherByID(2);
  ASSERT_TRUE(fetcher);
  FakeOnURLFetchComplete(fetcher, 404, std::string(responses[2]));

  fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  FakeOnURLFetchComplete(fetcher, 200, std::string(responses[0]));
  EXPECT_EQ(static_cast<size_t>(3), responses_.size());

  EXPECT_EQ(AutofillDownloadTest::UPLOAD_SUCCESSFULL,
            responses_.front().type_of_response);
  EXPECT_EQ(0, responses_.front().error);
  EXPECT_EQ(std::string(), responses_.front().signature);
  // Expected response on non-query request is an empty string.
  EXPECT_EQ(std::string(), responses_.front().response);
  responses_.pop_front();

  EXPECT_EQ(AutofillDownloadTest::REQUEST_UPLOAD_FAILED,
            responses_.front().type_of_response);
  EXPECT_EQ(404, responses_.front().error);
  EXPECT_EQ(form_structures[1]->FormSignature(),
            responses_.front().signature);
  // Expected response on non-query request is an empty string.
  EXPECT_EQ(std::string(), responses_.front().response);
  responses_.pop_front();

  EXPECT_EQ(responses_.front().type_of_response,
            AutofillDownloadTest::QUERY_SUCCESSFULL);
  EXPECT_EQ(0, responses_.front().error);
  EXPECT_EQ(std::string(), responses_.front().signature);
  EXPECT_EQ(responses[0], responses_.front().response);
  responses_.pop_front();

  // Set upload to 0% so no new requests happen.
  download_manager_.SetPositiveUploadRate(0.0);
  download_manager_.SetNegativeUploadRate(0.0);
  // No actual requests for the next two calls, as we set upload rate to 0%.
  EXPECT_FALSE(download_manager_.StartUploadRequest(
      *(form_structures[0]), true, FieldTypeSet()));
  EXPECT_FALSE(download_manager_.StartUploadRequest(
      *(form_structures[1]), false, FieldTypeSet()));
  fetcher = factory.GetFetcherByID(3);
  EXPECT_EQ(NULL, fetcher);

  // Modify form structures to miss the cache.
  field.label = ASCIIToUTF16("Address line 2");
  field.name = ASCIIToUTF16("address2");
  field.form_control_type = ASCIIToUTF16("text");
  form.fields.push_back(field);
  form_structure = new FormStructure(form);
  form_structures.push_back(form_structure);

  // Request with id 3.
  EXPECT_CALL(mock_metric_logger,
              LogServerQueryMetric(AutofillMetrics::QUERY_SENT)).Times(1);
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures.get(),
                                                  mock_metric_logger));
  fetcher = factory.GetFetcherByID(3);
  ASSERT_TRUE(fetcher);
  fetcher->set_backoff_delay(
      base::TimeDelta::FromMilliseconds(TestTimeouts::action_max_timeout_ms()));
  FakeOnURLFetchComplete(fetcher, 500, std::string(responses[0]));

  EXPECT_EQ(AutofillDownloadTest::REQUEST_QUERY_FAILED,
            responses_.front().type_of_response);
  EXPECT_EQ(500, responses_.front().error);
  // Expected response on non-query request is an empty string.
  EXPECT_EQ(std::string(), responses_.front().response);
  responses_.pop_front();

  // Query requests should be ignored for the next 10 seconds.
  EXPECT_CALL(mock_metric_logger,
              LogServerQueryMetric(AutofillMetrics::QUERY_SENT)).Times(0);
  EXPECT_FALSE(download_manager_.StartQueryRequest(form_structures.get(),
                                                   mock_metric_logger));
  fetcher = factory.GetFetcherByID(4);
  EXPECT_EQ(NULL, fetcher);

  // Set upload required to true so requests happen.
  form_structures[0]->upload_required_ = UPLOAD_REQUIRED;
  // Request with id 4.
  EXPECT_TRUE(download_manager_.StartUploadRequest(
      *(form_structures[0]), true, FieldTypeSet()));
  fetcher = factory.GetFetcherByID(4);
  ASSERT_TRUE(fetcher);
  fetcher->set_backoff_delay(
      base::TimeDelta::FromMilliseconds(TestTimeouts::action_max_timeout_ms()));
  FakeOnURLFetchComplete(fetcher, 503, std::string(responses[2]));
  EXPECT_EQ(AutofillDownloadTest::REQUEST_UPLOAD_FAILED,
            responses_.front().type_of_response);
  EXPECT_EQ(503, responses_.front().error);
  responses_.pop_front();

  // Upload requests should be ignored for the next 10 seconds.
  EXPECT_FALSE(download_manager_.StartUploadRequest(
      *(form_structures[0]), true, FieldTypeSet()));
  fetcher = factory.GetFetcherByID(5);
  EXPECT_EQ(NULL, fetcher);
}

TEST_F(AutofillDownloadTest, CacheQueryTest) {
  MessageLoopForUI message_loop;
  // Create and register factory.
  TestURLFetcherFactory factory;

  FormData form;
  form.method = ASCIIToUTF16("post");

  FormField field;
  field.form_control_type = ASCIIToUTF16("text");

  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  form.fields.push_back(field);

  FormStructure *form_structure = new FormStructure(form);
  ScopedVector<FormStructure> form_structures0;
  form_structures0.push_back(form_structure);

  // Add a slightly different form, which should result in a different request.
  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);
  form_structure = new FormStructure(form);
  ScopedVector<FormStructure> form_structures1;
  form_structures1.push_back(form_structure);

  // Add another slightly different form, which should also result in a
  // different request.
  field.label = ASCIIToUTF16("email2");
  field.name = ASCIIToUTF16("email2");
  form.fields.push_back(field);
  form_structure = new FormStructure(form);
  ScopedVector<FormStructure> form_structures2;
  form_structures2.push_back(form_structure);

  // Limit cache to two forms.
  LimitCache(2);

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
  MockAutofillMetrics mock_metric_logger;
  EXPECT_CALL(mock_metric_logger,
              LogServerQueryMetric(AutofillMetrics::QUERY_SENT)).Times(1);
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures0.get(),
                                                  mock_metric_logger));
  // No responses yet
  EXPECT_EQ(static_cast<size_t>(0), responses_.size());

  TestURLFetcher* fetcher = factory.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  FakeOnURLFetchComplete(fetcher, 200, std::string(responses[0]));
  ASSERT_EQ(static_cast<size_t>(1), responses_.size());
  EXPECT_EQ(responses[0], responses_.front().response);

  responses_.clear();

  // No actual request - should be a cache hit.
  EXPECT_CALL(mock_metric_logger,
              LogServerQueryMetric(AutofillMetrics::QUERY_SENT)).Times(1);
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures0.get(),
                                                  mock_metric_logger));
  // Data is available immediately from cache - no over-the-wire trip.
  ASSERT_EQ(static_cast<size_t>(1), responses_.size());
  EXPECT_EQ(responses[0], responses_.front().response);
  responses_.clear();

  // Request with id 1.
  EXPECT_CALL(mock_metric_logger,
              LogServerQueryMetric(AutofillMetrics::QUERY_SENT)).Times(1);
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures1.get(),
                                                  mock_metric_logger));
  // No responses yet
  EXPECT_EQ(static_cast<size_t>(0), responses_.size());

  fetcher = factory.GetFetcherByID(1);
  ASSERT_TRUE(fetcher);
  FakeOnURLFetchComplete(fetcher, 200, std::string(responses[1]));
  ASSERT_EQ(static_cast<size_t>(1), responses_.size());
  EXPECT_EQ(responses[1], responses_.front().response);

  responses_.clear();

  // Request with id 2.
  EXPECT_CALL(mock_metric_logger,
              LogServerQueryMetric(AutofillMetrics::QUERY_SENT)).Times(1);
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures2.get(),
                                                  mock_metric_logger));

  fetcher = factory.GetFetcherByID(2);
  ASSERT_TRUE(fetcher);
  FakeOnURLFetchComplete(fetcher, 200, std::string(responses[2]));
  ASSERT_EQ(static_cast<size_t>(1), responses_.size());
  EXPECT_EQ(responses[2], responses_.front().response);

  responses_.clear();

  // No actual requests - should be a cache hit.
  EXPECT_CALL(mock_metric_logger,
              LogServerQueryMetric(AutofillMetrics::QUERY_SENT)).Times(1);
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures1.get(),
                                                  mock_metric_logger));

  EXPECT_CALL(mock_metric_logger,
              LogServerQueryMetric(AutofillMetrics::QUERY_SENT)).Times(1);
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures2.get(),
                                                  mock_metric_logger));

  ASSERT_EQ(static_cast<size_t>(2), responses_.size());
  EXPECT_EQ(responses[1], responses_.front().response);
  EXPECT_EQ(responses[2], responses_.back().response);
  responses_.clear();

  // The first structure should've expired.
  // Request with id 3.
  EXPECT_CALL(mock_metric_logger,
              LogServerQueryMetric(AutofillMetrics::QUERY_SENT)).Times(1);
  EXPECT_TRUE(download_manager_.StartQueryRequest(form_structures0.get(),
                                                  mock_metric_logger));
  // No responses yet
  EXPECT_EQ(static_cast<size_t>(0), responses_.size());

  fetcher = factory.GetFetcherByID(3);
  ASSERT_TRUE(fetcher);
  FakeOnURLFetchComplete(fetcher, 200, std::string(responses[0]));
  ASSERT_EQ(static_cast<size_t>(1), responses_.size());
  EXPECT_EQ(responses[0], responses_.front().response);
}
