// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/url_request_mock_util.h"

#include <string>

#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/net/url_request_failed_dns_job.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/browser/net/url_request_mock_link_doctor_job.h"
#include "chrome/browser/net/url_request_slow_download_job.h"
#include "chrome/browser/net/url_request_slow_http_job.h"
#include "chrome/common/chrome_paths.h"
#include "content/browser/browser_thread.h"
#include "net/url_request/url_request_filter.h"

namespace chrome_browser_net {

void SetUrlRequestMocksEnabled(bool enabled) {
  // Since this involves changing the net::URLRequest ProtocolFactory, we need
  // to run on the IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (enabled) {
    // We have to look around for our helper files, but we only use
    // this from tests, so allow these IO operations to happen
    // anywhere.
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    net::URLRequestFilter::GetInstance()->ClearHandlers();

    URLRequestFailedDnsJob::AddUrlHandler();
    URLRequestMockLinkDoctorJob::AddUrlHandler();
    URLRequestSlowDownloadJob::AddUrlHandler();

    FilePath root_http;
    PathService::Get(chrome::DIR_TEST_DATA, &root_http);
    URLRequestMockHTTPJob::AddUrlHandler(root_http);
    URLRequestSlowHTTPJob::AddUrlHandler(root_http);
  } else {
    // Revert to the default handlers.
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }
}

}  // namespace chrome_browser_net
