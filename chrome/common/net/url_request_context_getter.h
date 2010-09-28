// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_URL_REQUEST_CONTEXT_GETTER_H_
#define CHROME_COMMON_NET_URL_REQUEST_CONTEXT_GETTER_H_
#pragma once

#include "base/ref_counted.h"
#include "base/task.h"

namespace base {
class MessageLoopProxy;
}

namespace net {
class CookieStore;
}

class URLRequestContext;
struct URLRequestContextGetterTraits;

// Interface for retrieving an URLRequestContext.
class URLRequestContextGetter
    : public base::RefCountedThreadSafe<URLRequestContextGetter,
                                        URLRequestContextGetterTraits> {
 public:
  virtual URLRequestContext* GetURLRequestContext() = 0;

  // Defaults to GetURLRequestContext()->cookie_store(), but
  // implementations can override it.
  virtual net::CookieStore* GetCookieStore();
  // Returns a MessageLoopProxy corresponding to the thread on which the
  // request IO happens (the thread on which the returned URLRequestContext
  // may be used).
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() = 0;

 protected:
  friend class DeleteTask<URLRequestContextGetter>;
  friend struct URLRequestContextGetterTraits;

  virtual ~URLRequestContextGetter() {}
 private:
  // OnDestruct is meant to ensure deletion on the thread on which the request
  // IO happens.
  void OnDestruct();
};

struct URLRequestContextGetterTraits {
  static void Destruct(URLRequestContextGetter* context_getter) {
    context_getter->OnDestruct();
  }
};

#endif  // CHROME_COMMON_NET_URL_REQUEST_CONTEXT_GETTER_H_

