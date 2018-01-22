// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_CRONET_GLOBAL_STATE_H_
#define COMPONENTS_CRONET_CRONET_GLOBAL_STATE_H_

#include <memory>
#include <string>
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"

namespace net {
class NetLog;
class ProxyConfigService;
class ProxyResolutionService;
}  // namespace net

namespace cronet {

// Returns true when running on initialization thread.
// Only callable after initialization thread is started.
bool OnInitThread();

// Creates a proxy config service appropriate for this platform that fetches the
// system proxy settings.
std::unique_ptr<net::ProxyConfigService> CreateProxyConfigService(
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner);

// Creates a proxy service appropriate for this platform that fetches the
// system proxy settings.
std::unique_ptr<net::ProxyResolutionService> CreateProxyService(
    std::unique_ptr<net::ProxyConfigService> proxy_config_service,
    net::NetLog* net_log);

}  // namespace cronet

#endif  // COMPONENTS_CRONET_CRONET_GLOBAL_STATE_H_
