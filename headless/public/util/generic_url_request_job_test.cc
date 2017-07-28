// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/generic_url_request_job.h"

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "headless/public/util/expedited_dispatcher.h"
#include "headless/public/util/testing/generic_url_request_mocks.h"
#include "headless/public/util/url_fetcher.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

std::ostream& operator<<(std::ostream& os, const base::DictionaryValue& value) {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  os << json;
  return os;
}

MATCHER_P(MatchesJson, json, json.c_str()) {
  std::unique_ptr<base::Value> expected(
      base::JSONReader::Read(json, base::JSON_PARSE_RFC));
  return arg.Equals(expected.get());
}

namespace headless {

namespace {

class MockDelegate : public MockGenericURLRequestJobDelegate {
 public:
  MOCK_METHOD2(OnResourceLoadFailed,
               void(const Request* request, net::Error error));
};

class MockFetcher : public URLFetcher {
 public:
  MockFetcher(base::DictionaryValue* fetch_request,
              std::map<std::string, std::string>* json_fetch_reply_map)
      : json_fetch_reply_map_(json_fetch_reply_map),
        fetch_request_(fetch_request) {}

  ~MockFetcher() override {}

  void StartFetch(const GURL& url,
                  const std::string& method,
                  const std::string& post_data,
                  const net::HttpRequestHeaders& request_headers,
                  ResultListener* result_listener) override {
    // Record the request.
    fetch_request_->SetString("url", url.spec());
    fetch_request_->SetString("method", method);
    std::unique_ptr<base::DictionaryValue> headers(new base::DictionaryValue);
    for (net::HttpRequestHeaders::Iterator it(request_headers); it.GetNext();) {
      headers->SetString(it.name(), it.value());
    }
    fetch_request_->Set("headers", std::move(headers));
    if (!post_data.empty())
      fetch_request_->SetString("post_data", post_data);

    const auto find_it = json_fetch_reply_map_->find(url.spec());
    if (find_it == json_fetch_reply_map_->end()) {
      result_listener->OnFetchStartError(net::ERR_ADDRESS_UNREACHABLE);
      return;
    }

    // Return the canned response.
    std::unique_ptr<base::Value> fetch_reply(
        base::JSONReader::Read(find_it->second, base::JSON_PARSE_RFC));
    CHECK(fetch_reply) << "Invalid json: " << find_it->second;

    base::DictionaryValue* reply_dictionary;
    ASSERT_TRUE(fetch_reply->GetAsDictionary(&reply_dictionary));
    std::string final_url;
    ASSERT_TRUE(reply_dictionary->GetString("url", &final_url));
    ASSERT_TRUE(reply_dictionary->GetString("data", &response_data_));
    base::DictionaryValue* reply_headers_dictionary;
    ASSERT_TRUE(
        reply_dictionary->GetDictionary("headers", &reply_headers_dictionary));
    scoped_refptr<net::HttpResponseHeaders> response_headers(
        new net::HttpResponseHeaders(""));
    for (base::DictionaryValue::Iterator it(*reply_headers_dictionary);
         !it.IsAtEnd(); it.Advance()) {
      std::string value;
      ASSERT_TRUE(it.value().GetAsString(&value));
      response_headers->AddHeader(
          base::StringPrintf("%s: %s", it.key().c_str(), value.c_str()));
    }

    result_listener->OnFetchComplete(
        GURL(final_url), std::move(response_headers), response_data_.c_str(),
        response_data_.size());
  }

 private:
  std::map<std::string, std::string>* json_fetch_reply_map_;  // NOT OWNED
  base::DictionaryValue* fetch_request_;  // NOT OWNED
  std::string response_data_;  // Here to ensure the required lifetime.
};

class MockProtocolHandler : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  // Details of the fetch will be stored in |fetch_request|.
  // The fetch response will be created from parsing |json_fetch_reply_map|.
  MockProtocolHandler(base::DictionaryValue* fetch_request,
                      std::map<std::string, std::string>* json_fetch_reply_map,
                      URLRequestDispatcher* dispatcher,
                      GenericURLRequestJob::Delegate* job_delegate)
      : fetch_request_(fetch_request),
        json_fetch_reply_map_(json_fetch_reply_map),
        job_delegate_(job_delegate),
        dispatcher_(dispatcher) {}

  // net::URLRequestJobFactory::ProtocolHandler override.
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new GenericURLRequestJob(
        request, network_delegate, dispatcher_,
        base::MakeUnique<MockFetcher>(fetch_request_, json_fetch_reply_map_),
        job_delegate_, nullptr);
  }

