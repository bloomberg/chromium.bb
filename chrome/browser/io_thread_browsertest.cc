// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/io_thread.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "net/base/filename_util.h"
#include "net/base/host_port_pair.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/simple_connection_listener.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// URLFetcherDelegate that expects a request to hang.
class HangingURLFetcherDelegate : public net::URLFetcherDelegate {
 public:
  HangingURLFetcherDelegate() {}
  ~HangingURLFetcherDelegate() override {}

  void OnURLFetchComplete(const net::URLFetcher* source) override {
    ADD_FAILURE() << "This request should never complete.";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HangingURLFetcherDelegate);
};

// URLFetcherDelegate that can wait for a request to succeed.
class TestURLFetcherDelegate : public net::URLFetcherDelegate {
 public:
  TestURLFetcherDelegate() {}
  ~TestURLFetcherDelegate() override {}

  void OnURLFetchComplete(const net::URLFetcher* source) override {
    run_loop_.Quit();
  }

  void WaitForCompletion() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestURLFetcherDelegate);
};

class IOThreadBrowserTest : public InProcessBrowserTest {
 public:
  IOThreadBrowserTest() {}
  ~IOThreadBrowserTest() override {}

  void SetUp() override {
    // Must start listening (And get a port for the proxy) before calling
    // SetUp(). Use two phase EmbeddedTestServer setup for proxy tests.
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->StartAcceptingConnections();

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDown() override {
    // Need to stop this before |connection_listener_| is destroyed.
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDown();
  }
};

// Make sure that the system URLRequestContext does not cache responses. Main
// reason for this test is that caching requires memory, so this guards against
// accidentally hooking up a cache.
IN_PROC_BROWSER_TEST_F(IOThreadBrowserTest, NoCache) {
  GURL cacheable_url = embedded_test_server()->GetURL("/cachetime");
  // Request a cacheable resource. Request should succeed.
  TestURLFetcherDelegate fetcher_delegate;
  std::unique_ptr<net::URLFetcher> fetcher = net::URLFetcher::Create(
      cacheable_url, net::URLFetcher::GET, &fetcher_delegate);
  fetcher->SetRequestContext(
      g_browser_process->io_thread()->system_url_request_context_getter());
  fetcher->Start();
  fetcher_delegate.WaitForCompletion();
  EXPECT_EQ(200, fetcher->GetResponseCode());

  // Shut down server and re-request resource.  Request should fail.
  TestURLFetcherDelegate failed_fetcher_delegate;
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  fetcher = net::URLFetcher::Create(cacheable_url, net::URLFetcher::GET,
                                    &failed_fetcher_delegate);
  fetcher->SetRequestContext(
      g_browser_process->io_thread()->system_url_request_context_getter());
  fetcher->Start();
  failed_fetcher_delegate.WaitForCompletion();
  EXPECT_FALSE(fetcher->GetStatus().is_success());
  EXPECT_EQ(net::ERR_CONNECTION_REFUSED, fetcher->GetStatus().error());
}

class IOThreadBrowserTestWithHangingPacRequest : public IOThreadBrowserTest {
 public:
  IOThreadBrowserTestWithHangingPacRequest() {}
  ~IOThreadBrowserTestWithHangingPacRequest() override {}

  void SetUpOnMainThread() override {
    // This must be created after the main message loop has been set up.
    // Waits for one connection.  Additional connections are fine.
    connection_listener_ =
        base::MakeUnique<net::test_server::SimpleConnectionListener>(
            1, net::test_server::SimpleConnectionListener::
                   ALLOW_ADDITIONAL_CONNECTIONS);

    embedded_test_server()->SetConnectionListener(connection_listener_.get());

    IOThreadBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kProxyPacUrl, embedded_test_server()->GetURL("/hung").spec());
  }

 protected:
  std::unique_ptr<net::test_server::SimpleConnectionListener>
      connection_listener_;
};

// Make sure that the SystemURLRequestContext is shut down correctly when
// there's an in-progress PAC script fetch.
IN_PROC_BROWSER_TEST_F(IOThreadBrowserTestWithHangingPacRequest, Shutdown) {
  // Request that should hang while trying to request the PAC script.
  // Enough requests are created on startup that this probably isn't needed, but
  // best to be safe.
  HangingURLFetcherDelegate hanging_fetcher_delegate;
  std::unique_ptr<net::URLFetcher> hanging_fetcher = net::URLFetcher::Create(
      GURL("http://blah/"), net::URLFetcher::GET, &hanging_fetcher_delegate);
  hanging_fetcher->SetRequestContext(
      g_browser_process->io_thread()->system_url_request_context_getter());
  hanging_fetcher->Start();

  connection_listener_->WaitForConnections();
}

class IOThreadBrowserTestWithPacFileURL : public IOThreadBrowserTest {
 public:
  IOThreadBrowserTestWithPacFileURL() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  ~IOThreadBrowserTestWithPacFileURL() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::FilePath pac_file_path;
    ASSERT_TRUE(
        base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &pac_file_path));

    std::string pac_script = base::StringPrintf(
        "function FindProxyForURL(url, host){ return 'PROXY %s;'; }",
        net::HostPortPair::FromURL(embedded_test_server()->base_url())
            .ToString()
            .c_str());
    ASSERT_EQ(
        static_cast<int>(pac_script.size()),
        base::WriteFile(pac_file_path, pac_script.c_str(), pac_script.size()));

    command_line->AppendSwitchASCII(
        switches::kProxyPacUrl, net::FilePathToFileURL(pac_file_path).spec());
  }

 protected:
  base::ScopedTempDir temp_dir_;
};

// Make sure the system URLRequestContext can hadle fetching PAC scripts from
// file URLs.
IN_PROC_BROWSER_TEST_F(IOThreadBrowserTestWithPacFileURL, FilePac) {
  TestURLFetcherDelegate fetcher_delegate;
  std::unique_ptr<net::URLFetcher> fetcher =
      net::URLFetcher::Create(GURL("http://foo:12345/echoheader?Foo"),
                              net::URLFetcher::GET, &fetcher_delegate);
  fetcher->AddExtraRequestHeader("Foo: Bar");
  fetcher->SetRequestContext(
      g_browser_process->io_thread()->system_url_request_context_getter());
  fetcher->Start();
  fetcher_delegate.WaitForCompletion();
  EXPECT_EQ(200, fetcher->GetResponseCode());
  std::string response;
  ASSERT_TRUE(fetcher->GetResponseAsString(&response));
  EXPECT_EQ("Bar", response);
}

}  // namespace
