// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/attachments/attachment_downloader_impl.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/engine/attachments/attachment_util.h"
#include "components/sync/engine_impl/attachments/attachment_uploader_impl.h"
#include "components/sync/model/attachments/attachment.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/util/crc32c.h"

namespace syncer {

namespace {

const char kAccountId[] = "attachments@gmail.com";
const char kAccessToken[] = "access.token";
const char kAttachmentServerUrl[] = "http://attachments.com/";
const char kAttachmentContent[] = "attachment.content";
const char kStoreBirthday[] = "z00000000-0000-007b-0000-0000000004d2";
const ModelType kModelType = ModelType::ARTICLES;

// MockOAuth2TokenService remembers last request for access token and verifies
// that only one request is active at a time.
// Call RespondToAccessTokenRequest to respond to it.
class MockOAuth2TokenService : public FakeOAuth2TokenService {
 public:
  MockOAuth2TokenService() : num_invalidate_token_(0) {}

  ~MockOAuth2TokenService() override {}

  void RespondToAccessTokenRequest(GoogleServiceAuthError error);

  int num_invalidate_token() const { return num_invalidate_token_; }

 protected:
  void FetchOAuth2Token(RequestImpl* request,
                        const std::string& account_id,
                        net::URLRequestContextGetter* getter,
                        const std::string& client_id,
                        const std::string& client_secret,
                        const ScopeSet& scopes) override;

  void InvalidateAccessTokenImpl(const std::string& account_id,
                                 const std::string& client_id,
                                 const ScopeSet& scopes,
                                 const std::string& access_token) override;

 private:
  base::WeakPtr<RequestImpl> last_request_;
  int num_invalidate_token_;
};

void MockOAuth2TokenService::RespondToAccessTokenRequest(
    GoogleServiceAuthError error) {
  EXPECT_TRUE(last_request_);
  std::string access_token;
  base::Time expiration_time;
  if (error == GoogleServiceAuthError::AuthErrorNone()) {
    access_token = kAccessToken;
    expiration_time = base::Time::Max();
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&OAuth2TokenService::RequestImpl::InformConsumer,
                 last_request_, error, access_token, expiration_time));
}

void MockOAuth2TokenService::FetchOAuth2Token(
    RequestImpl* request,
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes) {
  // Only one request at a time is allowed.
  EXPECT_FALSE(last_request_);
  last_request_ = request->AsWeakPtr();
}

void MockOAuth2TokenService::InvalidateAccessTokenImpl(
    const std::string& account_id,
    const std::string& client_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  ++num_invalidate_token_;
}

class TokenServiceProvider
    : public OAuth2TokenServiceRequest::TokenServiceProvider,
      base::NonThreadSafe {
 public:
  explicit TokenServiceProvider(OAuth2TokenService* token_service);

  // OAuth2TokenService::TokenServiceProvider implementation.
  scoped_refptr<base::SingleThreadTaskRunner> GetTokenServiceTaskRunner()
      override;
  OAuth2TokenService* GetTokenService() override;

 private:
  ~TokenServiceProvider() override;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  OAuth2TokenService* token_service_;
};

TokenServiceProvider::TokenServiceProvider(OAuth2TokenService* token_service)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      token_service_(token_service) {
  DCHECK(token_service_);
}

TokenServiceProvider::~TokenServiceProvider() {}

scoped_refptr<base::SingleThreadTaskRunner>
TokenServiceProvider::GetTokenServiceTaskRunner() {
  return task_runner_;
}

OAuth2TokenService* TokenServiceProvider::GetTokenService() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return token_service_;
}

}  // namespace

class AttachmentDownloaderImplTest : public testing::Test {
 protected:
  using ResultsMap =
      std::map<AttachmentId, AttachmentDownloader::DownloadResult>;

  enum HashHeaderType {
    HASH_HEADER_NONE,
    HASH_HEADER_VALID,
    HASH_HEADER_INVALID
  };

  AttachmentDownloaderImplTest()
      : num_completed_downloads_(0),
        attachment_id_(Attachment::Create(new base::RefCountedStaticMemory(
                                              kAttachmentContent,
                                              strlen(kAttachmentContent)))
                           .GetId()) {}

