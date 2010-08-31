// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/blob_url_request_job_factory.h"

#include "chrome/browser/chrome_blob_storage_context.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/url_constants.h"
#include "webkit/blob/blob_storage_controller.h"
#include "webkit/blob/blob_url_request_job.h"

namespace {

URLRequestJob* BlobURLRequestJobFactory(URLRequest* request,
                                        const std::string& scheme) {
  webkit_blob::BlobStorageController* blob_storage_controller =
      static_cast<ChromeURLRequestContext*>(request->context())->
          blob_storage_context()->controller();
  return new webkit_blob::BlobURLRequestJob(
      request,
      blob_storage_controller->GetBlobDataFromUrl(request->url()),
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE));
}

}

void RegisterBlobURLRequestJobFactory() {
  URLRequest::RegisterProtocolFactory(chrome::kBlobScheme,
                                      &BlobURLRequestJobFactory);
}
