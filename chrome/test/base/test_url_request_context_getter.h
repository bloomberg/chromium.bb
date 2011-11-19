// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TEST_URL_REQUEST_CONTEXT_GETTER_H_
#define CHROME_TEST_BASE_TEST_URL_REQUEST_CONTEXT_GETTER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "net/url_request/url_request_context_getter.h"

namespace base {
class MessageLoopProxy;
}

// Used to return a dummy context (normally the context is on the IO thread).
// The one here can be run on the main test thread. Note that this can lead to
// a leak if your test does not have a BrowserThread::IO in it because
// URLRequestContextGetter is defined as a ReferenceCounted object with a
// special trait that deletes it on the IO thread.
class TestURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  TestURLRequestContextGetter();
  virtual ~TestURLRequestContextGetter();

  // net::URLRequestContextGetter implementation.
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::MessageLoopProxy>
      GetIOMessageLoopProxy() const OVERRIDE;

 private:
  scoped_refptr<net::URLRequestContext> context_;
};

#endif  // CHROME_TEST_BASE_TEST_URL_REQUEST_CONTEXT_GETTER_H_
