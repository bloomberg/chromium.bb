// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/url_data_source_delegate.h"

#include "content/public/browser/browser_thread.h"

namespace content {

URLDataSourceDelegate::URLDataSourceDelegate()
    : url_data_source_(NULL) {
}

URLDataSourceDelegate::~URLDataSourceDelegate() {
}

MessageLoop* URLDataSourceDelegate::MessageLoopForRequestPath(
    const std::string& path) const {
  return BrowserThread::UnsafeGetMessageLoopForThread(BrowserThread::UI);
}

bool URLDataSourceDelegate::ShouldReplaceExistingSource() const {
  return true;
}

bool URLDataSourceDelegate::AllowCaching() const {
  return true;
}

}  // namespace content
