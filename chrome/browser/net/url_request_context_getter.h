// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_URL_REQUEST_CONTEXT_GETTER_H_
#define CHROME_BROWSER_NET_URL_REQUEST_CONTEXT_GETTER_H_

#include "base/ref_counted.h"
#include "chrome/browser/chrome_thread.h"

namespace net {
class CookieStore;
}

class URLRequestContext;

// Interface for retrieving an URLRequestContext.
class URLRequestContextGetter
    : public base::RefCountedThreadSafe<URLRequestContextGetter,
                                        ChromeThread::DeleteOnIOThread> {
 public:
  virtual URLRequestContext* GetURLRequestContext() = 0;

  // Defaults to GetURLRequestContext()->cookie_store(), but
  // implementations can override it.
  virtual net::CookieStore* GetCookieStore();

 protected:
  friend class ChromeThread;
  friend class DeleteTask<URLRequestContextGetter>;

  virtual ~URLRequestContextGetter() {}
};

#endif  // CHROME_BROWSER_NET_URL_REQUEST_CONTEXT_GETTER_H_

