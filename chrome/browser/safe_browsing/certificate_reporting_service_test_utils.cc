// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/certificate_reporting_service_test_utils.h"

#include "base/threading/thread_task_runner_handle.h"
#include "components/certificate_reporting/encrypted_cert_logger.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "crypto/curve25519.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_data_job.h"
#include "net/url_request/url_request_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const uint32_t kServerPublicKeyTestVersion = 16;

void SetUpURLHandlersOnIOThread(
    std::unique_ptr<net::URLRequestInterceptor> url_request_interceptor) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddUrlInterceptor(
      CertificateReportingService::GetReportingURLForTesting(),
      std::move(url_request_interceptor));
}

std::string GetUploadData(net::URLRequest* request) {
  const net::UploadDataStream* stream = request->get_upload();
  EXPECT_TRUE(stream);
  EXPECT_TRUE(stream->GetElementReaders());
  EXPECT_EQ(1u, stream->GetElementReaders()->size());
  const net::UploadBytesElementReader* reader =
      (*stream->GetElementReaders())[0]->AsBytesReader();
  return std::string(reader->bytes(), reader->length());
}

std::string GetReportContents(net::URLRequest* request,
                              const uint8_t* server_private_key) {
  std::string serialized_report(GetUploadData(request));
  certificate_reporting::EncryptedCertLoggerRequest encrypted_request;
  EXPECT_TRUE(encrypted_request.ParseFromString(serialized_report));
  EXPECT_EQ(kServerPublicKeyTestVersion,
            encrypted_request.server_public_key_version());
  EXPECT_EQ(certificate_reporting::EncryptedCertLoggerRequest::
                AEAD_ECDH_AES_128_CTR_HMAC_SHA256,
            encrypted_request.algorithm());
  std::string decrypted_report;
  certificate_reporting::ErrorReporter::DecryptErrorReport(
      server_private_key, encrypted_request, &decrypted_report);
  return decrypted_report;
}

}  // namespace

namespace certificate_reporting_test_utils {

DelayableCertReportURLRequestJob::DelayableCertReportURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate)
    : net::URLRequestJob(request, network_delegate), weak_factory_(this) {}

DelayableCertReportURLRequestJob::~DelayableCertReportURLRequestJob() {}

base::WeakPtr<DelayableCertReportURLRequestJob>
DelayableCertReportURLRequestJob::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void DelayableCertReportURLRequestJob::Start() {
  started_ = true;
  if (delayed_) {
    // Do nothing until Resume() is called.
    return;
  }
  Resume();
}

int DelayableCertReportURLRequestJob::ReadRawData(net::IOBuffer* buf,
                                                  int buf_size) {
  // Report sender ignores responses. Return empty response.
  return 0;
}

int DelayableCertReportURLRequestJob::GetResponseCode() const {
  // Report sender ignores responses. Return empty response.
  return 200;
}

void DelayableCertReportURLRequestJob::GetResponseInfo(
    net::HttpResponseInfo* info) {
  // Report sender ignores responses. Return empty response.
}

void DelayableCertReportURLRequestJob::Resume() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(delayed_);
  if (!started_) {
    // If Start() hasn't been called yet, then unset |delayed_| so
    // that when Start() is called, the request will begin
    // immediately.
    delayed_ = false;
    return;
  }
  // Start reading asynchronously as would a normal network request.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&DelayableCertReportURLRequestJob::NotifyHeadersComplete,
                 weak_factory_.GetWeakPtr()));
}

CertReportJobInterceptor::CertReportJobInterceptor(
    ReportSendingResult expected_report_result,
    const uint8_t* server_private_key)
    : expected_report_result_(expected_report_result),
      server_private_key_(server_private_key),
      weak_factory_(this) {}

CertReportJobInterceptor::~CertReportJobInterceptor() {}