  void SetUp() override;
  void TearDown() override;

  AttachmentDownloader* downloader() { return attachment_downloader_.get(); }

  MockOAuth2TokenService* token_service() { return token_service_.get(); }

  int num_completed_downloads() { return num_completed_downloads_; }

  const AttachmentId attachment_id() const { return attachment_id_; }

  AttachmentDownloader::DownloadCallback download_callback(
      const AttachmentId& id) {
    return base::Bind(&AttachmentDownloaderImplTest::DownloadDone,
                      base::Unretained(this), id);
  }

  // Respond with |response_code| and hash header of type |hash_header_type|.
  void CompleteDownload(int response_code, HashHeaderType hash_header_type);

  void DownloadDone(const AttachmentId& attachment_id,
                    const AttachmentDownloader::DownloadResult& result,
                    std::unique_ptr<Attachment> attachment);

  void VerifyDownloadResult(const AttachmentId& attachment_id,
                            const AttachmentDownloader::DownloadResult& result);

  void RunMessageLoop();

 private:
  static void AddHashHeader(HashHeaderType hash_header_type,
                            net::TestURLFetcher* fetcher);

  base::MessageLoopForIO message_loop_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  std::unique_ptr<MockOAuth2TokenService> token_service_;
  std::unique_ptr<AttachmentDownloader> attachment_downloader_;
  ResultsMap download_results_;
  int num_completed_downloads_;
  const AttachmentId attachment_id_;
};

void AttachmentDownloaderImplTest::SetUp() {
  url_request_context_getter_ =
      new net::TestURLRequestContextGetter(message_loop_.task_runner());
  url_fetcher_factory_.set_remove_fetcher_on_delete(true);
  token_service_ = base::MakeUnique<MockOAuth2TokenService>();
  token_service_->AddAccount(kAccountId);
  scoped_refptr<OAuth2TokenServiceRequest::TokenServiceProvider>
      token_service_provider(new TokenServiceProvider(token_service_.get()));

  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kChromeSyncOAuth2Scope);
  attachment_downloader_ = AttachmentDownloader::Create(
      GURL(kAttachmentServerUrl), url_request_context_getter_, kAccountId,
      scopes, token_service_provider, std::string(kStoreBirthday), kModelType);
}

void AttachmentDownloaderImplTest::TearDown() {
  RunMessageLoop();
}

void AttachmentDownloaderImplTest::CompleteDownload(
    int response_code,
    HashHeaderType hash_header_type) {
  // TestURLFetcherFactory remembers last active URLFetcher.
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  // There should be outstanding url fetch request.
  EXPECT_TRUE(fetcher);
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(response_code);
  if (response_code == net::HTTP_OK) {
    fetcher->SetResponseString(kAttachmentContent);
  }
  AddHashHeader(hash_header_type, fetcher);

  // Call URLFetcherDelegate.
  net::URLFetcherDelegate* delegate = fetcher->delegate();
  delegate->OnURLFetchComplete(fetcher);
  RunMessageLoop();
  // Once result is processed URLFetcher should be deleted.
  fetcher = url_fetcher_factory_.GetFetcherByID(0);
  EXPECT_FALSE(fetcher);
}

void AttachmentDownloaderImplTest::DownloadDone(
    const AttachmentId& attachment_id,
    const AttachmentDownloader::DownloadResult& result,
    std::unique_ptr<Attachment> attachment) {
  download_results_.insert(std::make_pair(attachment_id, result));
  if (result == AttachmentDownloader::DOWNLOAD_SUCCESS) {
    // Successful download should be accompanied by valid attachment with
    // matching id and valid data.
    EXPECT_TRUE(attachment);
    EXPECT_EQ(attachment_id, attachment->GetId());

    scoped_refptr<base::RefCountedMemory> data = attachment->GetData();
    std::string data_as_string(data->front_as<char>(), data->size());
    EXPECT_EQ(data_as_string, kAttachmentContent);
  } else {
    EXPECT_FALSE(attachment);
  }
  ++num_completed_downloads_;
}

