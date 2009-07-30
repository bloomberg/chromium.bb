// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/url_request_mock_util.h"

#include <string>

#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/task.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/url_request_failed_dns_job.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/browser/net/url_request_slow_download_job.h"
#include "chrome/browser/net/url_request_slow_http_job.h"
#include "chrome/common/chrome_paths.h"
#include "net/url_request/url_request_filter.h"

namespace {

// Helper class for making changes to the URLRequest ProtocolFactory on the
// IO thread.
class SetUrlRequestMocksEnabledTask : public Task {
 public:
  explicit SetUrlRequestMocksEnabledTask(bool enabled) : enabled_(enabled) {
  }

  virtual void Run() {
    if (enabled_) {
      URLRequestFilter::GetInstance()->ClearHandlers();

      URLRequestFailedDnsJob::AddUrlHandler();
      URLRequestSlowDownloadJob::AddUrlHandler();

      std::wstring root_http;
      PathService::Get(chrome::DIR_TEST_DATA, &root_http);
      URLRequestMockHTTPJob::AddUrlHandler(root_http);
      URLRequestSlowHTTPJob::AddUrlHandler(root_http);
    } else {
      // Revert to the default handlers.
      URLRequestFilter::GetInstance()->ClearHandlers();
    }
  }

 private:
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(SetUrlRequestMocksEnabledTask);
};

}  // namespace

namespace chrome_browser_net {

void SetUrlRequestMocksEnabled(bool enabled) {
  // Since this involves changing the URLRequest ProtocolFactory, we want to
  // run on the IO thread.
  g_browser_process->io_thread()->message_loop()->PostTask(FROM_HERE,
      new SetUrlRequestMocksEnabledTask(enabled));
}

}  // namespace chrome_browser_net
