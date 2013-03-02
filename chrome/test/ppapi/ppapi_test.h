// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PPAPI_PPAPI_TEST_H_
#define CHROME_TEST_PPAPI_PPAPI_TEST_H_

#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/javascript_test_observer.h"
#include "net/test/test_server.h"

namespace content {
class RenderViewHost;
}

class PPAPITestMessageHandler : public TestMessageHandler {
 public:
  PPAPITestMessageHandler();

  MessageResponse HandleMessage(const std::string& json);

  virtual void Reset() OVERRIDE;

  const std::string& message() const {
    return message_;
  }

 private:
  std::string message_;

  DISALLOW_COPY_AND_ASSIGN(PPAPITestMessageHandler);
};

class PPAPITestBase : public InProcessBrowserTest {
 public:
  PPAPITestBase();

  // InProcessBrowserTest:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;

  virtual std::string BuildQuery(const std::string& base,
                                 const std::string& test_case) = 0;

  // Returns the URL to load for file: tests.
  GURL GetTestFileUrl(const std::string& test_case);
  void RunTest(const std::string& test_case);
  // Run the test and reload. This can test for clean shutdown, including leaked
  // instance object vars.
  void RunTestAndReload(const std::string& test_case);
  void RunTestViaHTTP(const std::string& test_case);
  void RunTestWithSSLServer(const std::string& test_case);
  void RunTestWithWebSocketServer(const std::string& test_case);
  void RunTestIfAudioOutputAvailable(const std::string& test_case);
  void RunTestViaHTTPIfAudioOutputAvailable(const std::string& test_case);
  std::string StripPrefixes(const std::string& test_name);

 protected:
  class InfoBarObserver : public content::NotificationObserver {
   public:
    InfoBarObserver();
    ~InfoBarObserver();

    virtual void Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) OVERRIDE;

    void ExpectInfoBarAndAccept(bool should_accept);

   private:
    content::NotificationRegistrar registrar_;
    std::list<bool> expected_infobars_;
  };

  // Runs the test for a tab given the tab that's already navigated to the
  // given URL.
  void RunTestURL(const GURL& test_url);
  // Gets the URL of the the given |test_case| for the given HTTP test server.
  // If |extra_params| is non-empty, it will be appended as URL parameters.
  GURL GetTestURL(const net::TestServer& http_server,
                  const std::string& test_case,
                  const std::string& extra_params);

  // Return the document root for the HTTP server on which tests will be run.
  // The result is placed in |document_root|. False is returned upon failure.
  bool GetHTTPDocumentRoot(base::FilePath* document_root);
};

// In-process plugin test runner.  See OutOfProcessPPAPITest below for the
// out-of-process version.
class PPAPITest : public PPAPITestBase {
 public:
  PPAPITest();

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

  virtual std::string BuildQuery(const std::string& base,
                                 const std::string& test_case) OVERRIDE;
 protected:
  bool in_process_;  // Controls the --ppapi-in-process switch.
};

// Variant of PPAPITest that runs plugins out-of-process to test proxy
// codepaths.
class OutOfProcessPPAPITest : public PPAPITest {
 public:
  OutOfProcessPPAPITest();

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
};

// NaCl plugin test runner for Newlib runtime.
class PPAPINaClTest : public PPAPITestBase {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
};

// NaCl plugin test runner for Newlib runtime.
class PPAPINaClNewlibTest : public PPAPINaClTest {
 public:
  virtual std::string BuildQuery(const std::string& base,
                                 const std::string& test_case) OVERRIDE;
};

// NaCl plugin test runner for GNU-libc runtime.
class PPAPINaClGLibcTest : public PPAPINaClTest {
 public:
  virtual std::string BuildQuery(const std::string& base,
                                 const std::string& test_case) OVERRIDE;
};

class PPAPINaClTestDisallowedSockets : public PPAPITestBase {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

  virtual std::string BuildQuery(const std::string& base,
                                 const std::string& test_case) OVERRIDE;
};

class PPAPIBrokerInfoBarTest : public OutOfProcessPPAPITest {
 public:
  // PPAPITestBase override:
  virtual void SetUpOnMainThread() OVERRIDE;
};

#endif  // CHROME_TEST_PPAPI_PPAPI_TEST_H_
