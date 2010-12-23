// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_HTTP_RETURN_H_
#define CHROME_COMMON_NET_HTTP_RETURN_H_
#pragma once

// TODO(sanjeevr): This has been moved from the sync library so it only
// contains a few HTTP return codes. Add more HTTP return codes.
enum HTTPReturnCode {
  RC_REQUEST_OK = 200,
  RC_BAD_REQUEST = 400,
  RC_UNAUTHORIZED = 401,
  RC_FORBIDDEN = 403,
  RC_INTERNAL_SERVER_ERROR = 500,
};

#endif  // CHROME_COMMON_NET_HTTP_RETURN_H_
