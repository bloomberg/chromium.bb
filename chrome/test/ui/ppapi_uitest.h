// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UI_PPAPI_UITEST_H_
#define CHROME_TEST_UI_PPAPI_UITEST_H_
#pragma once

#include <string>

#include "chrome/test/base/in_process_browser_test.h"

class PPAPITestBase : public InProcessBrowserTest {
 public:
  PPAPITestBase();

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

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
  // Runs the test for a tab given the tab that's already navigated to the
  // given URL.
  void RunTestURL(const GURL& test_url);
  // Run the given |test_case| on a HTTP test server whose document root is
  // specified by |document_root|. |extra_params| will be passed as URL
  // parameters to the test.
  void RunHTTPTestServer(const FilePath& document_root,
                         const std::string& test_case,
                         const std::string& extra_params);
  // Return the document root for the HTTP server on which tests will be run.
  // The result is placed in |document_root|. False is returned upon failure.
  bool GetHTTPDocumentRoot(FilePath* document_root);
};

// In-process plugin test runner.  See OutOfProcessPPAPITest below for the
// out-of-process version.
class PPAPITest : public PPAPITestBase {
 public:
  PPAPITest();

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

  virtual std::string BuildQuery(const std::string& base,
                                 const std::string& test_case) OVERRIDE;
};

// Variant of PPAPITest that runs plugins out-of-process to test proxy
// codepaths.
class OutOfProcessPPAPITest : public PPAPITest {
 public:
  OutOfProcessPPAPITest();

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
};

// NaCl plugin test runner.
class PPAPINaClTest : public PPAPITestBase {
 public:
  PPAPINaClTest();

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

  virtual std::string BuildQuery(const std::string& base,
                                 const std::string& test_case) OVERRIDE;
};

class PPAPINaClTestDisallowedSockets : public PPAPITestBase {
 public:
  PPAPINaClTestDisallowedSockets();

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

  virtual std::string BuildQuery(const std::string& base,
                                 const std::string& test_case) OVERRIDE;
};

#endif  // CHROME_TEST_UI_PPAPI_UITEST_H_
