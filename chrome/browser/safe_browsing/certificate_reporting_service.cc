// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service.h"
#include "content/public/browser/browser_thread.h"

namespace {

// URL to upload invalid certificate chain reports. An HTTP URL is used because
// a client seeing an invalid cert might not be able to make an HTTPS connection
// to report it.
const char kExtendedReportingUploadUrl[] =
    "http://safebrowsing.googleusercontent.com/safebrowsing/clientreport/";

// Compare function that orders Reports in reverse chronological order (i.e.
// oldest item is last).
bool ReportCompareFunc(const CertificateReportingService::Report& item1,
                       const CertificateReportingService::Report& item2) {
  return item1.creation_time > item2.creation_time;
}

}  // namespace

CertificateReportingService::BoundedReportList::BoundedReportList(
    size_t max_size)
    : max_size_(max_size) {
  CHECK(max_size <= 20)
      << "Current implementation is not efficient for a large list.";
  DCHECK(thread_checker_.CalledOnValidThread());
}

CertificateReportingService::BoundedReportList::~BoundedReportList() {}

void CertificateReportingService::BoundedReportList::Add(const Report& item) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(items_.size() <= max_size_);
  if (items_.size() == max_size_) {
    const Report& last = items_.back();
    if (item.creation_time <= last.creation_time) {
      // Report older than the oldest item in the queue, ignore.
      return;
    }
    // Reached the maximum item count, remove the oldest item.
    items_.pop_back();
  }
  items_.push_back(item);
  std::sort(items_.begin(), items_.end(), ReportCompareFunc);
}

void CertificateReportingService::BoundedReportList::Clear() {
  items_.clear();
}

const std::vector<CertificateReportingService::Report>&
CertificateReportingService::BoundedReportList::items() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return items_;
}

CertificateReportingService::Reporter::Reporter(
    std::unique_ptr<certificate_reporting::ErrorReporter> error_reporter,
    std::unique_ptr<BoundedReportList> retry_list,
    base::Clock* clock,
    base::TimeDelta report_ttl,
    bool retries_enabled)
    : error_reporter_(std::move(error_reporter)),
      retry_list_(std::move(retry_list)),
      clock_(clock),
      report_ttl_(report_ttl),
      retries_enabled_(retries_enabled),
      current_report_id_(0),
      weak_factory_(this) {}

CertificateReportingService::Reporter::~Reporter() {}

void CertificateReportingService::Reporter::Send(
    const std::string& serialized_report) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  SendInternal(Report(current_report_id_++, clock_->Now(), serialized_report));
}

void CertificateReportingService::Reporter::SendPending() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!retries_enabled_) {
    return;
  }
  const base::Time now = clock_->Now();
  // Copy pending reports and clear the retry list.
  std::vector<Report> items = retry_list_->items();
  retry_list_->Clear();
  for (const Report& report : items) {
    if (report.creation_time < now - report_ttl_) {
      // Report too old, ignore.
      continue;
    }
    SendInternal(report);
  }
}

size_t
CertificateReportingService::Reporter::inflight_report_count_for_testing()
    const {
  return inflight_reports_.size();
}

CertificateReportingService::BoundedReportList*
CertificateReportingService::Reporter::GetQueueForTesting() const {
  return retry_list_.get();
}

void CertificateReportingService::Reporter::SendInternal(
    const CertificateReportingService::Report& report) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  inflight_reports_.insert(std::make_pair(report.report_id, report));
  error_reporter_->SendExtendedReportingReport(
      report.serialized_report,
      base::Bind(&CertificateReportingService::Reporter::SuccessCallback,
                 weak_factory_.GetWeakPtr(), report.report_id),
      base::Bind(&CertificateReportingService::Reporter::ErrorCallback,
                 weak_factory_.GetWeakPtr(), report.report_id));
}

void CertificateReportingService::Reporter::ErrorCallback(int report_id,
                                                          const GURL& url,
                                                          int error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (retries_enabled_) {
    auto it = inflight_reports_.find(report_id);
    DCHECK(it != inflight_reports_.end());
    retry_list_->Add(it->second);
  }
  CHECK_GT(inflight_reports_.erase(report_id), 0u);
}

void CertificateReportingService::Reporter::SuccessCallback(int report_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  CHECK_GT(inflight_reports_.erase(report_id), 0u);
}

