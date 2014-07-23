// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_WEBCOOKIEJAR_IMPL_H_
#define MOJO_SERVICES_HTML_VIEWER_WEBCOOKIEJAR_IMPL_H_

#include "mojo/services/public/interfaces/network/cookie_store.mojom.h"
#include "third_party/WebKit/public/platform/WebCookieJar.h"

namespace mojo {

class WebCookieJarImpl : public blink::WebCookieJar {
 public:
  explicit WebCookieJarImpl(CookieStorePtr store);
  virtual ~WebCookieJarImpl();

  // blink::WebCookieJar methods:
  virtual void setCookie(
      const blink::WebURL& url,
      const blink::WebURL& first_party_for_cookies,
      const blink::WebString& cookie);
  virtual blink::WebString cookies(
      const blink::WebURL& url,
      const blink::WebURL& first_party_for_cookies);
  virtual blink::WebString cookieRequestHeaderFieldValue(
      const blink::WebURL& url,
      const blink::WebURL& first_party_for_cookies);

 private:
  CookieStorePtr store_;
  DISALLOW_COPY_AND_ASSIGN(WebCookieJarImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_HTML_VIEWER_WEBCOOKIEJAR_IMPL_H_
