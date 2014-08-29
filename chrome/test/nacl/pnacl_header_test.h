// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_NACL_PNACL_HEADER_TEST_H_
#define CHROME_TEST_NACL_PNACL_HEADER_TEST_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace base {
class FilePath;
}

namespace net {
namespace test_server {
struct HttpRequest;
class HttpResponse;
}
}

class PnaclHeaderTest : public InProcessBrowserTest {
 public:
  PnaclHeaderTest();
  virtual ~PnaclHeaderTest();

  // Run a simple test that checks that the NaCl plugin sends the right
  // headers when doing |expected_noncors| same origin pexe load requests
  // and |expected_cors| cross origin pexe load requests.
  void RunLoadTest(const std::string& url,
                   int expected_noncors,
                   int expected_cors);

 private:
  void StartServer();

  scoped_ptr<net::test_server::HttpResponse> WatchForPexeFetch(
      const net::test_server::HttpRequest& request);

  int noncors_loads_;
  int cors_loads_;
  DISALLOW_COPY_AND_ASSIGN(PnaclHeaderTest);
};

#endif  // CHROME_TEST_NACL_PNACL_HEADER_TEST_H_
