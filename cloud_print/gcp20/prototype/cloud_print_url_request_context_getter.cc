// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/cloud_print_url_request_context_getter.h"

#include "net/proxy/proxy_config_service_fixed.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

CloudPrintURLRequestContextGetter::CloudPrintURLRequestContextGetter(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner.get());
  network_task_runner_ = task_runner;
}

CloudPrintURLRequestContextGetter::~CloudPrintURLRequestContextGetter() {
}

net::URLRequestContext*
CloudPrintURLRequestContextGetter::GetURLRequestContext() {
  if (!context_) {
    net::URLRequestContextBuilder builder;
#if defined(OS_LINUX) || defined(OS_ANDROID)
    builder.set_proxy_config_service(
        new net::ProxyConfigServiceFixed(net::ProxyConfig()));
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)
    context_.reset(builder.Build());
  }
  return context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
CloudPrintURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

