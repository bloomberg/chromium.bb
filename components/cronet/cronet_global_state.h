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

// Posts a task to run on initialization thread. Blocks until initialization
// thread is started.
void PostTaskToInitThread(const base::Location& posted_from,
                          base::OnceClosure task);

// Ensure that one time initialization of Cronet global state is done. Can be
// called from any thread. The initialization is performed on initialization
// thread.
void EnsureInitialized();

// Creates a proxy config service appropriate for this platform that fetches the
// system proxy settings.
std::unique_ptr<net::ProxyConfigService> CreateProxyConfigService(
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner);

// Creates a proxy service appropriate for this platform that fetches the
// system proxy settings.
std::unique_ptr<net::ProxyResolutionService> CreateProxyService(
    std::unique_ptr<net::ProxyConfigService> proxy_config_service,
    net::NetLog* net_log);

// Creates default User-Agent request value, combining optional
// |partial_user_agent| with system-dependent values.
std::string CreateDefaultUserAgent(const std::string& partial_user_agent);

}  // namespace cronet

#endif  // COMPONENTS_CRONET_CRONET_GLOBAL_STATE_H_
