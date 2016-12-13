// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_TEST_UTILS_H_
#define CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_TEST_UTILS_H_

#include <set>

#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/network_delegate_impl.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"

namespace net {
class NetworkDelegate;
}

namespace certificate_reporting_test_utils {

// Failure mode of the report sending attempts.
enum ReportSendingResult {
  // Report send attempts should be successful.
  REPORTS_SUCCESSFUL,
  // Report send attempts should fail.
  REPORTS_FAIL,
  // Report send attempts should hang until explicitly resumed.
  REPORTS_DELAY,
};

// A URLRequestJob that can be delayed until Resume() is called. Returns an
// empty response. If Resume() is called before a request is made, then the
// request will not be delayed.
class DelayableCertReportURLRequestJob : public net::URLRequestJob {
 public:
  DelayableCertReportURLRequestJob(net::URLRequest* request,
                                   net::NetworkDelegate* network_delegate);
  ~DelayableCertReportURLRequestJob() override;

  base::WeakPtr<DelayableCertReportURLRequestJob> GetWeakPtr();

  // net::URLRequestJob methods:
  void Start() override;
  int ReadRawData(net::IOBuffer* buf, int buf_size) override;
  int GetResponseCode() const override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;

  // Resumes a previously started request that was delayed. If no
  // request has been started yet, then when Start() is called it will
  // not delay.
  void Resume();

 private:
  bool delayed_ = true;
  bool started_ = false;
  base::WeakPtrFactory<DelayableCertReportURLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DelayableCertReportURLRequestJob);
};

// A job interceptor that returns a failed, succesful or delayed request job.
// Used to simulate report uploads that fail, succeed or hang.
class CertReportJobInterceptor : public net::URLRequestInterceptor {
 public:
  CertReportJobInterceptor(ReportSendingResult expected_report_result,
                           const uint8_t* server_private_key);
  ~CertReportJobInterceptor() override;

  // net::URLRequestInterceptor method:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

  // Sets the failure mode for reports. Must be called on the UI thread.
  void SetFailureMode(ReportSendingResult expected_report_result);
  // Resumes any hanging URL request. Must be called on the UI thread.
  void Resume();

  // These must be called on the UI thread.
  const std::set<std::string>& successful_reports() const;
  const std::set<std::string>& failed_reports() const;
  const std::set<std::string>& delayed_reports() const;
  void ClearObservedReports();

 private:
  void SetFailureModeOnIOThread(ReportSendingResult expected_report_result);
  void ResumeOnIOThread();
  void RequestCreated(const std::string& uploaded_report,
                      ReportSendingResult expected_report_result);

  std::set<std::string> successful_reports_;
  std::set<std::string> failed_reports_;
  std::set<std::string> delayed_reports_;

  ReportSendingResult expected_report_result_;

  // Private key to decrypt certificate reports.
  const uint8_t* server_private_key_;

  mutable base::WeakPtr<DelayableCertReportURLRequestJob> delayed_request_ =
      nullptr;
  mutable base::WeakPtrFactory<CertReportJobInterceptor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CertReportJobInterceptor);
};

// A network delegate used to observe URL request destructions. The tests check
// that no outstanding URL request is present during tear down.
class CertificateReportingServiceTestNetworkDelegate
    : public net::NetworkDelegateImpl {
 public:
  CertificateReportingServiceTestNetworkDelegate(
      const base::Callback<void()>& url_request_destroyed_callback);
  ~CertificateReportingServiceTestNetworkDelegate() override;

  // net::NetworkDelegate method:
  void OnURLRequestDestroyed(net::URLRequest* request) override;

 private:
  base::Callback<void()> url_request_destroyed_callback_;
};

// Base class for CertificateReportingService tests. Sets up an interceptor to
// keep track of reports that are being sent.
class CertificateReportingServiceTestBase {
 protected:
  CertificateReportingServiceTestBase();
  virtual ~CertificateReportingServiceTestBase();

  // Syntactic sugar for wrapping report expectations in a more readable way.
  // Passed to WaitForRequestDeletions() as input.
  // Example:
  // The following expects report0 and report1 to be successfully sent and their
  // URL requests to be deleted:
  // WaitForRequestDeletions(
  //     ReportExpectation::Successful("report0, report1"));
  struct ReportExpectation {
    ReportExpectation();
    ReportExpectation(const ReportExpectation& other);
    ~ReportExpectation();
    // Returns an expectation where all reports in |reports| succeed.
    static ReportExpectation Successful(const std::set<std::string>& reports);
    // Returns an expectation where all reports in |reports| fail.
    static ReportExpectation Failed(const std::set<std::string>& reports);
    // Returns an expectation where all reports in |reports| are delayed.
    static ReportExpectation Delayed(const std::set<std::string>& reports);
    std::set<std::string> successful_reports;
    std::set<std::string> failed_reports;
    std::set<std::string> delayed_reports;
  };

  void SetUpInterceptor();
  void TearDownInterceptor();

  // Changes the behavior of report uploads to fail, succeed or hang.
  void SetFailureMode(ReportSendingResult expected_report_result);

  // Resumes delayed report request. Failure mode should be REPORTS_DELAY when
  // calling this method.
  void ResumeDelayedRequest();

  // Waits for the URL requests for the expected reports to be destroyed.
  // Doesn't block if all requests have already been destroyed.
  void WaitForRequestsDestroyed(const ReportExpectation& expectation);

  uint8_t* server_public_key();
  uint32_t server_public_key_version() const;

  net::NetworkDelegate* network_delegate();

  CertReportJobInterceptor* interceptor() { return url_request_interceptor_; }

 private:
  void SetUpInterceptorOnIOThread(
      std::unique_ptr<net::URLRequestInterceptor> url_request_interceptor);
  void TearDownInterceptorOnIOThread();
  void OnURLRequestDestroyed();

  CertReportJobInterceptor* url_request_interceptor_;

  uint8_t server_public_key_[32];
  uint8_t server_private_key_[32];

  std::unique_ptr<CertificateReportingServiceTestNetworkDelegate>
      network_delegate_;

  int num_request_deletions_to_wait_for_;
  int num_deleted_requests_;
  std::unique_ptr<base::RunLoop> run_loop_;
  DISALLOW_COPY_AND_ASSIGN(CertificateReportingServiceTestBase);
};

}  // namespace certificate_reporting_test_utils

#endif  // CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_TEST_UTILS_H_
