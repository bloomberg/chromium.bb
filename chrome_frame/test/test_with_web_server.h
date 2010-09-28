// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_TEST_WITH_WEB_SERVER_H_
#define CHROME_FRAME_TEST_TEST_WITH_WEB_SERVER_H_

#include <windows.h>
#include <string>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/http_server.h"
#include "chrome_frame/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

// Include without path to make GYP build see it.
#include "chrome_tab.h"  // NOLINT

// Specifies the invocation method for CF.
class CFInvocation {
 public:
  enum Type {
    NONE = 0,
    META_TAG,
    HTTP_HEADER
  };

  CFInvocation(): method_(NONE) {}
  explicit CFInvocation(Type method): method_(method) {}

  // Convience methods for creating this class.
  static CFInvocation None() { return CFInvocation(NONE); }
  static CFInvocation MetaTag() { return CFInvocation(META_TAG); }
  static CFInvocation HttpHeader() { return CFInvocation(HTTP_HEADER); }

  // Returns whether this page does invoke CF.
  bool invokes_cf() const {
    return method_ != NONE;
  }

  Type type() const { return method_; }

 private:
  Type method_;
};

// Simple Gmock friendly web server. Sample usage:
// MockWebServer mock(9999, "0.0.0.0");
// EXPECT_CALL(mock, Get(_, StrEq("/favicon.ico"), _)).WillRepeatedly(SendFast(
//     "HTTP/1.1 404 Not Found"
//     "text/html; charset=UTF-8", EmptyString()));
//
// EXPECT_CALL(mock, Get(_, StrEq("/book"), _)).WillRepeatedly(Send(
//     "HTTP/1.1 302 Found\r\n"
//     "Connection: close\r\n"
//     "Content-Type: text/html\r\n"
//     "Location: library\r\n",
//     "<html>Lalalala</html>", 3, 1000));
//
// EXPECT_CALL(mock, Get(_, StrEq("/library"), _)).WillRepeatedly(Send(
//     "HTTP/1.1 200 OK\r\n"
//     "Connection: close\r\n"
//     "Content-Type: text/html\r\n",
//     "<html><meta http-equiv=\"X-UA-Compatible\" content=\"chrome=1\" />"
//     "<body>Rendered in CF.</body></html>", 4, 1000));
class MockWebServer : public test_server::HTTPTestServer {
 public:
  MockWebServer(int port, const std::wstring& address, FilePath root_dir)
      : test_server::HTTPTestServer(port, address, root_dir) {}

  // Overriden from test_server::HTTPTestServer.
  MOCK_METHOD3(Get, void(test_server::ConfigurableConnection* connection,
                         const std::wstring& path,
                         const test_server::Request&r));
  MOCK_METHOD3(Post, void(test_server::ConfigurableConnection* connection,
                          const std::wstring& path,
                          const test_server::Request&r));

  // Expect a GET request for |url|. Respond with the file appropriate for
  // the given |url|. Modify the file to follow the given CFInvocation method.
  // The response includes a no-cache header. |allow_meta_tag_double_req|
  // specifies whether to allow the request to happen twice if the invocation
  // is using the CF meta tag.
  void ExpectAndServeRequest(CFInvocation invocation, const std::wstring& url);

  // Same as above except do not include the no-cache header.
  void ExpectAndServeRequestAllowCache(CFInvocation invocation,
                                       const std::wstring& url);

  // Expect any number of GETs for the given resource path (e.g, /favicon.ico)
  // and respond with the file, if it exists, or a 404 if it does not.
  void ExpectAndServeRequestAnyNumberTimes(CFInvocation invocation,
                                           const std::wstring& path_prefix);

  void ExpectAndHandlePostedResult(CFInvocation invocation,
                                   const std::wstring& post_suffix);

  // Expect and serve all incoming GET requests.
  void ExpectAndServeAnyRequests(CFInvocation invocation) {
    ExpectAndServeRequestAnyNumberTimes(invocation, L"");
  }


  // Send a response on the given connection appropriate for |resource_uri|.
  // If the file referred to by |path| exists, send the file data, otherwise
  // send 404. Modify the file data according to the given invocation method.
  void SendResponseHelper(test_server::ConfigurableConnection* connection,
                          const std::wstring& resource_uri,
                          const test_server::Request& request,
                          CFInvocation invocation,
                          bool add_no_cache_header);
  // Handles the posted /writefile response
  void HandlePostedResponse(test_server::ConfigurableConnection* connection,
                            const test_server::Request& request);

  void set_expected_result(const std::string& expected_result) {
    expected_result_  = expected_result;
  }

  const std::string& posted_result() const {
    return posted_result_;
  }

 private:
  // Holds the results of tests which post success/failure.
  std::string posted_result_;
  std::string expected_result_;
};

// Class that:
// 1) Starts the local webserver,
// 2) Supports launching browsers - Internet Explorer and Firefox with local url
// 3) Wait the webserver to finish. It is supposed the test webpage to shutdown
//    the server by navigating to "kill" page
// 4) Supports read the posted results from the test webpage to the "dump"
//    webserver directory
class ChromeFrameTestWithWebServer: public testing::Test {
 public:
  ChromeFrameTestWithWebServer();

 protected:
  enum BrowserKind { INVALID, IE, FIREFOX, OPERA, SAFARI, CHROME };

  bool LaunchBrowser(BrowserKind browser, const wchar_t* url);
  bool WaitForTestToComplete(int milliseconds);
  // Waits for the page to notify us of the window.onload event firing.
  // Note that the milliseconds value is only approximate.
  bool WaitForOnLoad(int milliseconds);

