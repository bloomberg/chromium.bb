// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRECACHE_PRECACHE_UTIL_H_
#define CHROME_BROWSER_PRECACHE_PRECACHE_UTIL_H_

namespace net {
class URLRequest;
}

namespace precache {

void UpdatePrecacheMetricsAndState(const net::URLRequest* request,
                                   void* profile_id);

}  // namespace precache

#endif  // CHROME_BROWSER_PRECACHE_PRECACHE_UTIL_H_
