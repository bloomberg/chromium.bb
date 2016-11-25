// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service.h"
#include "content/public/browser/browser_thread.h"

namespace {
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
    base::TimeDelta report_ttl)
    : error_reporter_(std::move(error_reporter)),
      retry_list_(std::move(retry_list)),
      test_clock_(clock),
      report_ttl_(report_ttl),
      current_report_id_(0),
      weak_factory_(this) {}

CertificateReportingService::Reporter::~Reporter() {}

void CertificateReportingService::Reporter::Send(
    const std::string& serialized_report) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::Time now =
      test_clock_ ? test_clock_->Now() : base::Time::NowFromSystemTime();
  SendInternal(Report(current_report_id_++, now, serialized_report));
}

void CertificateReportingService::Reporter::SendPending() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::Time now =
      test_clock_ ? test_clock_->Now() : base::Time::NowFromSystemTime();
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
  auto it = inflight_reports_.find(report_id);
  DCHECK(it != inflight_reports_.end());
  retry_list_->Add(it->second);
  CHECK_GT(inflight_reports_.erase(report_id), 0u);
}

void CertificateReportingService::Reporter::SuccessCallback(int report_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  CHECK_GT(inflight_reports_.erase(report_id), 0u);
}
