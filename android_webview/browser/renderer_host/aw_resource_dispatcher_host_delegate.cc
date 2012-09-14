// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/renderer_host/aw_resource_dispatcher_host_delegate.h"

#include "android_webview/browser/aw_login_delegate.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_dispatcher_host_login_delegate.h"
#include "content/public/browser/resource_throttle.h"

namespace {

base::LazyInstance<android_webview::AwResourceDispatcherHostDelegate>
    g_webview_resource_dispatcher_host_delegate = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace android_webview {

// static
void AwResourceDispatcherHostDelegate::ResourceDispatcherHostCreated() {
  content::ResourceDispatcherHost::Get()->SetDelegate(
      &g_webview_resource_dispatcher_host_delegate.Get());
}

AwResourceDispatcherHostDelegate::AwResourceDispatcherHostDelegate()
    : content::ResourceDispatcherHostDelegate() {
}

AwResourceDispatcherHostDelegate::~AwResourceDispatcherHostDelegate() {
}

void AwResourceDispatcherHostDelegate::RequestBeginning(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    ResourceType::Type resource_type,
    int child_id,
    int route_id,
    bool is_continuation_of_transferred_request,
    ScopedVector<content::ResourceThrottle>* throttles) {
}

bool AwResourceDispatcherHostDelegate::AcceptAuthRequest(
    net::URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  return true;
}

content::ResourceDispatcherHostLoginDelegate*
    AwResourceDispatcherHostDelegate::CreateLoginDelegate(
        net::AuthChallengeInfo* auth_info,
        net::URLRequest* request) {
  return new AwLoginDelegate(auth_info, request);
}

}
