// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_HTTP_SERVER_H_
#define CHROME_FRAME_TEST_HTTP_SERVER_H_

#include <windows.h>
#include <string>

#include "googleurl/src/gurl.h"
#include "base/ref_counted.h"
#include "net/test/test_server.h"

class FilePath;

// Chrome Frame specilization of http server from net.
class ChromeFrameHTTPServer {
 public:
  ChromeFrameHTTPServer();

  void SetUp();
  void TearDown();
  GURL Resolve(const wchar_t* relative_url);
  FilePath GetDataDir();

  net::TestServer* test_server() { return &test_server_; }

 protected:
  net::TestServer test_server_;
};

#endif  // CHROME_FRAME_TEST_HTTP_SERVER_H_