net::URLRequestJob* CertReportJobInterceptor::MaybeInterceptRequest(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  const std::string uploaded_report =
      GetReportContents(request, server_private_key_);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&CertReportJobInterceptor::RequestCreated,
                 weak_factory_.GetWeakPtr(), uploaded_report,
                 expected_report_result_));

  if (expected_report_result_ == REPORTS_FAIL) {
    return new net::URLRequestFailedJob(request, network_delegate,
                                        net::ERR_SSL_PROTOCOL_ERROR);
  } else if (expected_report_result_ == REPORTS_DELAY) {
    DCHECK(!delayed_request_) << "Supports only one delayed request at a time";
    DelayableCertReportURLRequestJob* job =
        new DelayableCertReportURLRequestJob(request, network_delegate);
    delayed_request_ = job->GetWeakPtr();
    return job;
  }
  // Successful url request job.
  return new net::URLRequestMockDataJob(request, network_delegate, "some data",
                                        1, false);
}

void CertReportJobInterceptor::SetFailureMode(
    ReportSendingResult expected_report_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&CertReportJobInterceptor::SetFailureModeOnIOThread,
                 weak_factory_.GetWeakPtr(), expected_report_result));
}

void CertReportJobInterceptor::Resume() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&CertReportJobInterceptor::ResumeOnIOThread,
                 base::Unretained(this)));
}

const std::set<std::string>& CertReportJobInterceptor::successful_reports()
    const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return successful_reports_;
}

const std::set<std::string>& CertReportJobInterceptor::failed_reports() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return failed_reports_;
}

const std::set<std::string>& CertReportJobInterceptor::delayed_reports() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return delayed_reports_;
}

void CertReportJobInterceptor::ClearObservedReports() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  successful_reports_.clear();
  failed_reports_.clear();
  delayed_reports_.clear();
}

void CertReportJobInterceptor::SetFailureModeOnIOThread(
    ReportSendingResult expected_report_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  expected_report_result_ = expected_report_result;
}

void CertReportJobInterceptor::ResumeOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  EXPECT_EQ(REPORTS_DELAY, expected_report_result_);
  if (delayed_request_)
    delayed_request_->Resume();
}

void CertReportJobInterceptor::RequestCreated(
    const std::string& uploaded_report,
    ReportSendingResult expected_report_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  switch (expected_report_result) {
    case REPORTS_SUCCESSFUL:
      successful_reports_.insert(uploaded_report);
      break;
    case REPORTS_FAIL:
      failed_reports_.insert(uploaded_report);
      break;
    case REPORTS_DELAY:
      delayed_reports_.insert(uploaded_report);
      break;
  }
}

CertificateReportingServiceTestNetworkDelegate::
    CertificateReportingServiceTestNetworkDelegate(
        const base::Callback<void()>& url_request_destroyed_callback)
    : url_request_destroyed_callback_(url_request_destroyed_callback) {}

CertificateReportingServiceTestNetworkDelegate::
    ~CertificateReportingServiceTestNetworkDelegate() {}

void CertificateReportingServiceTestNetworkDelegate::OnURLRequestDestroyed(
    net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   url_request_destroyed_callback_);
}

CertificateReportingServiceTestBase::ReportExpectation::ReportExpectation() {}

CertificateReportingServiceTestBase::ReportExpectation::ReportExpectation(
    const ReportExpectation& other) = default;

CertificateReportingServiceTestBase::ReportExpectation::~ReportExpectation() {}

// static
CertificateReportingServiceTestBase::ReportExpectation
CertificateReportingServiceTestBase::ReportExpectation::Successful(
    const std::set<std::string>& reports) {
  ReportExpectation expectation;
  expectation.successful_reports = reports;
  return expectation;
}

// static
CertificateReportingServiceTestBase::ReportExpectation
CertificateReportingServiceTestBase::ReportExpectation::Failed(
    const std::set<std::string>& reports) {
  ReportExpectation expectation;
  expectation.failed_reports = reports;
  return expectation;
}

// static
CertificateReportingServiceTestBase::ReportExpectation
CertificateReportingServiceTestBase::ReportExpectation::Delayed(
    const std::set<std::string>& reports) {
  ReportExpectation expectation;
  expectation.delayed_reports = reports;
  return expectation;
}

