// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#include "content/common/resource_dispatcher.h"

#include "base/compiler_specific.h"

// ResourceDispatcher ---------------------------------------------------------

ResourceDispatcher::ResourceDispatcher(IPC::Message::Sender* sender)
    : message_sender_(sender),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
}

ResourceDispatcher::~ResourceDispatcher() {
}

// ResourceDispatcher implementation ------------------------------------------

bool ResourceDispatcher::OnMessageReceived(const IPC::Message& message) {
  return false;
}

webkit_glue::ResourceLoaderBridge* ResourceDispatcher::CreateBridge(
    const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info) {
  // This function is used only by plugin and renderer code, so it should
  // never be called in a 64-bit Windows process.
  NOTREACHED();
  return NULL;
}
