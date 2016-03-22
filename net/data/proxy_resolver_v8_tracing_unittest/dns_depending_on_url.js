// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This proxy script has different DNS dependencies based on whether the URL
// contains "UseMyIpAddress". The final proxy list however is the same.
function FindProxyForURL(url, host) {
  if (url.indexOf("UseMyIpAddress") == -1) {
    if (!myIpAddress())
      return null;
  } else {
    if (!dnsResolve(host))
      return null;
  }

  return "PROXY foopy:47";
}
