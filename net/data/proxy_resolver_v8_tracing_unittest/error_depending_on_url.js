// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This proxy script throws an error if the URL does not contain the substring
// "DontThrowError".
function FindProxyForURL(url, host) {
  if (url.indexOf("DontThrowError") == -1) {
    ErrorUndefinedFunction();
    return -1;
  }
  return "PROXY foopy:42";
}