 private:
  base::DictionaryValue* fetch_request_;          // NOT OWNED
  std::map<std::string, std::string>* json_fetch_reply_map_;  // NOT OWNED
  GenericURLRequestJob::Delegate* job_delegate_;  // NOT OWNED
  URLRequestDispatcher* dispatcher_;              // NOT OWNED
};

}  // namespace

class GenericURLRequestJobTest : public testing::Test {
 public:
  GenericURLRequestJobTest() : dispatcher_(message_loop_.task_runner()) {
    url_request_job_factory_.SetProtocolHandler(
        "https", base::WrapUnique(new MockProtocolHandler(
                     &fetch_request_, &json_fetch_reply_map_, &dispatcher_,
                     &job_delegate_)));
    url_request_context_.set_job_factory(&url_request_job_factory_);
    url_request_context_.set_cookie_store(&cookie_store_);
  }

  std::unique_ptr<net::URLRequest> CreateAndCompleteGetJob(
      const GURL& url,
      const std::string& json_reply) {
    json_fetch_reply_map_[url.spec()] = json_reply;

    std::unique_ptr<net::URLRequest> request(url_request_context_.CreateRequest(
        url, net::DEFAULT_PRIORITY, &request_delegate_,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    request->Start();
    base::RunLoop().RunUntilIdle();
    return request;
  }

  std::unique_ptr<net::URLRequest> CreateAndCompletePostJob(
      const GURL& url,
      const std::string& post_data,
      const std::string& json_reply) {
    json_fetch_reply_map_[url.spec()] = json_reply;

    std::unique_ptr<net::URLRequest> request(url_request_context_.CreateRequest(
        url, net::DEFAULT_PRIORITY, &request_delegate_,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    request->set_method("POST");
    request->set_upload(net::ElementsUploadDataStream::CreateWithReader(
        base::MakeUnique<net::UploadBytesElementReader>(post_data.data(),
                                                        post_data.size()),
        0));
    request->Start();
    base::RunLoop().RunUntilIdle();
    return request;
  }

 protected:
  base::MessageLoop message_loop_;
  ExpeditedDispatcher dispatcher_;

  net::URLRequestJobFactoryImpl url_request_job_factory_;
  net::URLRequestContext url_request_context_;
  MockCookieStore cookie_store_;

  MockURLRequestDelegate request_delegate_;
  base::DictionaryValue fetch_request_;  // The request sent to MockFetcher.
  std::map<std::string, std::string>
      json_fetch_reply_map_;  // Replies to be sent by MockFetcher.
  MockDelegate job_delegate_;
};

TEST_F(GenericURLRequestJobTest, BasicGetRequestParams) {
  net::StaticHttpUserAgentSettings user_agent_settings("en-UK", "TestBrowser");

  json_fetch_reply_map_["https://example.com/"] = R"(
      {
        "url": "https://example.com",
        "data": "Reply",
        "headers": {
          "Content-Type": "text/html; charset=UTF-8"
        }
      })";

  url_request_context_.set_http_user_agent_settings(&user_agent_settings);
  std::unique_ptr<net::URLRequest> request(url_request_context_.CreateRequest(
      GURL("https://example.com"), net::DEFAULT_PRIORITY, &request_delegate_,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  request->SetReferrer("https://referrer.example.com");
  request->SetExtraRequestHeaderByName("Extra-Header", "Value", true);
  request->Start();
  base::RunLoop().RunUntilIdle();

  std::string expected_request_json = R"(
      {
        "url": "https://example.com/",
        "method": "GET",
        "headers": {
          "Accept-Language": "en-UK",
          "Extra-Header": "Value",
          "Referer": "https://referrer.example.com/",
          "User-Agent": "TestBrowser"
        }
      })";

  EXPECT_THAT(fetch_request_, MatchesJson(expected_request_json));
}

TEST_F(GenericURLRequestJobTest, BasicPostRequestParams) {
  json_fetch_reply_map_["https://example.com/"] = R"(
      {
        "url": "https://example.com",
        "data": "Reply",
        "headers": {
          "Content-Type": "text/html; charset=UTF-8"
        }
      })";

  std::unique_ptr<net::URLRequest> request(url_request_context_.CreateRequest(
      GURL("https://example.com"), net::DEFAULT_PRIORITY, &request_delegate_,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  request->SetReferrer("https://referrer.example.com");
  request->SetExtraRequestHeaderByName("Extra-Header", "Value", true);
  request->SetExtraRequestHeaderByName("User-Agent", "TestBrowser", true);
  request->SetExtraRequestHeaderByName("Accept", "text/plain", true);
  request->set_method("POST");

  std::string post_data = "lorem ipsom";
  request->set_upload(net::ElementsUploadDataStream::CreateWithReader(
      base::MakeUnique<net::UploadBytesElementReader>(post_data.data(),
                                                      post_data.size()),
      0));
  request->Start();
  base::RunLoop().RunUntilIdle();

  std::string expected_request_json = R"(
      {
        "url": "https://example.com/",
        "method": "POST",
        "post_data": "lorem ipsom",
        "headers": {
          "Accept": "text/plain",
          "Extra-Header": "Value",
          "Referer": "https://referrer.example.com/",
          "User-Agent": "TestBrowser"
        }
      })";

  EXPECT_THAT(fetch_request_, MatchesJson(expected_request_json));
}

TEST_F(GenericURLRequestJobTest, BasicRequestProperties) {
  std::string reply = R"(
      {
        "url": "https://example.com",
        "data": "Reply",
        "headers": {
          "Content-Type": "text/html; charset=UTF-8"
        }
      })";

  std::unique_ptr<net::URLRequest> request(
      CreateAndCompleteGetJob(GURL("https://example.com"), reply));

  EXPECT_EQ(200, request->GetResponseCode());

  std::string mime_type;
  request->GetMimeType(&mime_type);
  EXPECT_EQ("text/html", mime_type);

  std::string charset;
  request->GetCharset(&charset);
  EXPECT_EQ("utf-8", charset);

  std::string content_type;
  EXPECT_TRUE(request->response_info().headers->GetNormalizedHeader(
      "Content-Type", &content_type));
  EXPECT_EQ("text/html; charset=UTF-8", content_type);
}

TEST_F(GenericURLRequestJobTest, BasicRequestContents) {
  std::string reply = R"(
      {
        "url": "https://example.com",
        "data": "Reply",
        "headers": {
          "Content-Type": "text/html; charset=UTF-8"
        }
      })";

  std::unique_ptr<net::URLRequest> request(
      CreateAndCompleteGetJob(GURL("https://example.com"), reply));

  const int kBufferSize = 256;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));
  int bytes_read;
  EXPECT_TRUE(request->Read(buffer.get(), kBufferSize, &bytes_read));
  EXPECT_EQ(5, bytes_read);
  EXPECT_EQ("Reply", std::string(buffer->data(), 5));

