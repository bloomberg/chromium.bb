// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_service.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/reporting/reporting_browsing_data_remover.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_delegate.h"
#include "net/reporting/reporting_header_parser.h"
#include "net/reporting/reporting_uploader.h"
#include "url/gurl.h"

namespace net {

namespace {

class ReportingServiceImpl : public ReportingService {
 public:
  ReportingServiceImpl(std::unique_ptr<ReportingContext> context)
      : context_(std::move(context)) {}

  // ReportingService implementation:

  ~ReportingServiceImpl() override = default;

  void QueueReport(const GURL& url,
                   const std::string& group,
                   const std::string& type,
                   std::unique_ptr<const base::Value> body) override {
    DCHECK(context_);
    DCHECK(context_->delegate());

    if (!context_->delegate()->CanQueueReport(url::Origin::Create(url)))
      return;

    context_->cache()->AddReport(url, group, type, std::move(body),
                                 context_->tick_clock()->NowTicks(), 0);
  }

  void ProcessHeader(const GURL& url,
                     const std::string& header_value) override {
    ReportingHeaderParser::ParseHeader(context_.get(), url, header_value);
  }

  void RemoveBrowsingData(int data_type_mask,
                          const base::RepeatingCallback<bool(const GURL&)>&
                              origin_filter) override {
    ReportingBrowsingDataRemover::RemoveBrowsingData(
        context_->cache(), data_type_mask, origin_filter);
  }

  bool RequestIsUpload(const URLRequest& request) override {
    return context_->uploader()->RequestIsUpload(request);
  }

  const ReportingPolicy& GetPolicy() const override {
    return context_->policy();
  }

 private:
  std::unique_ptr<ReportingContext> context_;

  DISALLOW_COPY_AND_ASSIGN(ReportingServiceImpl);
};

}  // namespace

ReportingService::~ReportingService() = default;

// static
std::unique_ptr<ReportingService> ReportingService::Create(
    const ReportingPolicy& policy,
    URLRequestContext* request_context) {
  return std::make_unique<ReportingServiceImpl>(
      ReportingContext::Create(policy, request_context));
}

// static
std::unique_ptr<ReportingService> ReportingService::CreateForTesting(
    std::unique_ptr<ReportingContext> reporting_context) {
  return std::make_unique<ReportingServiceImpl>(std::move(reporting_context));
}

}  // namespace net
