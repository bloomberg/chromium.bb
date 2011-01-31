// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include "base/logging.h"
#include "net/http/http_request_headers.h"

ChromeNetworkDelegate::ChromeNetworkDelegate() {}
ChromeNetworkDelegate::~ChromeNetworkDelegate() {}

void ChromeNetworkDelegate::OnSendHttpRequest(
    net::HttpRequestHeaders* headers) {
  DCHECK(headers);

  // TODO(willchan): Add Chrome-side hooks to listen / mutate requests here.
}
