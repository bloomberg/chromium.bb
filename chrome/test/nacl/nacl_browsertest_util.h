// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_NACL_NACL_BROWSERTEST_UTIL_H_
#define CHROME_TEST_NACL_NACL_BROWSERTEST_UTIL_H_

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/javascript_test_observer.h"

// A helper base class that decodes structured automation messages of the form:
// {"type": type_name, ...}
class StructuredMessageHandler : public TestMessageHandler {
 public:
  virtual MessageResponse HandleMessage(const std::string& json) OVERRIDE;

  // This method provides a higher-level interface for handling JSON messages
  // from the DOM automation controler.  Instead of handling a string
  // containing a JSON-encoded object, this specialization of TestMessageHandler
  // decodes the string into a dictionary. This makes it easier to send and
  // receive structured messages.  It is assumed the dictionary will always have
  // a "type" field that indicates the nature of message.
  virtual MessageResponse HandleStructuredMessage(
      const std::string& type,
      base::DictionaryValue* msg) = 0;

 protected:
  // The structured message is missing an expected field.
  MessageResponse MissingField(
      const std::string& type,
      const std::string& field) WARN_UNUSED_RESULT;

  // Something went wrong while decoding the message.
  MessageResponse InternalError(const std::string& reason) WARN_UNUSED_RESULT;
};

// A simple structured message handler for tests that load nexes.
class LoadTestMessageHandler : public StructuredMessageHandler {
 public:
  LoadTestMessageHandler();

  void Log(const std::string& type, const std::string& message);

  virtual MessageResponse HandleStructuredMessage(
      const std::string& type,
      base::DictionaryValue* msg) OVERRIDE;

  bool test_passed() const {
    return test_passed_;
  }

 private:
  bool test_passed_;

  DISALLOW_COPY_AND_ASSIGN(LoadTestMessageHandler);
};

class NaClBrowserTestBase : public InProcessBrowserTest {
 public:
  NaClBrowserTestBase();
  virtual ~NaClBrowserTestBase();

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;

  // What variant are we running - newlib, glibc, pnacl, etc?
  // This is used to compute what directory we're pulling data from, but it can
  // also be used to affect the behavior of the test.
  virtual base::FilePath::StringType Variant() = 0;

  // Where are the files for this class of test located on disk?
  virtual bool GetDocumentRoot(base::FilePath* document_root);

  virtual bool IsPnacl();

  // Map a file relative to the variant directory to a URL served by the test
  // web server.
  GURL TestURL(const base::FilePath::StringType& url_fragment);

  // Load a URL and listen to automation events with a given handler.
  // Returns true if the test glue function correctly.  (The handler should
  // seperately indicate if the test failed.)
  bool RunJavascriptTest(const GURL& url, TestMessageHandler* handler);

  // Run a simple test that checks that a nexe loads correctly.  Useful for
  // setting up other tests, such as checking that UMA data was logged.
  void RunLoadTest(const base::FilePath::StringType& test_file);

  // Run a test that was originally written to use NaCl's integration testing
  // jig. These tests were originally driven by NaCl's SCons build in the
  // nacl_integration test stage on the Chrome waterfall. Changes in the
  // boundaries between the Chrome and NaCl repos have resulted in many of
  // these tests having a stronger affinity with the Chrome repo. This method
  // provides a compatibility layer to simplify turning nacl_integration tests
  // into browser tests.
  void RunNaClIntegrationTest(const base::FilePath::StringType& url_fragment);

 private:
  bool StartTestServer();

  scoped_ptr<net::TestServer> test_server_;
};

class NaClBrowserTestNewlib : public NaClBrowserTestBase {
 public:
  virtual base::FilePath::StringType Variant() OVERRIDE;
};

class NaClBrowserTestGLibc : public NaClBrowserTestBase {
 public:
  virtual base::FilePath::StringType Variant() OVERRIDE;
};

class NaClBrowserTestPnacl : public NaClBrowserTestBase {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

  virtual base::FilePath::StringType Variant() OVERRIDE;

  virtual bool IsPnacl() OVERRIDE;
};

// A NaCl browser test only using static files.
class NaClBrowserTestStatic : public NaClBrowserTestBase {
 public:
  virtual base::FilePath::StringType Variant() OVERRIDE;
  virtual bool GetDocumentRoot(base::FilePath* document_root) OVERRIDE;
};

#if defined(ARCH_CPU_ARM_FAMILY)

// There is no support for Glibc on ARM NaCl.
#define NACL_BROWSER_TEST_F(suite, name, body) \
IN_PROC_BROWSER_TEST_F(suite##Newlib, name) \
body

#elif defined(ADDRESS_SANITIZER) || (defined(OS_WIN) && !defined(NDEBUG))
// PNaCl's cache and PPB_FileIO currently trip up under ASAN:
// https://code.google.com/p/chromium/issues/detail?id=171810
// PNaCl tests take a long time on windows debug builds
// and sometimes time out.  Disable until it is made faster:
// https://code.google.com/p/chromium/issues/detail?id=177555
#define NACL_BROWSER_TEST_F(suite, name, body) \
IN_PROC_BROWSER_TEST_F(suite##Newlib, name) \
body \
IN_PROC_BROWSER_TEST_F(suite##GLibc, name) \
body

#else

// Otherwise, we have Glibc, Newlib and PNaCl tests
#define NACL_BROWSER_TEST_F(suite, name, body) \
IN_PROC_BROWSER_TEST_F(suite##Newlib, name) \
body \
IN_PROC_BROWSER_TEST_F(suite##GLibc, name) \
body \
IN_PROC_BROWSER_TEST_F(suite##Pnacl, name) \
body

#endif

#endif  // CHROME_TEST_NACL_NACL_BROWSERTEST_UTIL_H_
