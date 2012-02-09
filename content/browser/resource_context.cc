// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/resource_context.h"

#include "base/logging.h"
#include "content/browser/plugin_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/database/database_tracker.h"

namespace content {

ResourceContext::~ResourceContext() {
  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
    // Band-aid for http://crbug.com/94704 until we change plug-in channel
    // requests to be owned by the ResourceContext.
    PluginProcessHost::CancelPendingRequestsForResourceContext(this);
  }
}

}  // namespace content
