// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NET_REPORTING_SERVICE_PROXY_H_
#define CONTENT_BROWSER_NET_REPORTING_SERVICE_PROXY_H_

#include "third_party/WebKit/public/platform/reporting.mojom.h"

namespace service_manager {
struct BindSourceInfo;
}  // namespace service_manager

namespace content {

class StoragePartition;

void CreateReportingServiceProxy(
    StoragePartition* storage_partition,
    const service_manager::BindSourceInfo& source_info,
    mojom::ReportingServiceProxyRequest request);

}  // namespace content

#endif  // CONTENT_BROWSER_NET_REPORTING_SERVICE_PROXY_H_
