// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/test_url_request_context_getter.h"

#include "content/browser/browser_thread.h"
#include "net/url_request/url_request_test_util.h"

TestURLRequestContextGetter::TestURLRequestContextGetter() {}

TestURLRequestContextGetter::~TestURLRequestContextGetter() {}

net::URLRequestContext* TestURLRequestContextGetter::GetURLRequestContext() {
  if (!context_)
    context_ = new TestURLRequestContext();
  return context_.get();
}

scoped_refptr<base::MessageLoopProxy>
TestURLRequestContextGetter::GetIOMessageLoopProxy() const {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
}