void AttachmentDownloaderImplTest::VerifyDownloadResult(
    const AttachmentId& attachment_id,
    const AttachmentDownloader::DownloadResult& result) {
  ResultsMap::const_iterator iter = download_results_.find(attachment_id);
  EXPECT_TRUE(iter != download_results_.end());
  EXPECT_EQ(iter->second, result);
}

void AttachmentDownloaderImplTest::RunMessageLoop() {
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

void AttachmentDownloaderImplTest::AddHashHeader(
    HashHeaderType hash_header_type,
    net::TestURLFetcher* fetcher) {
  std::string header = "X-Goog-Hash: crc32c=";
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(""));
  switch (hash_header_type) {
    case HASH_HEADER_NONE:
      break;
    case HASH_HEADER_VALID:
      header += AttachmentUploaderImpl::FormatCrc32cHash(leveldb::crc32c::Value(
          kAttachmentContent, strlen(kAttachmentContent)));
      headers->AddHeader(header);
      break;
    case HASH_HEADER_INVALID:
      header += "BOGUS1==";
      headers->AddHeader(header);
      break;
  }
  fetcher->set_response_headers(headers);
}

TEST_F(AttachmentDownloaderImplTest, HappyCase) {
  AttachmentId id1 = attachment_id();
  // DownloadAttachment should trigger RequestAccessToken.
  downloader()->DownloadAttachment(id1, download_callback(id1));
  RunMessageLoop();
  // Return valid access token.
  token_service()->RespondToAccessTokenRequest(
      GoogleServiceAuthError::AuthErrorNone());
  RunMessageLoop();
  // Catch histogram entries.
  base::HistogramTester histogram_tester;
  // Check that there is outstanding URLFetcher request and complete it.
  CompleteDownload(net::HTTP_OK, HASH_HEADER_VALID);
  // Verify that the response code was logged properly.
  histogram_tester.ExpectUniqueSample("Sync.Attachments.DownloadResponseCode",
                                      net::HTTP_OK, 1);
  // Verify that callback was called for the right id with the right result.
  VerifyDownloadResult(id1, AttachmentDownloader::DOWNLOAD_SUCCESS);
}

TEST_F(AttachmentDownloaderImplTest, SameIdMultipleDownloads) {
  AttachmentId id1 = attachment_id();
  base::HistogramTester histogram_tester;
  // Call DownloadAttachment two times for the same id.
  downloader()->DownloadAttachment(id1, download_callback(id1));
  downloader()->DownloadAttachment(id1, download_callback(id1));
  RunMessageLoop();
  // Return valid access token.
  token_service()->RespondToAccessTokenRequest(
      GoogleServiceAuthError::AuthErrorNone());
  RunMessageLoop();
  // Start one more download after access token is received.
  downloader()->DownloadAttachment(id1, download_callback(id1));
  // Complete URLFetcher request.
  CompleteDownload(net::HTTP_OK, HASH_HEADER_VALID);
  // Verify that all download requests completed.
  VerifyDownloadResult(id1, AttachmentDownloader::DOWNLOAD_SUCCESS);
  EXPECT_EQ(3, num_completed_downloads());

  // Let's download the same attachment again.
  downloader()->DownloadAttachment(id1, download_callback(id1));
  RunMessageLoop();
  // Verify that it didn't finish prematurely.
  EXPECT_EQ(3, num_completed_downloads());
  // Return valid access token.
  token_service()->RespondToAccessTokenRequest(
      GoogleServiceAuthError::AuthErrorNone());
  RunMessageLoop();
  // Complete URLFetcher request.
  CompleteDownload(net::HTTP_OK, HASH_HEADER_VALID);
  // Verify that all download requests completed.
  VerifyDownloadResult(id1, AttachmentDownloader::DOWNLOAD_SUCCESS);
  EXPECT_EQ(4, num_completed_downloads());
  histogram_tester.ExpectUniqueSample("Sync.Attachments.DownloadResponseCode",
                                      net::HTTP_OK, 2);
}

