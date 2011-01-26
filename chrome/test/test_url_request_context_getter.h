// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TEST_URL_REQUEST_CONTEXT_GETTER_H_
#define CHROME_TEST_TEST_URL_REQUEST_CONTEXT_GETTER_H_
#pragma once

#include "base/ref_counted.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"

// Used to return a dummy context (normally the context is on the IO thread).
// The one here can be run on the main test thread. Note that this can lead to
// a leak if your test does not have a BrowserThread::IO in it because
// URLRequestContextGetter is defined as a ReferenceCounted object with a
// special trait that deletes it on the IO thread.
class TestURLRequestContextGetter : public URLRequestContextGetter {
 public:
  virtual net::URLRequestContext* GetURLRequestContext() {
    if (!context_)
      context_ = new TestURLRequestContext();
    return context_.get();
  }
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const {
    return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
  }

 private:
  scoped_refptr<net::URLRequestContext> context_;
};

#endif  // CHROME_TEST_TEST_URL_REQUEST_CONTEXT_GETTER_H_
