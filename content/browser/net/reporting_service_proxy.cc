// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/reporting_service_proxy.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/WebKit/public/platform/reporting.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

class ReportingServiceProxyImpl : public mojom::ReportingServiceProxy {
 public:
  ReportingServiceProxyImpl(
      scoped_refptr<net::URLRequestContextGetter> request_context_getter)
      : request_context_getter_(std::move(request_context_getter)) {}

  // mojom::ReportingServiceProxy:
  void QueueReport(const GURL& url,
                   const std::string& group,
                   const std::string& type,
                   std::unique_ptr<base::Value> body) override {
    std::unique_ptr<const base::Value> const_body =
        base::WrapUnique(body.release());

    net::URLRequestContext* request_context =
        request_context_getter_->GetURLRequestContext();
    if (!request_context) {
      net::ReportingReport::RecordReportDiscardedForNoURLRequestContext();
      return;
    }

    net::ReportingService* reporting_service =
        request_context->reporting_service();
    if (!reporting_service) {
      net::ReportingReport::RecordReportDiscardedForNoReportingService();
      return;
    }

    reporting_service->QueueReport(url, group, type, std::move(const_body));
  }

 private:
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
};

void CreateReportingServiceProxyOnNetworkTaskRunner(
    mojom::ReportingServiceProxyRequest request,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter) {
  mojo::MakeStrongBinding(base::MakeUnique<ReportingServiceProxyImpl>(
                              std::move(request_context_getter)),
                          std::move(request));
}

}  // namespace

// static
void CreateReportingServiceProxy(
    StoragePartition* storage_partition,
    mojom::ReportingServiceProxyRequest request) {
  scoped_refptr<net::URLRequestContextGetter> request_context_getter(
      storage_partition->GetURLRequestContext());
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner(
      request_context_getter->GetNetworkTaskRunner());
  network_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateReportingServiceProxyOnNetworkTaskRunner,
                     std::move(request), std::move(request_context_getter)));
}

}  // namespace content
