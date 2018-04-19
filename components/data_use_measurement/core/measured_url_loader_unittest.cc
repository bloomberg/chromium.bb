// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/measured_url_loader.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/test/histogram_tester.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;
using ::testing::Return;
using ::testing::_;

namespace data_use_measurement {

class MockSimpleURLLoader : public network::SimpleURLLoader {
 public:
  MockSimpleURLLoader() = default;
  ~MockSimpleURLLoader() = default;

  // GMock doesn't support move only params, such as OnceCallbacks. Use a
  // pointer to mock those methods.
  void DownloadToString(
      network::mojom::URLLoaderFactory* url_loader_factory,
      network::SimpleURLLoader::BodyAsStringCallback body_as_string_callback,
      size_t max_body_size) override {
    DownloadToString(url_loader_factory, &body_as_string_callback,
                     max_body_size);
  }
  void DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      network::mojom::URLLoaderFactory* url_loader_factory,
      network::SimpleURLLoader::BodyAsStringCallback body_as_string_callback)
      override {
    DownloadToStringOfUnboundedSizeUntilCrashAndDie(url_loader_factory,
                                                    &body_as_string_callback);
  }

  void DownloadToFile(network::mojom::URLLoaderFactory* url_loader_factory,
                      network::SimpleURLLoader::DownloadToFileCompleteCallback
                          download_to_file_complete_callback,
                      const base::FilePath& file_path,
                      int64_t max_body_size) override {
    DownloadToFile(url_loader_factory, &download_to_file_complete_callback,
                   file_path, max_body_size);
  }

  void DownloadToTempFile(
      network::mojom::URLLoaderFactory* url_loader_factory,
      network::SimpleURLLoader::DownloadToFileCompleteCallback
          download_to_file_complete_callback,
      int64_t max_body_size) override {
    DownloadToTempFile(url_loader_factory, &download_to_file_complete_callback,
                       max_body_size);
  }

  void SetOnResponseStartedCallback(
      network::SimpleURLLoader::OnResponseStartedCallback
          on_response_started_callback) override {
    SetOnResponseStartedCallback(&on_response_started_callback);
  }

  MOCK_METHOD3(DownloadToString,
               void(network::mojom::URLLoaderFactory*,
                    network::SimpleURLLoader::BodyAsStringCallback*,
                    size_t));
  MOCK_METHOD2(DownloadToStringOfUnboundedSizeUntilCrashAndDie,
               void(network::mojom::URLLoaderFactory*,
                    network::SimpleURLLoader::BodyAsStringCallback*));
  MOCK_METHOD4(DownloadToFile,
               void(network::mojom::URLLoaderFactory*,
                    network::SimpleURLLoader::DownloadToFileCompleteCallback*,
                    const base::FilePath&,
                    int64_t));
  MOCK_METHOD3(DownloadToTempFile,
               void(network::mojom::URLLoaderFactory*,
                    network::SimpleURLLoader::DownloadToFileCompleteCallback*,
                    int64_t));
  MOCK_METHOD2(DownloadAsStream,
               void(network::mojom::URLLoaderFactory*,
                    network::SimpleURLLoaderStreamConsumer*));
  MOCK_METHOD1(SetOnRedirectCallback,
               void(const network::SimpleURLLoader::OnRedirectCallback&));
  MOCK_METHOD1(SetOnResponseStartedCallback,
               void(network::SimpleURLLoader::OnResponseStartedCallback*));
  MOCK_METHOD1(SetAllowPartialResults, void(bool));
  MOCK_METHOD1(SetAllowHttpErrorResults, void(bool));
  MOCK_METHOD2(AttachStringForUpload,
               void(const std::string&, const std::string&));
  MOCK_METHOD2(AttachFileForUpload,
               void(const base::FilePath&, const std::string&));
  MOCK_METHOD2(SetRetryOptions, void(int, int));
  MOCK_CONST_METHOD0(NetError, int());
  MOCK_CONST_METHOD0(ResponseInfo, const network::ResourceResponseHead*());
  MOCK_CONST_METHOD0(GetFinalURL, const GURL&());
};

// A wrapper class to allow access to protected constructor.
class MeasuredURLLoaderWrapper : public MeasuredURLLoader {
 public:
  MeasuredURLLoaderWrapper(
      std::unique_ptr<network::SimpleURLLoader> default_loader,
      ServiceName service_name)
      : MeasuredURLLoader(std::move(default_loader), service_name) {}
  ~MeasuredURLLoaderWrapper() override {}
};

class MeasuredURLLoaderTest : public testing::Test {
 public:
  MeasuredURLLoaderTest() {}
  ~MeasuredURLLoaderTest() override {}

  void SetUp() override {
    std::unique_ptr<MockSimpleURLLoader> mock_loader =
        std::make_unique<MockSimpleURLLoader>();
    mock_simple_url_loader_ = mock_loader.get();
    service_url_loader_ = std::make_unique<MeasuredURLLoaderWrapper>(
        std::move(mock_loader),
        MeasuredURLLoader::ServiceName::kDataReductionProxy);
  }

