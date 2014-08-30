// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SHELL_BROWSER_CAST_BROWSER_PROCESS_H_
#define CHROMECAST_SHELL_BROWSER_CAST_BROWSER_PROCESS_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace breakpad {
class CrashDumpManager;
}  // namespace breakpad

namespace chromecast {
class CastService;
class WebCryptoServer;

namespace metrics {
class CastMetricsHelper;
class CastMetricsServiceClient;
}  // namespace metrics

namespace shell {
class CastBrowserContext;
class RemoteDebuggingServer;

class CastBrowserProcess {
 public:
  // Gets the global instance of CastBrowserProcess. Does not create lazily and
  // assumes the instance already exists.
  static CastBrowserProcess* GetInstance();

  CastBrowserProcess();
  virtual ~CastBrowserProcess();

  void SetBrowserContext(CastBrowserContext* browser_context);
  void SetCastService(CastService* cast_service);
  void SetRemoteDebuggingServer(RemoteDebuggingServer* remote_debugging_server);
  void SetMetricsServiceClient(
      metrics::CastMetricsServiceClient* metrics_service_client);

  CastBrowserContext* browser_context() const { return browser_context_.get(); }
  CastService* cast_service() const { return cast_service_.get(); }
  metrics::CastMetricsServiceClient* metrics_service_client() const {
    return metrics_service_client_.get();
  }

 private:
  scoped_ptr<CastBrowserContext> browser_context_;
  scoped_ptr<metrics::CastMetricsServiceClient> metrics_service_client_;
  scoped_ptr<RemoteDebuggingServer> remote_debugging_server_;

  // Note: CastService must be destroyed before others.
  scoped_ptr<CastService> cast_service_;

  DISALLOW_COPY_AND_ASSIGN(CastBrowserProcess);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_SHELL_BROWSER_CAST_BROWSER_PROCESS_H_
