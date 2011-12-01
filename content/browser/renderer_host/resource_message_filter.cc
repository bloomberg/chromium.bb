// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/resource_message_filter.h"

#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/resource_context.h"
#include "content/public/browser/browser_thread.h"

ResourceMessageFilter::ResourceMessageFilter(
    int child_id,
    content::ProcessType process_type,
    const content::ResourceContext* resource_context,
    URLRequestContextSelector* url_request_context_selector,
    ResourceDispatcherHost* resource_dispatcher_host)
    : child_id_(child_id),
      process_type_(process_type),
      resource_context_(resource_context),
      url_request_context_selector_(url_request_context_selector),
      resource_dispatcher_host_(resource_dispatcher_host) {
  DCHECK(resource_context);
  DCHECK(url_request_context_selector);
}

ResourceMessageFilter::~ResourceMessageFilter() {
}

void ResourceMessageFilter::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();

  // Unhook us from all pending network requests so they don't get sent to a
  // deleted object.
  resource_dispatcher_host_->CancelRequestsForProcess(child_id_);
}

bool ResourceMessageFilter::OnMessageReceived(const IPC::Message& message,
                                              bool* message_was_ok) {
  return resource_dispatcher_host_->OnMessageReceived(
      message, this, message_was_ok);
}

net::URLRequestContext* ResourceMessageFilter::GetURLRequestContext(
    ResourceType::Type type) {
  return url_request_context_selector_->GetRequestContext(type);
}
