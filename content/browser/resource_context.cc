// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/resource_context.h"

#include "base/logging.h"
#include "content/browser/browser_thread.h"
#include "webkit/database/database_tracker.h"

namespace content {

ResourceContext::ResourceContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

ResourceContext::~ResourceContext() {}

webkit_database::DatabaseTracker* ResourceContext::database_tracker() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  EnsureInitialized();
  return database_tracker_;
}

void ResourceContext::set_database_tracker(
    webkit_database::DatabaseTracker* database_tracker) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  database_tracker_ = database_tracker;
}

}  // namespace content
