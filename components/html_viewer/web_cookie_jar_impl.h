// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_WEB_COOKIE_JAR_IMPL_H_
#define COMPONENTS_HTML_VIEWER_WEB_COOKIE_JAR_IMPL_H_

#include "mojo/services/network/public/interfaces/cookie_store.mojom.h"
#include "third_party/WebKit/public/platform/WebCookieJar.h"

namespace html_viewer {

class WebCookieJarImpl : public blink::WebCookieJar {
 public:
  explicit WebCookieJarImpl(mojo::CookieStorePtr store);
  virtual ~WebCookieJarImpl();

  // blink::WebCookieJar methods:
  virtual void setCookie(const blink::WebURL& url,
                         const blink::WebURL& first_party_for_cookies,
                         const blink::WebString& cookie);
  virtual blink::WebString cookies(
      const blink::WebURL& url,
      const blink::WebURL& first_party_for_cookies);
  virtual blink::WebString cookieRequestHeaderFieldValue(
      const blink::WebURL& url,
      const blink::WebURL& first_party_for_cookies);

 private:
  mojo::CookieStorePtr store_;
  DISALLOW_COPY_AND_ASSIGN(WebCookieJarImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_WEB_COOKIE_JAR_IMPL_H_
