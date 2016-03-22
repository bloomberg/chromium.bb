// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This proxy script returns a proxy list that encodes the URL that was passed
// in.

function convertUrlToHostname(url) {
  // Turn the URL into something that resembles a hostname.
  var result = encodeURIComponent(url);
  return result.replace(/%/g, "x");
}

function FindProxyForURL(url, host) {
  return "PROXY " + convertUrlToHostname(url) + ":99";
}
