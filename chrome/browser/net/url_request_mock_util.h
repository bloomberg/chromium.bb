// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_URL_REQUEST_MOCK_UTIL_H_
#define CHROME_BROWSER_NET_URL_REQUEST_MOCK_UTIL_H_

// You should use routines in this file only for test code!

namespace chrome_browser_net {

// Enables or disables url request filters for mocked url requests.
void SetUrlRequestMocksEnabled(bool enabled);

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_URL_REQUEST_MOCK_UTIL_H_