TEST_F(AttachmentDownloaderImplTest, RequestAccessTokenFails) {
  AttachmentId id1 = attachment_id();
  AttachmentId id2 = AttachmentId::Create(id1.GetSize(), id1.GetCrc32c());
  // Trigger first RequestAccessToken.
  downloader()->DownloadAttachment(id1, download_callback(id1));
  RunMessageLoop();
  // Return valid access token.
  token_service()->RespondToAccessTokenRequest(
      GoogleServiceAuthError::AuthErrorNone());
  RunMessageLoop();
  // Trigger second RequestAccessToken.
  downloader()->DownloadAttachment(id2, download_callback(id2));
  RunMessageLoop();
  // Fail RequestAccessToken.
  token_service()->RespondToAccessTokenRequest(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  RunMessageLoop();
  // Only id2 should fail.
  VerifyDownloadResult(id2, AttachmentDownloader::DOWNLOAD_TRANSIENT_ERROR);
  // Complete request for id1.
  CompleteDownload(net::HTTP_OK, HASH_HEADER_VALID);
  VerifyDownloadResult(id1, AttachmentDownloader::DOWNLOAD_SUCCESS);
}

TEST_F(AttachmentDownloaderImplTest, URLFetcher_BadToken) {
  AttachmentId id1 = attachment_id();
  downloader()->DownloadAttachment(id1, download_callback(id1));
  RunMessageLoop();
  // Return valid access token.
  token_service()->RespondToAccessTokenRequest(
      GoogleServiceAuthError::AuthErrorNone());
  RunMessageLoop();
  // Fail URLFetcher. This should trigger download failure and access token
  // invalidation.
  base::HistogramTester histogram_tester;
  CompleteDownload(net::HTTP_UNAUTHORIZED, HASH_HEADER_VALID);
  EXPECT_EQ(1, token_service()->num_invalidate_token());
  VerifyDownloadResult(id1, AttachmentDownloader::DOWNLOAD_TRANSIENT_ERROR);
  histogram_tester.ExpectUniqueSample("Sync.Attachments.DownloadResponseCode",
                                      net::HTTP_UNAUTHORIZED, 1);
}

TEST_F(AttachmentDownloaderImplTest, URLFetcher_ServiceUnavailable) {
  AttachmentId id1 = attachment_id();
  downloader()->DownloadAttachment(id1, download_callback(id1));
  RunMessageLoop();
  // Return valid access token.
  token_service()->RespondToAccessTokenRequest(
      GoogleServiceAuthError::AuthErrorNone());
  RunMessageLoop();
  // Fail URLFetcher. This should trigger download failure. Access token
  // shouldn't be invalidated.
  base::HistogramTester histogram_tester;
  CompleteDownload(net::HTTP_SERVICE_UNAVAILABLE, HASH_HEADER_VALID);
  EXPECT_EQ(0, token_service()->num_invalidate_token());
  VerifyDownloadResult(id1, AttachmentDownloader::DOWNLOAD_TRANSIENT_ERROR);
  histogram_tester.ExpectUniqueSample("Sync.Attachments.DownloadResponseCode",
                                      net::HTTP_SERVICE_UNAVAILABLE, 1);
}

// Verify that if no hash is present on the response the downloader accepts the
// received attachment.
TEST_F(AttachmentDownloaderImplTest, NoHash) {
  AttachmentId id1 = attachment_id();
  downloader()->DownloadAttachment(id1, download_callback(id1));
  RunMessageLoop();
  token_service()->RespondToAccessTokenRequest(
      GoogleServiceAuthError::AuthErrorNone());
  RunMessageLoop();
  CompleteDownload(net::HTTP_OK, HASH_HEADER_NONE);
  VerifyDownloadResult(id1, AttachmentDownloader::DOWNLOAD_SUCCESS);
}

// Verify that if an invalid hash is present on the response the downloader
// treats it as a transient error.
TEST_F(AttachmentDownloaderImplTest, InvalidHash) {
  AttachmentId id1 = attachment_id();
  downloader()->DownloadAttachment(id1, download_callback(id1));
  RunMessageLoop();
  token_service()->RespondToAccessTokenRequest(
      GoogleServiceAuthError::AuthErrorNone());
  RunMessageLoop();
  CompleteDownload(net::HTTP_OK, HASH_HEADER_INVALID);
  VerifyDownloadResult(id1, AttachmentDownloader::DOWNLOAD_TRANSIENT_ERROR);
}

// Verify that when the hash from the attachment id does not match the one on
// the response the result is an unspecified error.
TEST_F(AttachmentDownloaderImplTest, IdHashDoesNotMatch) {
  // id1 has the wrong crc32c.
  AttachmentId id1 = AttachmentId::Create(attachment_id().GetSize(), 12345);
  downloader()->DownloadAttachment(id1, download_callback(id1));
  RunMessageLoop();
  token_service()->RespondToAccessTokenRequest(
      GoogleServiceAuthError::AuthErrorNone());
  RunMessageLoop();
  CompleteDownload(net::HTTP_OK, HASH_HEADER_VALID);
  VerifyDownloadResult(id1, AttachmentDownloader::DOWNLOAD_UNSPECIFIED_ERROR);
}

// Verify that extract fails when there is no headers object.
TEST_F(AttachmentDownloaderImplTest, ExtractCrc32c_NoHeaders) {
  uint32_t extracted;
  ASSERT_FALSE(AttachmentDownloaderImpl::ExtractCrc32c(nullptr, &extracted));
}

// Verify that extract fails when there is no crc32c value.
TEST_F(AttachmentDownloaderImplTest, ExtractCrc32c_Empty) {
  std::string raw;
  raw += "HTTP/1.1 200 OK\n";
  raw += "Foo: bar\n";
  raw += "X-Goog-HASH: crc32c=\n";
  raw += "\n";
  std::replace(raw.begin(), raw.end(), '\n', '\0');
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(raw));
  uint32_t extracted;
  ASSERT_FALSE(
      AttachmentDownloaderImpl::ExtractCrc32c(headers.get(), &extracted));
}

