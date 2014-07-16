// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_request.h"

#include "base/logging.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/common/resource_request_body.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

void OnBeginNavigation(const NavigationRequestInfo& info,
                       scoped_refptr<ResourceRequestBody> request_body,
                       int64 frame_node_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ResourceDispatcherHostImpl::Get()->NavigationRequest(
      info, request_body, frame_node_id);
}

}  // namespace

NavigationRequest::NavigationRequest(const NavigationRequestInfo& info,
                                     int64 frame_node_id)
  : info_(info),
    frame_node_id_(frame_node_id) {
}

NavigationRequest::~NavigationRequest() {
  // TODO(clamy): Cancel the corresponding request in ResourceDispatcherHost if
  // it has not commited yet.
}

void NavigationRequest::BeginNavigation(
    scoped_refptr<ResourceRequestBody> request_body) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&OnBeginNavigation, info_, request_body, frame_node_id_));
}

}  // namespace content
