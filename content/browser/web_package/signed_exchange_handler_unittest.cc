// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_handler.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "content/browser/web_package/signed_exchange_cert_fetcher_factory.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/filter/mock_source_stream.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const uint64_t kSignatureHeaderDate = 1520834000;
const int kOutputBufferSize = 4096;

std::string GetTestFileContents(const char* name) {
  base::FilePath path;
  PathService::Get(content::DIR_TEST_DATA, &path);
  path = path.AppendASCII("htxg").AppendASCII(name);

  std::string contents;
  CHECK(base::ReadFileToString(path, &contents));
  return contents;
}

class MockSignedExchangeCertFetcherFactory
    : public SignedExchangeCertFetcherFactory {
 public:
  MockSignedExchangeCertFetcherFactory(const GURL& expected_cert_url,
                                       std::string cert_str)
      : expected_cert_url_(expected_cert_url), cert_str_(cert_str) {}

  std::unique_ptr<SignedExchangeCertFetcher> CreateFetcherAndStart(
      const GURL& cert_url,
      bool force_fetch,
      SignedExchangeCertFetcher::CertificateCallback callback) override {
    EXPECT_EQ(cert_url, expected_cert_url_);

    base::Optional<std::vector<base::StringPiece>> der_certs =
        SignedExchangeCertFetcher::GetCertChainFromMessage(cert_str_);
    DCHECK(der_certs);
    scoped_refptr<net::X509Certificate> cert =
        net::X509Certificate::CreateFromDERCertChain(*der_certs);

    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), cert));
    return nullptr;
  }

 private:
  const GURL expected_cert_url_;
  const std::string cert_str_;
};

}  // namespace

