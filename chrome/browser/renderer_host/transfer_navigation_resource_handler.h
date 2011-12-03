// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_TRANSFER_NAVIGATION_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_RENDERER_HOST_TRANSFER_NAVIGATION_RESOURCE_HANDLER_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "content/browser/renderer_host/resource_handler.h"

namespace content {
struct ResourceResponse;
}

namespace net {
class URLRequest;
}

class ResourceDispatcherHost;

// This ResourceHandler checks whether a navigation redirect will cause a
// renderer process swap. When that happens, we remember the request so
// that we can transfer it to be handled by the new renderer. This fixes
// http://crbug.com/79520
class TransferNavigationResourceHandler : public ResourceHandler {
 public:
  TransferNavigationResourceHandler(
        ResourceHandler* handler,
        ResourceDispatcherHost* resource_dispatcher_host,
        net::URLRequest* request);

  // ResourceHandler implementation:
  virtual bool OnUploadProgress(
      int request_id, uint64 position, uint64 size) OVERRIDE;
  virtual bool OnRequestRedirected(
      int request_id, const GURL& new_url, content::ResourceResponse* response,
      bool* defer) OVERRIDE;
  virtual bool OnResponseStarted(
      int request_id, content::ResourceResponse* response) OVERRIDE;
  virtual bool OnWillStart(
      int request_id, const GURL& url, bool* defer) OVERRIDE;
  virtual bool OnWillRead(
      int request_id, net::IOBuffer** buf, int* buf_size,
      int min_size) OVERRIDE;
  virtual bool OnReadCompleted(int request_id, int* bytes_read) OVERRIDE;
  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info) OVERRIDE;
  virtual void OnRequestClosed() OVERRIDE;

 private:
  virtual ~TransferNavigationResourceHandler();

  scoped_refptr<ResourceHandler> next_handler_;
  ResourceDispatcherHost* rdh_;
  net::URLRequest* request_;

  DISALLOW_COPY_AND_ASSIGN(TransferNavigationResourceHandler);
};


#endif  // CHROME_BROWSER_RENDERER_HOST_TRANSFER_NAVIGATION_RESOURCE_HANDLER_H_