CertificateReportingServiceTestBase::CertificateReportingServiceTestBase()
    : num_request_deletions_to_wait_for_(0), num_deleted_requests_(0) {
  memset(server_private_key_, 1, sizeof(server_private_key_));
  crypto::curve25519::ScalarBaseMult(server_private_key_, server_public_key_);
}

CertificateReportingServiceTestBase::~CertificateReportingServiceTestBase() {}

void CertificateReportingServiceTestBase::SetUpInterceptor() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  url_request_interceptor_ =
      new CertReportJobInterceptor(REPORTS_FAIL, server_private_key_);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(
          &CertificateReportingServiceTestBase::SetUpInterceptorOnIOThread,
          base::Unretained(this),
          base::Passed(std::unique_ptr<net::URLRequestInterceptor>(
              url_request_interceptor_))));
}

void CertificateReportingServiceTestBase::TearDownInterceptor() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(
          &CertificateReportingServiceTestBase::TearDownInterceptorOnIOThread,
          base::Unretained(this)));
}

// Changes the behavior of report uploads to fail or succeed.
void CertificateReportingServiceTestBase::SetFailureMode(
    ReportSendingResult expected_report_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  url_request_interceptor_->SetFailureMode(expected_report_result);
}

void CertificateReportingServiceTestBase::ResumeDelayedRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  url_request_interceptor_->Resume();
}

void CertificateReportingServiceTestBase::WaitForRequestsDestroyed(
    const ReportExpectation& expectation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!run_loop_);

  const int num_request_deletions_to_wait_for =
      expectation.successful_reports.size() +
      expectation.failed_reports.size() + expectation.delayed_reports.size();

  ASSERT_LE(num_deleted_requests_, num_request_deletions_to_wait_for)
      << "Observed unexpected report";
  if (num_deleted_requests_ < num_request_deletions_to_wait_for) {
    num_request_deletions_to_wait_for_ = num_request_deletions_to_wait_for;
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
    run_loop_.reset(nullptr);
    EXPECT_EQ(0, num_deleted_requests_);
    EXPECT_EQ(0, num_request_deletions_to_wait_for_);
  } else if (num_deleted_requests_ == num_request_deletions_to_wait_for) {
    num_deleted_requests_ = 0;
    num_request_deletions_to_wait_for_ = 0;
  }
  EXPECT_EQ(expectation.successful_reports,
            url_request_interceptor_->successful_reports());
  EXPECT_EQ(expectation.failed_reports,
            url_request_interceptor_->failed_reports());
  EXPECT_EQ(expectation.delayed_reports,
            url_request_interceptor_->delayed_reports());
  url_request_interceptor_->ClearObservedReports();
}

uint8_t* CertificateReportingServiceTestBase::server_public_key() {
  return server_public_key_;
}

uint32_t CertificateReportingServiceTestBase::server_public_key_version()
    const {
  return kServerPublicKeyTestVersion;
}

net::NetworkDelegate* CertificateReportingServiceTestBase::network_delegate() {
  return network_delegate_.get();
}

void CertificateReportingServiceTestBase::SetUpInterceptorOnIOThread(
    std::unique_ptr<net::URLRequestInterceptor> url_request_interceptor) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  network_delegate_.reset(new CertificateReportingServiceTestNetworkDelegate(
      base::Bind(&CertificateReportingServiceTestBase::OnURLRequestDestroyed,
                 base::Unretained(this))));
  SetUpURLHandlersOnIOThread(std::move(url_request_interceptor));
}

void CertificateReportingServiceTestBase::TearDownInterceptorOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  network_delegate_.reset(nullptr);
}

void CertificateReportingServiceTestBase::OnURLRequestDestroyed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  num_deleted_requests_++;
  if (!run_loop_) {
    return;
  }
  EXPECT_LE(num_deleted_requests_, num_request_deletions_to_wait_for_);
  if (num_deleted_requests_ == num_request_deletions_to_wait_for_) {
    num_request_deletions_to_wait_for_ = 0;
    num_deleted_requests_ = 0;
    run_loop_->Quit();
  }
}

}  // namespace certificate_reporting_test_utils
