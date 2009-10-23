// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/url_request_context_getter.h"
#include "net/url_request/url_request_context.h"

net::CookieStore* URLRequestContextGetter::GetCookieStore() {
  return GetURLRequestContext()->cookie_store();
}