  net::LoadTimingInfo load_timing_info;
  request->GetLoadTimingInfo(&load_timing_info);
  EXPECT_FALSE(load_timing_info.receive_headers_end.is_null());
}

TEST_F(GenericURLRequestJobTest, ReadInParts) {
  std::string reply = R"(
      {
        "url": "https://example.com",
        "data": "Reply",
        "headers": {
          "Content-Type": "text/html; charset=UTF-8"
        }
      })";

  std::unique_ptr<net::URLRequest> request(
      CreateAndCompleteGetJob(GURL("https://example.com"), reply));

  const int kBufferSize = 3;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));
  int bytes_read;
  EXPECT_TRUE(request->Read(buffer.get(), kBufferSize, &bytes_read));
  EXPECT_EQ(3, bytes_read);
  EXPECT_EQ("Rep", std::string(buffer->data(), bytes_read));

  EXPECT_TRUE(request->Read(buffer.get(), kBufferSize, &bytes_read));
  EXPECT_EQ(2, bytes_read);
  EXPECT_EQ("ly", std::string(buffer->data(), bytes_read));

  EXPECT_TRUE(request->Read(buffer.get(), kBufferSize, &bytes_read));
  EXPECT_EQ(0, bytes_read);
}

TEST_F(GenericURLRequestJobTest, RequestWithCookies) {
  net::CookieList* cookies = cookie_store_.cookies();

  // Basic matching cookie.
  cookies->push_back(net::CanonicalCookie(
      "basic_cookie", "1", ".example.com", "/", base::Time(), base::Time(),
      base::Time(),
      /* secure */ false,
      /* http_only */ false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_DEFAULT));

  // Matching secure cookie.
  cookies->push_back(net::CanonicalCookie(
      "secure_cookie", "2", ".example.com", "/", base::Time(), base::Time(),
      base::Time(),
      /* secure */ true,
      /* http_only */ false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_DEFAULT));

  // Matching http-only cookie.
  cookies->push_back(net::CanonicalCookie(
      "http_only_cookie", "3", ".example.com", "/", base::Time(), base::Time(),
      base::Time(),
      /* secure */ false,
      /* http_only */ true, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_DEFAULT));

  // Matching cookie with path.
  cookies->push_back(net::CanonicalCookie(
      "cookie_with_path", "4", ".example.com", "/widgets", base::Time(),
      base::Time(), base::Time(),
      /* secure */ false,
      /* http_only */ false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_DEFAULT));

  // Matching cookie with subdomain.
  cookies->push_back(net::CanonicalCookie(
      "bad_subdomain_cookie", "5", ".cdn.example.com", "/", base::Time(),
      base::Time(), base::Time(),
      /* secure */ false,
      /* http_only */ false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_DEFAULT));

  // Non-matching cookie (different site).
  cookies->push_back(net::CanonicalCookie(
      "bad_site_cookie", "6", ".zombo.com", "/", base::Time(), base::Time(),
      base::Time(),
      /* secure */ false,
      /* http_only */ false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_DEFAULT));

  // Non-matching cookie (different path).
  cookies->push_back(net::CanonicalCookie(
      "bad_path_cookie", "7", ".example.com", "/gadgets", base::Time(),
      base::Time(), base::Time(),
      /* secure */ false,
      /* http_only */ false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_DEFAULT));

  std::string reply = R"(
      {
        "url": "https://example.com",
        "data": "Reply",
        "headers": {
          "Content-Type": "text/html; charset=UTF-8"
        }
      })";

  std::unique_ptr<net::URLRequest> request(
      CreateAndCompleteGetJob(GURL("https://example.com"), reply));

  std::string expected_request_json = R"(
      {
        "url": "https://example.com/",
        "method": "GET",
        "headers": {
          "Cookie": "basic_cookie=1; secure_cookie=2; http_only_cookie=3"
        }
      })";

  EXPECT_THAT(fetch_request_, MatchesJson(expected_request_json));
}

