// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/blob_url_request_job_factory.h"

#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/chrome_blob_storage_context.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "webkit/blob/blob_storage_controller.h"
#include "webkit/blob/blob_url_request_job.h"

namespace {

net::URLRequestJob* BlobURLRequestJobFactory(net::URLRequest* request,
                                             const std::string& scheme) {
  scoped_refptr<webkit_blob::BlobData> data;
  ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);
  if (info) {
    // Resource dispatcher host already looked up the blob data.
    data = info->requested_blob_data();
  } else {
    // This request is not coming thru resource dispatcher host.
    data  = static_cast<ChromeURLRequestContext*>(request->context())->
        blob_storage_context()->
            controller()->GetBlobDataFromUrl(request->url());
  }
  return new webkit_blob::BlobURLRequestJob(
      request, data,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
}

}

void RegisterBlobURLRequestJobFactory() {
  net::URLRequest::RegisterProtocolFactory(chrome::kBlobScheme,
                                      &BlobURLRequestJobFactory);
}
