// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net_network_service/android_stream_reader_url_loader.h"

#include "android_webview/browser/input_stream.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "content/public/common/resource_type.h"
#include "mojo/core/embedder/embedder.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

// Always succeeds
class FakeInputStream : public InputStream {
 public:
  FakeInputStream() {}
  ~FakeInputStream() override {}
  bool BytesAvailable(int* bytes_available) const override { return true; }
  bool Skip(int64_t n, int64_t* bytes_skipped) override {
    *bytes_skipped = n;
    return true;
  }
  bool Read(net::IOBuffer* dest, int length, int* bytes_read) override {
    return true;
  }
};

// Stream that always fails
class FakeFailingInputStream : public InputStream {
 public:
  FakeFailingInputStream() {}
  ~FakeFailingInputStream() override {}
  bool BytesAvailable(int* bytes_available) const override { return false; }
  bool Skip(int64_t n, int64_t* bytes_skipped) override { return false; }
  bool Read(net::IOBuffer* dest, int length, int* bytes_read) override {
    return false;
  }
};

class TestResponseDelegate
    : public AndroidStreamReaderURLLoader::ResponseDelegate {
 public:
  TestResponseDelegate(std::unique_ptr<InputStream> input_stream)
      : input_stream_(std::move(input_stream)) {}
  ~TestResponseDelegate() override {}

  std::unique_ptr<android_webview::InputStream> OpenInputStream(
      JNIEnv* env) override {
    return std::move(input_stream_);
  }

 private:
  std::unique_ptr<InputStream> input_stream_;
};

class AndroidStreamReaderURLLoaderTest : public ::testing::Test {
 protected:
  AndroidStreamReaderURLLoaderTest() {}
  ~AndroidStreamReaderURLLoaderTest() override = default;

  void SetUp() override {
    mojo::core::Init();
    feature_list_.InitAndEnableFeature(network::features::kNetworkService);
  }

  network::ResourceRequest CreateRequest(const GURL& url) {
    network::ResourceRequest request;
    request.url = url;
    request.method = "GET";
    request.resource_type = content::RESOURCE_TYPE_SUB_RESOURCE;
    return request;
  }

  // helper method for creating loaders given a stream
  AndroidStreamReaderURLLoader* CreateLoader(
      const network::ResourceRequest& request,
      network::TestURLLoaderClient* client,
      std::unique_ptr<InputStream> input_stream) {
    return new AndroidStreamReaderURLLoader(
        request, client->CreateInterfacePtr(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        std::make_unique<TestResponseDelegate>(std::move(input_stream)));
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(AndroidStreamReaderURLLoaderTest);
};

// crbug.com/919929
TEST_F(AndroidStreamReaderURLLoaderTest, DISABLED_ReadFakeStream) {
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/"));
  std::unique_ptr<network::TestURLLoaderClient> client =
      std::make_unique<network::TestURLLoaderClient>();
  AndroidStreamReaderURLLoader* loader =
      CreateLoader(request, client.get(), std::make_unique<FakeInputStream>());
  loader->Start();
  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);
  EXPECT_EQ("HTTP/1.1 200 OK",
            client->response_head().headers->GetStatusLine());
}

TEST_F(AndroidStreamReaderURLLoaderTest, ReadFailingStream) {
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/"));
  std::unique_ptr<network::TestURLLoaderClient> client =
      std::make_unique<network::TestURLLoaderClient>();
  AndroidStreamReaderURLLoader* loader = CreateLoader(
      request, client.get(), std::make_unique<FakeFailingInputStream>());
  loader->Start();
  client->RunUntilComplete();
  EXPECT_EQ(net::ERR_FAILED, client->completion_status().error_code);
}

// crbug.com/919929
TEST_F(AndroidStreamReaderURLLoaderTest, DISABLED_ValidRangeRequest) {
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/"));
  request.headers.SetHeader(net::HttpRequestHeaders::kRange, "bytes=10-200");

  std::unique_ptr<network::TestURLLoaderClient> client =
      std::make_unique<network::TestURLLoaderClient>();
  AndroidStreamReaderURLLoader* loader =
      CreateLoader(request, client.get(), std::make_unique<FakeInputStream>());
  loader->Start();
  client->RunUntilComplete();
  EXPECT_EQ(net::OK, client->completion_status().error_code);
  EXPECT_EQ("HTTP/1.1 200 OK",
            client->response_head().headers->GetStatusLine());
}

TEST_F(AndroidStreamReaderURLLoaderTest, InvalidRangeRequest) {
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/"));
  request.headers.SetHeader(net::HttpRequestHeaders::kRange, "bytes=10-0");

  std::unique_ptr<network::TestURLLoaderClient> client =
      std::make_unique<network::TestURLLoaderClient>();
  AndroidStreamReaderURLLoader* loader =
      CreateLoader(request, client.get(), std::make_unique<FakeInputStream>());
  loader->Start();
  client->RunUntilComplete();
  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
            client->completion_status().error_code);
}

TEST_F(AndroidStreamReaderURLLoaderTest, NullInputStream) {
  network::ResourceRequest request =
      CreateRequest(GURL("https://www.example.com/"));

  std::unique_ptr<network::TestURLLoaderClient> client =
      std::make_unique<network::TestURLLoaderClient>();
  AndroidStreamReaderURLLoader* loader =
      CreateLoader(request, client.get(), nullptr);
  loader->Start();
  client->RunUntilComplete();
  EXPECT_EQ(net::ERR_FAILED, client->completion_status().error_code);
  EXPECT_EQ("HTTP/1.1 404 Not Found",
            client->response_head().headers->GetStatusLine());
}

}  // namespace android_webview
