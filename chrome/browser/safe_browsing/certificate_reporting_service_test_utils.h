// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_TEST_UTILS_H_
#define CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_TEST_UTILS_H_

#include <unordered_map>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/non_thread_safe.h"
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

enum RetryStatus {
  NOT_RETRIED = 0,
  RETRIED = 1,
};

typedef std::unordered_map<std::string, RetryStatus> ObservedReportMap;

// Syntactic sugar for wrapping report expectations in a more readable way.
// Passed to WaitForRequestDeletions() as input.
// Example:
// The following expects report0 and report1 to be successfully sent and their
// URL requests to be deleted:
// WaitForRequestsDestroyed(
//     ReportExpectation::Successful("report0, report1"));
struct ReportExpectation {
  ReportExpectation();
  ReportExpectation(const ReportExpectation& other);
  ~ReportExpectation();
  // Returns an expectation where all reports in |reports| succeed.
  static ReportExpectation Successful(const ObservedReportMap& reports);
  // Returns an expectation where all reports in |reports| fail.
  static ReportExpectation Failed(const ObservedReportMap& reports);
  // Returns an expectation where all reports in |reports| are delayed.
  static ReportExpectation Delayed(const ObservedReportMap& reports);
  // Total number of reports expected.
  int num_reports() const;
  ObservedReportMap successful_reports;
  ObservedReportMap failed_reports;
  ObservedReportMap delayed_reports;
};

// Failure mode of the report sending attempts.
enum ReportSendingResult {
  // Report send attempts should be successful.
  REPORTS_SUCCESSFUL,
  // Report send attempts should fail.
  REPORTS_FAIL,
  // Report send attempts should hang until explicitly resumed.
  REPORTS_DELAY,
};

// Helper class to wait for a number of events (e.g. request destroyed, report
// observed).
class RequestObserver {
 public:
  RequestObserver();
  ~RequestObserver();

  // Waits for |num_request| requests to be created or destroyed, depending on
  // whichever one this class observes.
  void Wait(unsigned int num_events_to_wait_for);

  // Called when a request created or destroyed, depending on whichever one this
  // class observes.
  void OnRequest(const std::string& serialized_report,
                 ReportSendingResult report_type);

  // These must be called on the UI thread.
  const ObservedReportMap& successful_reports() const;
  const ObservedReportMap& failed_reports() const;
  const ObservedReportMap& delayed_reports() const;
  void ClearObservedReports();

 private:
  unsigned int num_events_to_wait_for_;
  unsigned int num_received_events_;
  std::unique_ptr<base::RunLoop> run_loop_;

  ObservedReportMap successful_reports_;
  ObservedReportMap failed_reports_;
  ObservedReportMap delayed_reports_;
};

// A URLRequestJob that can be delayed until Resume() is called. Returns an
// empty response. If Resume() is called before a request is made, then the
// request will not be delayed. If not delayed, it can return a failed or a
// successful URL request job.
class DelayableCertReportURLRequestJob : public net::URLRequestJob,
                                         public base::NonThreadSafe {
 public:
  DelayableCertReportURLRequestJob(
      bool delayed,
      bool should_fail,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const base::Callback<void()>& destruction_callback);
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
  bool delayed_;
  bool should_fail_;
  bool started_;
  base::Callback<void()> destruction_callback_;
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
  // Resumes any hanging URL request and runs callback when the request
  // is resumed (i.e. response starts). Must be called on the UI thread.
  void Resume();

  RequestObserver* request_created_observer() const;
  RequestObserver* request_destroyed_observer() const;

 private:
  void SetFailureModeOnIOThread(ReportSendingResult expected_report_result);
  void ResumeOnIOThread();
  void RequestCreated(const std::string& serialized_report,
                      ReportSendingResult expected_report_result) const;
  void RequestDestructed(const std::string& serialized_report,
                         ReportSendingResult expected_report_result) const;

  mutable std::set<std::string> successful_reports_;
  mutable std::set<std::string> failed_reports_;
  mutable std::set<std::string> delayed_reports_;

  ReportSendingResult expected_report_result_;

  // Private key to decrypt certificate reports.
  const uint8_t* server_private_key_;

  mutable RequestObserver request_created_observer_;
  mutable RequestObserver request_destroyed_observer_;

  mutable base::WeakPtr<DelayableCertReportURLRequestJob> delayed_request_ =
      nullptr;
  mutable base::WeakPtrFactory<CertReportJobInterceptor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CertReportJobInterceptor);
};

// Class to wait for the CertificateReportingService to reset.
class CertificateReportingServiceObserver {
 public:
  CertificateReportingServiceObserver();
  ~CertificateReportingServiceObserver();

  // Clears the state of the observer. Must be called before waiting each time.
  void Clear();

  // Waits for the service to reset.
  void WaitForReset();

  // Must be called when the service is reset.
  void OnServiceReset();

 private:
  bool did_reset_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Base class for CertificateReportingService tests. Sets up an interceptor to
// keep track of reports that are being sent.
class CertificateReportingServiceTestHelper {
 public:
  CertificateReportingServiceTestHelper();
  ~CertificateReportingServiceTestHelper();

  void SetUpInterceptor();

  // Changes the behavior of report uploads to fail, succeed or hang.
  void SetFailureMode(ReportSendingResult expected_report_result);

  // Resumes delayed report request. Failure mode should be REPORTS_DELAY when
  // calling this method.
  void ResumeDelayedRequest();

  void WaitForRequestsCreated(const ReportExpectation& expectation);
  void WaitForRequestsDestroyed(const ReportExpectation& expectation);

  // Checks that all requests are destroyed and that there are no in-flight
  // reports in |service|.
  void ExpectNoRequests(CertificateReportingService* service);

  uint8_t* server_public_key();
  uint32_t server_public_key_version() const;

 private:
  CertReportJobInterceptor* interceptor() { return url_request_interceptor_; }
  void SetUpInterceptorOnIOThread();

  CertReportJobInterceptor* url_request_interceptor_;

  uint8_t server_public_key_[32];
  uint8_t server_private_key_[32];

  DISALLOW_COPY_AND_ASSIGN(CertificateReportingServiceTestHelper);
};

}  // namespace certificate_reporting_test_utils

#endif  // CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_TEST_UTILS_H_
