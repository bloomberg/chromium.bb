// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/resource_dispatcher_delegate.h"

ResourceDispatcherDelegate::ResourceDispatcherDelegate() {
}

ResourceDispatcherDelegate::~ResourceDispatcherDelegate() {
}

webkit_glue::ResourceLoaderBridge::Peer*
    ResourceDispatcherDelegate::OnRequestComplete(
        webkit_glue::ResourceLoaderBridge::Peer* current_peer,
        ResourceType::Type resource_type,
        const net::URLRequestStatus& status) {
  return NULL;
}

webkit_glue::ResourceLoaderBridge::Peer*
    ResourceDispatcherDelegate::OnReceivedResponse(
        webkit_glue::ResourceLoaderBridge::Peer* current_peer,
        const std::string& mime_type,
        const GURL& url) {
  return NULL;
}
