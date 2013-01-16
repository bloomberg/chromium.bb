// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/url_data_source.h"

#include "content/public/browser/browser_thread.h"

namespace content {

MessageLoop* URLDataSource::MessageLoopForRequestPath(
    const std::string& path) const {
  return BrowserThread::UnsafeGetMessageLoopForThread(BrowserThread::UI);
}

bool URLDataSource::ShouldReplaceExistingSource() const {
  return true;
}

bool URLDataSource::AllowCaching() const {
  return true;
}

}  // namespace content
