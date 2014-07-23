// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/html_viewer/webcookiejar_impl.h"

#include "base/bind.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace mojo {
namespace {

void CopyBool(bool* output, bool input) {
  *output = input;
}

void CopyString(String* output, const String& input) {
  *output = input;
}

}  // namespace

WebCookieJarImpl::WebCookieJarImpl(CookieStorePtr store)
    : store_(store.Pass()) {
}

WebCookieJarImpl::~WebCookieJarImpl() {
}

void WebCookieJarImpl::setCookie(const blink::WebURL& url,
                                 const blink::WebURL& first_party_for_cookies,
                                 const blink::WebString& cookie) {
  bool success;
  store_->Set(url.string().utf8(), cookie.utf8(),
              base::Bind(&CopyBool, &success));

  // Wait to ensure the cookie was set before advancing. That way any
  // subsequent URL request will see the changes to the cookie store.
  //
  // TODO(darin): Consider using associated message pipes for the CookieStore
  // and URLLoader, so that we could let this method call run asynchronously
  // without suffering an ordering problem. See crbug/386825.
  //
  store_.WaitForIncomingMethodCall();
}

blink::WebString WebCookieJarImpl::cookies(
    const blink::WebURL& url,
    const blink::WebURL& first_party_for_cookies) {
  String result;
  store_->Get(url.string().utf8(), base::Bind(&CopyString, &result));

  // Wait for the result. Since every outbound request we make to the cookie
  // store is followed up with WaitForIncomingMethodCall, we can be sure that
  // the next incoming method call will be the response to our request.
  store_.WaitForIncomingMethodCall();
  if (!result)
    return blink::WebString();

  return blink::WebString::fromUTF8(result);
}

blink::WebString WebCookieJarImpl::cookieRequestHeaderFieldValue(
    const blink::WebURL& url,
    const blink::WebURL& first_party_for_cookies) {
  return cookies(url, first_party_for_cookies);
}

}  // namespace mojo
