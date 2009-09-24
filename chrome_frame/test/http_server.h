// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_FRAME_TEST_HTTP_SERVER_H_
#define CHROME_FRAME_TEST_HTTP_SERVER_H_

#include <windows.h>
#include <string>

#include "googleurl/src/gurl.h"
#include "base/ref_counted.h"
#include "net/url_request/url_request_unittest.h"

// chrome frame specilization of http server from net.
class ChromeFrameHTTPServer {
 public:
  void SetUp();
  void TearDown();
  bool WaitToFinish(int milliseconds);
  GURL Resolve(const wchar_t* relative_url);
  std::wstring GetDataDir();

  HTTPTestServer* server() {
    return server_;
  }

 protected:
  scoped_refptr<HTTPTestServer> server_;
};

#endif  // CHROME_FRAME_TEST_HTTP_SERVER_H_