CertificateReportingService::CertificateReportingService(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
    uint8_t server_public_key[/* 32 */],
    uint32_t server_public_key_version,
    size_t max_queued_report_count,
    base::TimeDelta max_report_age,
    std::unique_ptr<base::Clock> clock)
    : enabled_(true),
      url_request_context_(nullptr),
      max_queued_report_count_(max_queued_report_count),
      max_report_age_(max_report_age),
      clock_(std::move(clock)),
      made_send_attempt_(false),
      server_public_key_(server_public_key),
      server_public_key_version_(server_public_key_version) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&CertificateReportingService::InitializeOnIOThread,
                 base::Unretained(this), enabled_, url_request_context_getter,
                 max_queued_report_count_, max_report_age_, clock_.get(),
                 server_public_key_, server_public_key_version_));
}

CertificateReportingService::~CertificateReportingService() {
  DCHECK(!reporter_);
}

void CertificateReportingService::Shutdown() {
  // Shutdown will be called twice: Once after SafeBrowsing shuts down, and once
  // when all KeyedServices shut down. All calls after the first one are no-op.
  enabled_ = false;
  Reset();
}

void CertificateReportingService::Send(const std::string& serialized_report) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  made_send_attempt_ = true;
  if (!reporter_) {
    return;
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&CertificateReportingService::Reporter::Send,
                 base::Unretained(reporter_.get()), serialized_report));
}

void CertificateReportingService::SendPending() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  made_send_attempt_ = true;
  if (!reporter_) {
    return;
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&CertificateReportingService::Reporter::SendPending,
                 base::Unretained(reporter_.get())));
}

void CertificateReportingService::InitializeOnIOThread(
    bool enabled,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
    size_t max_queued_report_count,
    base::TimeDelta max_report_age,
    base::Clock* clock,
    uint8_t* server_public_key,
    uint32_t server_public_key_version) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!url_request_context_);
  url_request_context_ = url_request_context_getter->GetURLRequestContext();
  ResetOnIOThread(enabled, url_request_context_, max_queued_report_count,
                  max_report_age, clock, server_public_key,
                  server_public_key_version);
}

void CertificateReportingService::SetEnabled(bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  enabled_ = enabled;
  Reset();
}

CertificateReportingService::Reporter*
CertificateReportingService::GetReporterForTesting() const {
  return reporter_.get();
}

void CertificateReportingService::SetMaxQueuedReportCountForTesting(
    size_t count) {
  DCHECK(!made_send_attempt_);
  max_queued_report_count_ = count;
  Reset();
}

void CertificateReportingService::SetClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  DCHECK(!made_send_attempt_);
  clock_ = std::move(clock);
  Reset();
}

void CertificateReportingService::SetMaxReportAgeForTesting(
    base::TimeDelta max_report_age) {
  DCHECK(!made_send_attempt_);
  max_report_age_ = max_report_age;
  Reset();
}

// static
GURL CertificateReportingService::GetReportingURLForTesting() {
  return GURL(kExtendedReportingUploadUrl);
}

void CertificateReportingService::Reset() {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&CertificateReportingService::ResetOnIOThread,
                 base::Unretained(this), enabled_, url_request_context_,
                 max_queued_report_count_, max_report_age_, clock_.get(),
                 server_public_key_, server_public_key_version_));
}

void CertificateReportingService::ResetOnIOThread(
    bool enabled,
    net::URLRequestContext* url_request_context,
    size_t max_queued_report_count,
    base::TimeDelta max_report_age,
    base::Clock* clock,
    uint8_t* const server_public_key,
    uint32_t server_public_key_version) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // url_request_context_ is null during shutdown.
  if (!enabled || !url_request_context) {
    reporter_.reset(nullptr);
    return;
  }
  std::unique_ptr<certificate_reporting::ErrorReporter> error_reporter;
  if (server_public_key) {
    // Only used in tests.
    std::unique_ptr<net::ReportSender> report_sender(new net::ReportSender(
        url_request_context, net::ReportSender::DO_NOT_SEND_COOKIES));
    error_reporter.reset(new certificate_reporting::ErrorReporter(
        GURL(kExtendedReportingUploadUrl), server_public_key,
        server_public_key_version, std::move(report_sender)));
  } else {
    error_reporter.reset(new certificate_reporting::ErrorReporter(
        url_request_context, GURL(kExtendedReportingUploadUrl),
        net::ReportSender::DO_NOT_SEND_COOKIES));
  }

  reporter_.reset(
      new Reporter(std::move(error_reporter),
                   std::unique_ptr<BoundedReportList>(
                       new BoundedReportList(max_queued_report_count)),
                   clock, max_report_age, true /* retries_enabled */));
}