TEST_F(GenericURLRequestJobTest, DelegateBlocksLoading) {
  std::string reply = R"(
      {
        "url": "https://example.com",
        "data": "Reply",
        "headers": {
          "Content-Type": "text/html; charset=UTF-8"
        }
      })";

  job_delegate_.SetPolicy(base::Bind([](PendingRequest* pending_request) {
    pending_request->BlockRequest(net::ERR_FILE_NOT_FOUND);
  }));

  std::unique_ptr<net::URLRequest> request(
      CreateAndCompleteGetJob(GURL("https://example.com"), reply));

  EXPECT_EQ(net::URLRequestStatus::FAILED, request->status().status());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, request->status().error());
}

TEST_F(GenericURLRequestJobTest, DelegateModifiesRequest) {
  json_fetch_reply_map_["https://example.com/"] = R"(
      {
        "url": "https://example.com",
        "data": "Welcome to example.com",
        "headers": {
          "Content-Type": "text/html; charset=UTF-8"
        }
      })";

  json_fetch_reply_map_["https://othersite.com/"] = R"(
      {
        "url": "https://example.com",
        "data": "Welcome to othersite.com",
        "headers": {
          "Content-Type": "text/html; charset=UTF-8"
        }
      })";

  // Turn the GET into a POST to a different site.
  job_delegate_.SetPolicy(base::Bind([](PendingRequest* pending_request) {
    net::HttpRequestHeaders headers;
    headers.SetHeader("TestHeader", "Hello");
    pending_request->ModifyRequest(GURL("https://othersite.com"), "POST",
                                   "Some post data!", headers);
  }));

  std::unique_ptr<net::URLRequest> request(url_request_context_.CreateRequest(
      GURL("https://example.com"), net::DEFAULT_PRIORITY, &request_delegate_,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  base::RunLoop().RunUntilIdle();

  std::string expected_request_json = R"(
      {
        "url": "https://othersite.com/",
        "method": "POST",
        "post_data": "Some post data!",
        "headers": {
          "TestHeader": "Hello"
        }
      })";

  EXPECT_THAT(fetch_request_, MatchesJson(expected_request_json));

  EXPECT_EQ(200, request->GetResponseCode());
  // The modification should not be visible to the URlRequest.
  EXPECT_EQ("https://example.com/", request->url().spec());
  EXPECT_EQ("GET", request->method());

  const int kBufferSize = 256;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));
  int bytes_read;
  EXPECT_TRUE(request->Read(buffer.get(), kBufferSize, &bytes_read));
  EXPECT_EQ(24, bytes_read);
  EXPECT_EQ("Welcome to othersite.com",
            std::string(buffer->data(), bytes_read));
}