  // Launches the specified browser and waits for the test to complete
  // (see WaitForTestToComplete).  Then checks that the outcome is equal
  // to the expected result.
  // This function uses EXPECT_TRUE and ASSERT_TRUE for all steps performed
  // hence no return value.
  void SimpleBrowserTestExpectedResult(BrowserKind browser,
      const wchar_t* page, const char* result);
  void SimpleBrowserTest(BrowserKind browser, const wchar_t* page);

  // Same as SimpleBrowserTest but if the browser isn't installed (LaunchBrowser
  // fails), the function will print out a warning but not treat the test
  // as failed.
  // Currently this is how we run Opera tests.
  void OptionalBrowserTest(BrowserKind browser, const wchar_t* page);

  // Test if chrome frame correctly reports its version.
  void VersionTest(BrowserKind browser, const wchar_t* page);

  // Closes all browsers in preparation for a test and during cleanup.
  void CloseAllBrowsers();

  void CloseBrowser();

  // Ensures (well, at least tries to ensure) that the browser window has focus.
  bool BringBrowserToTop();

  const FilePath& GetCFTestFilePath() {
    return test_file_path_;
  }

  virtual void SetUp();
  virtual void TearDown();

  // Important: kind means "sheep" in Icelandic. ?:-o
  const char* ToString(BrowserKind kind) {
    switch (kind) {
      case IE:
        return "IE";
      case FIREFOX:
        return "Firefox";
      case OPERA:
        return "Opera";
      case CHROME:
        return "Chrome";
      case SAFARI:
        return "Safari";
      default:
        NOTREACHED();
        break;
    }
    return "";
  }

  BrowserKind browser_;
  FilePath results_dir_;
  ScopedHandle browser_handle_;
  // The on-disk path to our html test files.
  FilePath test_file_path_;

  FilePath CFInstall_path_;
  FilePath CFInstance_path_;

  chrome_frame_test::TimedMsgLoop loop_;
  testing::StrictMock<MockWebServer> server_mock_;
};

// A helper class for doing some bookkeeping when using the
// SimpleWebServer class.
class SimpleWebServerTest {
 public:
  explicit SimpleWebServerTest(int port) : server_(port), port_(port) {
  }

  ~SimpleWebServerTest() {
    server_.DeleteAllResponses();
  }

  template <class ResponseClass>
  void PopulateStaticFileListT(const wchar_t* pages[], int count,
                               const FilePath& directory) {
    for (int i = 0; i < count; ++i) {
      server_.AddResponse(new ResponseClass(
          StringPrintf("/%ls", pages[i]).c_str(), directory.Append(pages[i])));
    }
  }

  std::wstring FormatHttpPath(const wchar_t* document_path) {
    return StringPrintf(L"http://localhost:%i/%ls", port_, document_path);
  }

  // Returns the last client request object.
  // Under normal circumstances this will be the request for /quit.
  const test_server::Request& last_request() const {
    const test_server::ConnectionList& connections = server_.connections();
    DCHECK(connections.size());
    const test_server::Connection* c = connections.back();
    return c->request();
  }

  bool FindRequest(const std::string& path,
                   const test_server::Request** request) {
    test_server::ConnectionList::const_iterator index;
    for (index = server_.connections().begin();
         index != server_.connections().end(); index++) {
      const test_server::Connection* connection = *index;
      if (!lstrcmpiA(connection->request().path().c_str(), path.c_str())) {
        if (request)
          *request = &connection->request();
        return true;
      }
    }
    return false;
  }

  // Counts the number of times a page was requested.
  // Optionally checks if the request method for each is equal to
  // |expected_method|.  If expected_method is NULL no such check is made.
  int GetRequestCountForPage(const wchar_t* page, const char* expected_method) {
    // Check how many requests we got for the cf page.
    test_server::ConnectionList::const_iterator it;
    int requests = 0;
    const test_server::ConnectionList& connections = server_.connections();
    for (it = connections.begin(); it != connections.end(); ++it) {
      const test_server::Connection* c = (*it);
      const test_server::Request& r = c->request();
      if (!r.path().empty() &&
          ASCIIToWide(r.path().substr(1)).compare(page) == 0) {
        if (expected_method) {
          EXPECT_EQ(expected_method, r.method());
        }
        requests++;
      }
    }
    return requests;
  }

  test_server::SimpleWebServer* web_server() {
    return &server_;
  }

 protected:
  test_server::SimpleWebServer server_;
  int port_;
};

ACTION_P2(SendFast, headers, content) {
  arg0->Send(headers, content);
}

ACTION_P4(Send, headers, content, chunk, timeout) {
  test_server::ConfigurableConnection::SendOptions options(
      test_server::ConfigurableConnection::SendOptions::
        IMMEDIATE_HEADERS_DELAYED_CONTENT, chunk, timeout);
  arg0->SendWithOptions(std::string(headers),
                        std::string(content),
                        options);
}

ACTION_P4(SendSlow, headers, content, chunk, timeout) {
  test_server::ConfigurableConnection::SendOptions options(
    test_server::ConfigurableConnection::SendOptions::DELAYED, chunk, timeout);
  arg0->SendWithOptions(std::string(headers),
                        std::string(content),
                        options);
}

// Sends a response with the file data for the given path, if the file exists,
// or a 404 if the file does not. This response includes a no-cache header.
ACTION_P2(SendResponse, server, invocation) {
  server->SendResponseHelper(arg0, arg1, arg2, invocation, true);
}

// Same as above except that the response does not include the no-cache header.
ACTION_P2(SendAllowCacheResponse, server, invocation) {
  server->SendResponseHelper(arg0, arg1, invocation, false);
}

ACTION_P2(HandlePostedResponseHelper, server, invocation) {
  server->HandlePostedResponse(arg0, arg2);
}

#endif  // CHROME_FRAME_TEST_TEST_WITH_WEB_SERVER_H_
