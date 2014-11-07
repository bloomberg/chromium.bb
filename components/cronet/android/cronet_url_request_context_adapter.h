// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_CRONET_URL_REQUEST_CONTEXT_ADAPTER_H_
#define COMPONENTS_CRONET_ANDROID_CRONET_URL_REQUEST_CONTEXT_ADAPTER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "net/base/net_log.h"
#include "net/base/network_change_notifier.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class NetLogLogger;
class URLRequestContext;
}  // namespace net

namespace cronet {

struct URLRequestContextConfig;

// Adapter between Java CronetUrlRequestContext and net::URLRequestContext.
class CronetURLRequestContextAdapter {
 public:
  CronetURLRequestContextAdapter();

  ~CronetURLRequestContextAdapter();

  void Initialize(scoped_ptr<URLRequestContextConfig> config,
                  const base::Closure& java_init_network_thread);

  // Releases all resources for the request context and deletes the object.
  // Blocks until network thread is destroyed after running all pending tasks.
  void Destroy();

  net::URLRequestContext* GetURLRequestContext();

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner() const;

  void StartNetLogToFile(const std::string& file_name);

  void StopNetLog();

 private:
  // Network thread is owned by |this|, but is destroyed from java thread.
  base::Thread* network_thread_;
  // |net_log_logger_| and |context_| should only be accessed on network thread.
  scoped_ptr<net::NetLogLogger> net_log_logger_;
  scoped_ptr<net::URLRequestContext> context_;

  // Initializes |context_| on the Network thread.
  void InitializeOnNetworkThread(
      scoped_ptr<URLRequestContextConfig> config,
      const base::Closure& java_init_network_thread);
  void StartNetLogToFileOnNetworkThread(const std::string& file_name);
  void StopNetLogOnNetworkThread();

  DISALLOW_COPY_AND_ASSIGN(CronetURLRequestContextAdapter);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_CRONET_URL_REQUEST_CONTEXT_ADAPTER_H_