class SignedExchangeHandlerTest
    : public ::testing::TestWithParam<net::MockSourceStream::Mode> {
 public:
  SignedExchangeHandlerTest()
      : mock_cert_verifier_(std::make_unique<net::MockCertVerifier>()),
        request_initiator_(
            url::Origin::Create(GURL("https://htxg.example.com/test.htxg"))) {}

  void SetUp() override {
    SignedExchangeHandler::SetCertVerifierForTesting(mock_cert_verifier_.get());
    SignedExchangeHandler::SetVerificationTimeForTesting(
        base::Time::UnixEpoch() +
        base::TimeDelta::FromSeconds(kSignatureHeaderDate));
    feature_list_.InitAndEnableFeature(features::kSignedHTTPExchange);

    output_buffer_ = new net::IOBuffer(kOutputBufferSize);
    std::unique_ptr<net::MockSourceStream> source(new net::MockSourceStream());
    source->set_read_one_byte_at_a_time(true);
    source_ = source.get();
    auto cert_fetcher_factory =
        std::make_unique<MockSignedExchangeCertFetcherFactory>(
            GURL("https://cert.example.org/cert.msg"),
            GetTestFileContents("wildcard_example.org.public.pem.msg"));
    request_context_getter_ = new net::TestURLRequestContextGetter(
        scoped_task_environment_.GetMainThreadTaskRunner());
    handler_ = std::make_unique<SignedExchangeHandler>(
        "application/signed-exchange;v=b0", std::move(source),
        base::BindOnce(&SignedExchangeHandlerTest::OnHeaderFound,
                       base::Unretained(this)),
        std::move(cert_fetcher_factory), request_context_getter_);
  }

  void TearDown() override {
    SignedExchangeHandler::SetCertVerifierForTesting(nullptr);
    SignedExchangeHandler::SetVerificationTimeForTesting(
        base::Optional<base::Time>());
  }

  // Reads from |payload_stream_| until an error occurs or the EOF is reached.
  // When an error occurs, returns the net error code. When an EOF is reached,
  // returns the number of bytes read and appends data read to |output|.
  int ReadPayloadStream(std::string* output) {
    int bytes_read = 0;
    while (true) {
      net::TestCompletionCallback callback;
      int rv = payload_stream_->Read(output_buffer_.get(), kOutputBufferSize,
                                     callback.callback());
      if (rv == net::ERR_IO_PENDING) {
        while (source_->awaiting_completion())
          source_->CompleteNextRead();
        rv = callback.WaitForResult();
      }
      if (rv == net::OK)
        break;
      if (rv < net::OK)
        return rv;
      EXPECT_GT(rv, net::OK);
      bytes_read += rv;
      output->append(output_buffer_->data(), rv);
    }
    return bytes_read;
  }

  bool read_header() const { return read_header_; }
  net::Error error() const { return error_; }
  const network::ResourceResponseHead& resource_response() const {
    return resource_response_;
  }

  void WaitForHeader() {
    while (!read_header()) {
      while (source_->awaiting_completion())
        source_->CompleteNextRead();
      scoped_task_environment_.RunUntilIdle();
    }
  }

 protected:
  std::unique_ptr<net::MockCertVerifier> mock_cert_verifier_;
  net::MockSourceStream* source_;
  std::unique_ptr<SignedExchangeHandler> handler_;

 private:
  void OnHeaderFound(net::Error error,
                     const GURL&,
                     const std::string&,
                     const network::ResourceResponseHead& resource_response,
                     std::unique_ptr<net::SourceStream> payload_stream,
                     base::Optional<net::SSLInfo>) {
    read_header_ = true;
    error_ = error;
    resource_response_ = resource_response;
    payload_stream_ = std::move(payload_stream);
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  const url::Origin request_initiator_;
  scoped_refptr<net::IOBuffer> output_buffer_;

  bool read_header_ = false;
  net::Error error_;
  network::ResourceResponseHead resource_response_;
  std::unique_ptr<net::SourceStream> payload_stream_;
};

TEST_P(SignedExchangeHandlerTest, Empty) {
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(net::ERR_FAILED, error());
}

TEST_P(SignedExchangeHandlerTest, Simple) {
  mock_cert_verifier_->set_default_result(net::OK);
  std::string contents = GetTestFileContents("test.example.org_test.htxg");
  source_->AddReadResult(contents.data(), contents.size(), net::OK, GetParam());
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(net::OK, error());
  EXPECT_EQ(200, resource_response().headers->response_code());
  EXPECT_EQ("text/html", resource_response().mime_type);
  EXPECT_EQ("utf-8", resource_response().charset);
  EXPECT_FALSE(resource_response().load_timing.request_start_time.is_null());
  EXPECT_FALSE(resource_response().load_timing.request_start.is_null());
  EXPECT_FALSE(resource_response().load_timing.send_start.is_null());
  EXPECT_FALSE(resource_response().load_timing.send_end.is_null());
  EXPECT_FALSE(resource_response().load_timing.receive_headers_end.is_null());

  std::string payload;
  int rv = ReadPayloadStream(&payload);

  std::string expected_payload = GetTestFileContents("test.html");

  EXPECT_EQ(payload, expected_payload);
  EXPECT_EQ(rv, static_cast<int>(expected_payload.size()));
}

TEST_P(SignedExchangeHandlerTest, MimeType) {
  mock_cert_verifier_->set_default_result(net::OK);
  std::string contents = GetTestFileContents("test.example.org_hello.txt.htxg");
  source_->AddReadResult(contents.data(), contents.size(), net::OK, GetParam());
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(net::OK, error());
  EXPECT_EQ(200, resource_response().headers->response_code());
  EXPECT_EQ("text/plain", resource_response().mime_type);
  EXPECT_EQ("iso-8859-1", resource_response().charset);

  std::string payload;
  int rv = ReadPayloadStream(&payload);

  std::string expected_payload = GetTestFileContents("hello.txt");

  EXPECT_EQ(payload, expected_payload);
  EXPECT_EQ(rv, static_cast<int>(expected_payload.size()));
}

TEST_P(SignedExchangeHandlerTest, ParseError) {
  const uint8_t data[] = {0x00, 0x00, 0x01, 0x00};
  source_->AddReadResult(reinterpret_cast<const char*>(data), sizeof(data),
                         net::OK, GetParam());
  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(net::ERR_FAILED, error());
}

TEST_P(SignedExchangeHandlerTest, TruncatedInHeader) {
  std::string contents = GetTestFileContents("test.example.org_test.htxg");
  contents.resize(30);
  source_->AddReadResult(contents.data(), contents.size(), net::OK, GetParam());
  source_->AddReadResult(nullptr, 0, net::OK, GetParam());

  WaitForHeader();

  ASSERT_TRUE(read_header());
  EXPECT_EQ(net::ERR_FAILED, error());
}

INSTANTIATE_TEST_CASE_P(SignedExchangeHandlerTests,
                        SignedExchangeHandlerTest,
                        ::testing::Values(net::MockSourceStream::SYNC,
                                          net::MockSourceStream::ASYNC));

}  // namespace content
