// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_NET_HTTP_RETURN_H_
#define CHROME_BROWSER_SYNC_ENGINE_NET_HTTP_RETURN_H_

namespace browser_sync {
enum HTTPReturnCode {
  RC_REQUEST_OK = 200,
  RC_UNAUTHORIZED = 401,
  RC_FORBIDDEN = 403,
};
}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_NET_HTTP_RETURN_H_