  MeasuredURLLoader* service_url_loader() { return service_url_loader_.get(); }

  MockSimpleURLLoader& mock_simple_url_loader() {
    return *mock_simple_url_loader_;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
  MockSimpleURLLoader* mock_simple_url_loader_;
  std::unique_ptr<MeasuredURLLoader> service_url_loader_;
};

TEST_F(MeasuredURLLoaderTest, CheckHistogram) {
  histogram_tester().ExpectUniqueSample(
      "MeasuredURLLoader.MeasuredServiceType",
      MeasuredURLLoader::ServiceName::kDataReductionProxy, 1);
}

TEST_F(MeasuredURLLoaderTest, DownloadToStringPassThrough) {
  EXPECT_CALL(mock_simple_url_loader(), DownloadToString(_, _, _));
  service_url_loader()->DownloadToString(
      nullptr, network::SimpleURLLoader::BodyAsStringCallback(), 0);
}

TEST_F(MeasuredURLLoaderTest,
       DownloadToStringOfUnboundedSizeUntilCrashAndDiePassThrough) {
  EXPECT_CALL(mock_simple_url_loader(),
              DownloadToStringOfUnboundedSizeUntilCrashAndDie(_, _));
  service_url_loader()->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      nullptr, network::SimpleURLLoader::BodyAsStringCallback());
}

TEST_F(MeasuredURLLoaderTest, DownloadToFilePassThrough) {
  EXPECT_CALL(mock_simple_url_loader(), DownloadToFile(_, _, _, _));
  service_url_loader()->DownloadToFile(
      nullptr, network::SimpleURLLoader::DownloadToFileCompleteCallback(),
      base::FilePath(), 0);
}

TEST_F(MeasuredURLLoaderTest, DownloadToTempFilePassThrough) {
  EXPECT_CALL(mock_simple_url_loader(), DownloadToTempFile(_, _, _));
  service_url_loader()->DownloadToTempFile(
      nullptr, network::SimpleURLLoader::DownloadToFileCompleteCallback(), 0);
}

TEST_F(MeasuredURLLoaderTest, DownloadAsStreamPassThrough) {
  EXPECT_CALL(mock_simple_url_loader(), DownloadAsStream(_, _));
  service_url_loader()->DownloadAsStream(nullptr, nullptr);
}

TEST_F(MeasuredURLLoaderTest, SetOnRedirectCallbackPassThrough) {
  EXPECT_CALL(mock_simple_url_loader(), SetOnRedirectCallback(_));
  service_url_loader()->SetOnRedirectCallback(
      network::SimpleURLLoader::OnRedirectCallback());
}

TEST_F(MeasuredURLLoaderTest, SetOnResponseStartedCallbackPassThrough) {
  EXPECT_CALL(mock_simple_url_loader(), SetOnResponseStartedCallback(_));
  service_url_loader()->SetOnResponseStartedCallback(
      network::SimpleURLLoader::OnResponseStartedCallback());
}

TEST_F(MeasuredURLLoaderTest, SetAllowPartialResultsPassThrough) {
  EXPECT_CALL(mock_simple_url_loader(), SetAllowPartialResults(_));
  service_url_loader()->SetAllowPartialResults(true);
}

TEST_F(MeasuredURLLoaderTest, SetAllowHttpErrorResultsPassThrough) {
  EXPECT_CALL(mock_simple_url_loader(), SetAllowHttpErrorResults(_));
  service_url_loader()->SetAllowHttpErrorResults(true);
}

TEST_F(MeasuredURLLoaderTest, AttachStringForUploadPassThrough) {
  EXPECT_CALL(mock_simple_url_loader(), AttachStringForUpload(_, _));
  service_url_loader()->AttachStringForUpload(std::string(), std::string());
}

TEST_F(MeasuredURLLoaderTest, AttachFileForUploadPassThrough) {
  EXPECT_CALL(mock_simple_url_loader(), AttachFileForUpload(_, _));
  service_url_loader()->AttachFileForUpload(base::FilePath(), std::string());
}

TEST_F(MeasuredURLLoaderTest, SetRetryOptionsPassThrough) {
  EXPECT_CALL(mock_simple_url_loader(), SetRetryOptions(_, _));
  service_url_loader()->SetRetryOptions(0, 0);
}

TEST_F(MeasuredURLLoaderTest, NetErrorPassThrough) {
  EXPECT_CALL(mock_simple_url_loader(), NetError());
  service_url_loader()->NetError();
}

TEST_F(MeasuredURLLoaderTest, ResponseInfoPassThrough) {
  EXPECT_CALL(mock_simple_url_loader(), ResponseInfo());
  service_url_loader()->ResponseInfo();
}

}  // namespace data_use_measurement
