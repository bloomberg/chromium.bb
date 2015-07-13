// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_REQUEST_SOURCE_BANDWIDTH_HISTOGRAMS_H_
#define CHROME_BROWSER_NET_REQUEST_SOURCE_BANDWIDTH_HISTOGRAMS_H_

namespace net {
class URLRequest;
}  // namespace net

// Records the bandwidth (currently just response size) made by |request| in a
// histogram chosen based on the source of the request (currently browser,
// renderer, or other process).
void RecordRequestSourceBandwidth(const net::URLRequest* request,
                                  bool started);

#endif  // CHROME_BROWSER_NET_REQUEST_SOURCE_BANDWIDTH_HISTOGRAMS_H_
