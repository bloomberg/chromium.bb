// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/prescient_networking_dispatcher.h"

#include "base/strings/utf_string_conversions.h"

using blink::WebPrescientNetworking;

PrescientNetworkingDispatcher::PrescientNetworkingDispatcher() {
}

PrescientNetworkingDispatcher::~PrescientNetworkingDispatcher() {
}

void PrescientNetworkingDispatcher::prefetchDNS(
    const blink::WebString& hostname) {
  if (hostname.isEmpty())
    return;

  std::string hostname_utf8 = base::UTF16ToUTF8(hostname);
  net_predictor_.Resolve(hostname_utf8.data(), hostname_utf8.length());
}
