// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/blob_url_request_job_factory.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/chrome_blob_storage_context.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "net/url_request/url_request_job_factory.h"
#include "webkit/blob/blob_storage_controller.h"
#include "webkit/blob/blob_url_request_job.h"

namespace {

class BlobProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  explicit BlobProtocolHandler(
      webkit_blob::BlobStorageController* blob_storage_controller);
  virtual ~BlobProtocolHandler();

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request) const OVERRIDE;

 private:
  // No scoped_refptr because |blob_storage_controller_| is owned by the
  // ProfileIOData, which also owns this ProtocolHandler.
  webkit_blob::BlobStorageController* const blob_storage_controller_;

  DISALLOW_COPY_AND_ASSIGN(BlobProtocolHandler);
};

BlobProtocolHandler::BlobProtocolHandler(
    webkit_blob::BlobStorageController* blob_storage_controller)
    : blob_storage_controller_(blob_storage_controller) {
  DCHECK(blob_storage_controller);
}

BlobProtocolHandler::~BlobProtocolHandler() {}

net::URLRequestJob* BlobProtocolHandler::MaybeCreateJob(
    net::URLRequest* request) const {
  scoped_refptr<webkit_blob::BlobData> data;
  ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);
  if (info) {
    // Resource dispatcher host already looked up the blob data.
    data = info->requested_blob_data();
  } else {
    // This request is not coming through resource dispatcher host.
    data = blob_storage_controller_->GetBlobDataFromUrl(request->url());
  }
  return new webkit_blob::BlobURLRequestJob(
      request, data,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
}

}  // namespace

net::URLRequestJobFactory::ProtocolHandler*
CreateBlobProtocolHandler(webkit_blob::BlobStorageController* controller) {
  DCHECK(controller);
  return new BlobProtocolHandler(controller);
}
