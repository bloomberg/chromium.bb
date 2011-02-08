// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TEST_URL_REQUEST_CONTEXT_GETTER_H_
#define CHROME_TEST_TEST_URL_REQUEST_CONTEXT_GETTER_H_
#pragma once

#include "base/ref_counted.h"
#include "chrome/common/net/url_request_context_getter.h"

namespace base {
class MessageLoopProxy;
}

// Used to return a dummy context (normally the context is on the IO thread).
// The one here can be run on the main test thread. Note that this can lead to
// a leak if your test does not have a BrowserThread::IO in it because
// URLRequestContextGetter is defined as a ReferenceCounted object with a
// special trait that deletes it on the IO thread.
class TestURLRequestContextGetter : public URLRequestContextGetter {
 public:
  TestURLRequestContextGetter();
  virtual ~TestURLRequestContextGetter();

  // URLRequestContextGetter:
  virtual net::URLRequestContext* GetURLRequestContext();
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const;

 private:
  scoped_refptr<net::URLRequestContext> context_;
};

#endif  // CHROME_TEST_TEST_URL_REQUEST_CONTEXT_GETTER_H_
