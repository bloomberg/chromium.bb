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

// The next available browser-global navigation request ID.
static int64 next_navigation_request_id_ = 0;

void OnBeginNavigation(const NavigationRequestInfo& info,
                       scoped_refptr<ResourceRequestBody> request_body,
                       int64 navigation_request_id,
                       int64 frame_tree_node_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ResourceDispatcherHostImpl::Get()->StartNavigationRequest(
      info, request_body, navigation_request_id, frame_tree_node_id);
}

void CancelNavigationRequest(int64 navigation_request_id,
                             int64 frame_tree_node_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ResourceDispatcherHostImpl::Get()->CancelNavigationRequest(
      navigation_request_id, frame_tree_node_id);
}

}  // namespace

NavigationRequest::NavigationRequest(const NavigationRequestInfo& info,
                                     int64 frame_tree_node_id)
  : navigation_request_id_(++next_navigation_request_id_),
    info_(info),
    frame_tree_node_id_(frame_tree_node_id) {
}

NavigationRequest::~NavigationRequest() {
}

void NavigationRequest::BeginNavigation(
    scoped_refptr<ResourceRequestBody> request_body) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&OnBeginNavigation,
                 info_,
                 request_body,
                 navigation_request_id_,
                 frame_tree_node_id_));
}

void NavigationRequest::CancelNavigation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&CancelNavigationRequest,
                 navigation_request_id_, frame_tree_node_id_));
}

}  // namespace content