// Verify that extract finds the first crc32c and ignores others.
TEST_F(AttachmentDownloaderImplTest, ExtractCrc32c_First) {
  const std::string expected_encoded = "z8SuHQ==";
  const uint32_t expected = 3485773341;
  std::string raw;
  raw += "HTTP/1.1 200 OK\n";
  raw += "Foo: bar\n";
  // Ignored because it's the wrong header.
  raw += "X-Goog-Hashes: crc32c=AAAAAA==\n";
  // Header name matches.  The md5 item is ignored.
  raw += "X-Goog-HASH: md5=rL0Y20zC+Fzt72VPzMSk2A==,crc32c=" +
         expected_encoded + "\n";
  // Ignored because we already found a crc32c in the one above.
  raw += "X-Goog-HASH: crc32c=AAAAAA==\n";
  raw += "\n";
  std::replace(raw.begin(), raw.end(), '\n', '\0');
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(raw));
  uint32_t extracted;
  ASSERT_TRUE(
      AttachmentDownloaderImpl::ExtractCrc32c(headers.get(), &extracted));
  ASSERT_EQ(expected, extracted);
}

// Verify that extract fails when encoded value is too long.
TEST_F(AttachmentDownloaderImplTest, ExtractCrc32c_TooLong) {
  std::string raw;
  raw += "HTTP/1.1 200 OK\n";
  raw += "Foo: bar\n";
  raw += "X-Goog-HASH: crc32c=AAAAAAAA\n";
  raw += "\n";
  std::replace(raw.begin(), raw.end(), '\n', '\0');
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(raw));
  uint32_t extracted;
  ASSERT_FALSE(
      AttachmentDownloaderImpl::ExtractCrc32c(headers.get(), &extracted));
}

// Verify that extract fails if there is no crc32c.
TEST_F(AttachmentDownloaderImplTest, ExtractCrc32c_None) {
  std::string raw;
  raw += "HTTP/1.1 200 OK\n";
  raw += "Foo: bar\n";
  raw += "X-Goog-Hash: md5=rL0Y20zC+Fzt72VPzMSk2A==\n";
  raw += "\n";
  std::replace(raw.begin(), raw.end(), '\n', '\0');
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(raw));
  uint32_t extracted;
  ASSERT_FALSE(
      AttachmentDownloaderImpl::ExtractCrc32c(headers.get(), &extracted));
}

}  // namespace syncer