TEST_F(GenericURLRequestJobTest, DelegateMocks404Response) {
  std::string reply = R"(
      {
        "url": "https://example.com",
        "data": "Reply",
        "headers": {
          "Content-Type": "text/html; charset=UTF-8"
        }
      })";

  job_delegate_.SetPolicy(base::Bind([](PendingRequest* pending_request) {
    std::unique_ptr<GenericURLRequestJob::MockResponseData> mock_response_data(
        new GenericURLRequestJob::MockResponseData());
    mock_response_data->response_data = "HTTP/1.1 404 Not Found\r\n\r\n";
    pending_request->MockResponse(std::move(mock_response_data));
  }));

  std::unique_ptr<net::URLRequest> request(
      CreateAndCompleteGetJob(GURL("https://example.com"), reply));

  EXPECT_EQ(404, request->GetResponseCode());
}

TEST_F(GenericURLRequestJobTest, DelegateMocks302Response) {
  job_delegate_.SetPolicy(base::Bind([](PendingRequest* pending_request) {
    if (pending_request->GetRequest()->GetURLRequest()->url().spec() ==
        "https://example.com/") {
      std::unique_ptr<GenericURLRequestJob::MockResponseData>
          mock_response_data(new GenericURLRequestJob::MockResponseData());
      mock_response_data->response_data =
          "HTTP/1.1 302 Found\r\n"
          "Location: https://foo.com/\r\n\r\n";
      pending_request->MockResponse(std::move(mock_response_data));
    } else {
      pending_request->AllowRequest();
    }
  }));

  json_fetch_reply_map_["https://example.com/"] = R"(
      {
        "url": "https://example.com",
        "data": "Welcome to example.com",
        "headers": {
          "Content-Type": "text/html; charset=UTF-8"
        }
      })";

  json_fetch_reply_map_["https://foo.com/"] = R"(
      {
        "url": "https://example.com",
        "data": "Welcome to foo.com",
        "headers": {
          "Content-Type": "text/html; charset=UTF-8"
        }
      })";

  std::unique_ptr<net::URLRequest> request(url_request_context_.CreateRequest(
      GURL("https://example.com"), net::DEFAULT_PRIORITY, &request_delegate_,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(200, request->GetResponseCode());
  EXPECT_EQ("https://foo.com/", request->url().spec());

  const int kBufferSize = 256;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));
  int bytes_read;
  EXPECT_TRUE(request->Read(buffer.get(), kBufferSize, &bytes_read));
  EXPECT_EQ(18, bytes_read);
  EXPECT_EQ("Welcome to foo.com", std::string(buffer->data(), bytes_read));
}

TEST_F(GenericURLRequestJobTest, OnResourceLoadFailed) {
  EXPECT_CALL(job_delegate_,
              OnResourceLoadFailed(_, net::ERR_ADDRESS_UNREACHABLE));

  std::unique_ptr<net::URLRequest> request(url_request_context_.CreateRequest(
      GURL("https://i-dont-exist.com"), net::DEFAULT_PRIORITY,
      &request_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS));
  request->Start();
  base::RunLoop().RunUntilIdle();
}

TEST_F(GenericURLRequestJobTest, RequestsHaveDistinctIds) {
  std::string reply = R"(
      {
        "url": "https://example.com",
        "http_response_code": 200,
        "data": "Reply",
        "headers": {
          "Content-Type": "text/html; charset=UTF-8"
        }
      })";

  std::set<uint64_t> ids;
  job_delegate_.SetPolicy(base::Bind(
      [](std::set<uint64_t>* ids, PendingRequest* pending_request) {
        ids->insert(pending_request->GetRequest()->GetRequestId());
        pending_request->AllowRequest();
      },
      &ids));

  CreateAndCompleteGetJob(GURL("https://example.com"), reply);
  CreateAndCompleteGetJob(GURL("https://example.com"), reply);
  CreateAndCompleteGetJob(GURL("https://example.com"), reply);

  // We expect three distinct ids.
  EXPECT_EQ(3u, ids.size());
}

TEST_F(GenericURLRequestJobTest, GetPostData) {
  std::string reply = R"(
      {
        "url": "https://example.com",
        "http_response_code": 200,
        "data": "Reply",
        "headers": {
          "Content-Type": "text/html; charset=UTF-8"
        }
      })";

  std::string post_data;
  job_delegate_.SetPolicy(base::Bind(
      [](std::string* post_data, PendingRequest* pending_request) {
        *post_data = pending_request->GetRequest()->GetPostData();
        pending_request->AllowRequest();
      },
      &post_data));

  CreateAndCompletePostJob(GURL("https://example.com"), "payload", reply);

  EXPECT_EQ("payload", post_data);
}

}  // namespace headless
