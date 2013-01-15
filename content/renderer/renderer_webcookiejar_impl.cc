// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webcookiejar_impl.h"

#include "base/utf_string_conversions.h"
#include "content/common/view_messages.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCookie.h"
#include "webkit/glue/webcookie.h"

using WebKit::WebCookie;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebVector;

namespace content {

void RendererWebCookieJarImpl::setCookie(
    const WebURL& url, const WebURL& first_party_for_cookies,
    const WebString& value) {
  std::string value_utf8;
  UTF16ToUTF8(value.data(), value.length(), &value_utf8);
  if (!GetContentClient()->renderer()->HandleSetCookieRequest(
          sender_, url, first_party_for_cookies, value_utf8)) {
    sender_->Send(new ViewHostMsg_SetCookie(
        MSG_ROUTING_NONE, url, first_party_for_cookies, value_utf8));
  }
}

WebString RendererWebCookieJarImpl::cookies(
    const WebURL& url, const WebURL& first_party_for_cookies) {
  std::string value_utf8;

  if (!GetContentClient()->renderer()->HandleGetCookieRequest(
          sender_, url, first_party_for_cookies, &value_utf8)) {
    // NOTE: This may pump events (see RenderThread::Send).
    sender_->Send(new ViewHostMsg_GetCookies(
        MSG_ROUTING_NONE, url, first_party_for_cookies, &value_utf8));
  }
  return WebString::fromUTF8(value_utf8);
}

WebString RendererWebCookieJarImpl::cookieRequestHeaderFieldValue(
    const WebURL& url, const WebURL& first_party_for_cookies) {
  return cookies(url, first_party_for_cookies);
}

void RendererWebCookieJarImpl::rawCookies(
    const WebURL& url, const WebURL& first_party_for_cookies,
    WebVector<WebCookie>& raw_cookies) {
  std::vector<webkit_glue::WebCookie> cookies;
  // NOTE: This may pump events (see RenderThread::Send).
  sender_->Send(new ViewHostMsg_GetRawCookies(
      url, first_party_for_cookies, &cookies));

  WebVector<WebCookie> result(cookies.size());
  int i = 0;
  for (std::vector<webkit_glue::WebCookie>::iterator it = cookies.begin();
       it != cookies.end(); ++it) {
     result[i++] = WebCookie(WebString::fromUTF8(it->name),
                             WebString::fromUTF8(it->value),
                             WebString::fromUTF8(it->domain),
                             WebString::fromUTF8(it->path),
                             it->expires,
                             it->http_only,
                             it->secure,
                             it->session);
  }
  raw_cookies.swap(result);
}

void RendererWebCookieJarImpl::deleteCookie(
    const WebURL& url, const WebString& cookie_name) {
  std::string cookie_name_utf8;
  UTF16ToUTF8(cookie_name.data(), cookie_name.length(), &cookie_name_utf8);
  sender_->Send(new ViewHostMsg_DeleteCookie(url, cookie_name_utf8));
}

bool RendererWebCookieJarImpl::cookiesEnabled(
    const WebURL& url, const WebURL& first_party_for_cookies) {
  bool cookies_enabled;
  // NOTE: This may pump events (see RenderThread::Send).
  sender_->Send(new ViewHostMsg_CookiesEnabled(
      url, first_party_for_cookies, &cookies_enabled));
  return cookies_enabled;
}

}  // namespace content
