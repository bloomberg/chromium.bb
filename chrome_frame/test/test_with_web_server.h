// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_TEST_WITH_WEB_SERVER_H_
#define CHROME_FRAME_TEST_TEST_WITH_WEB_SERVER_H_

#include <windows.h>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "chrome_frame/chrome_tab.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

// An interface for listeners of interesting events on a MockWebServer.
class WebServerListener {
 public:
  virtual ~WebServerListener() {}

  // Invoked when a MockWebServer receives an expected response; see
  // MockWebServer::ExpectAndHandlePostedResult.
  virtual void OnExpectedResponse() = 0;
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
  MockWebServer(int port, const std::wstring& address, base::FilePath root_dir)
      : test_server::HTTPTestServer(port, address, root_dir), listener_(NULL) {}

  // Overriden from test_server::HTTPTestServer.
  MOCK_METHOD3(Get, void(test_server::ConfigurableConnection* connection,
                         const std::wstring& path,
                         const test_server::Request& r));
  MOCK_METHOD3(Post, void(test_server::ConfigurableConnection* connection,
                          const std::wstring& path,
                          const test_server::Request& r));

  // Expect a GET request for |url|. Respond with the file appropriate for
  // the given |url|. Modify the file to follow the given CFInvocation method.
  // The response includes a no-cache header. |allow_meta_tag_double_req|
  // specifies whether to allow the request to happen twice if the invocation
  // is using the CF meta tag.
  void ExpectAndServeRequest(CFInvocation invocation, const std::wstring& url);

  // Expect a number of GET requests for |url|. Rest is similar to the function
  // ExpectAndServeRequest.
  void ExpectAndServeRequestWithCardinality(CFInvocation invocation,
                                            const std::wstring& url,
                                            testing::Cardinality cardinality);

  // Same as above except do not include the no-cache header.
  void ExpectAndServeRequestAllowCache(CFInvocation invocation,
                                       const std::wstring& url);

  // Expect any number of GETs for the given resource path (e.g, /favicon.ico)
  // and respond with the file, if it exists, or a 404 if it does not.
  void ExpectAndServeRequestAnyNumberTimes(CFInvocation invocation,
                                           const std::wstring& path_prefix);

  void set_listener(WebServerListener* listener) { listener_ = listener; }

  // Expect a POST to an URL containing |post_suffix|, saving the response
  // contents for retrieval by posted_result(). Invokes the listener's
  // OnExpectedResponse method if the posted response matches the expected
  // result.
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

  void ClearResults() {
    posted_result_.clear();
    expected_result_.clear();
  }

  void set_expected_result(const std::string& expected_result) {
    expected_result_  = expected_result;
  }

  const std::string& posted_result() const {
    return posted_result_;
  }

 private:
  WebServerListener* listener_;
  // Holds the results of tests which post success/failure.
  std::string posted_result_;
  std::string expected_result_;
};

class MockWebServerListener : public WebServerListener {
 public:
  MOCK_METHOD0(OnExpectedResponse, void());
};

// Class that:
// 1) Starts the local webserver,
// 2) Supports launching browsers - Internet Explorer with local url
// 3) Wait the webserver to finish. It is supposed the test webpage to shutdown
//    the server by navigating to "kill" page
// 4) Supports read the posted results from the test webpage to the "dump"
//    webserver directory
class ChromeFrameTestWithWebServer : public testing::Test {
 public:
  ChromeFrameTestWithWebServer();

 protected:
  enum BrowserKind { INVALID, IE, CHROME };

  bool LaunchBrowser(BrowserKind browser, const wchar_t* url);

  // Returns true if the test completed in time, or false if it timed out.
  bool WaitForTestToComplete(base::TimeDelta duration);

  // Waits for the page to notify us of the window.onload event firing.
  // Note that the milliseconds value is only approximate.
  bool WaitForOnLoad(int milliseconds);

  // Launches the specified browser and waits for the test to complete (see
  // WaitForTestToComplete).  Then checks that the outcome is equal to the
  // expected result.  The test is repeated once if it fails due to a timeout.
  // This function uses EXPECT_TRUE and ASSERT_TRUE for all steps performed
  // hence no return value.
  void SimpleBrowserTestExpectedResult(BrowserKind browser,
      const wchar_t* page, const char* result);
  void SimpleBrowserTest(BrowserKind browser, const wchar_t* page);

  // Sets up expectations for a page to post back a result.
  void ExpectAndHandlePostedResult();

  // Test if chrome frame correctly reports its version.
  void VersionTest(BrowserKind browser, const wchar_t* page);

  void CloseBrowser();

  // Ensures (well, at least tries to ensure) that the browser window has focus.
  bool BringBrowserToTop();

  const base::FilePath& GetCFTestFilePath() {
    return test_file_path_;
  }

  static chrome_frame_test::TimedMsgLoop& loop() {
    return *loop_;
  }

  static testing::StrictMock<MockWebServerListener>& listener_mock() {
    return *listener_mock_;
  }

  static testing::StrictMock<MockWebServer>& server_mock() {
    return *server_mock_;
  }

  static void SetUpTestCase();
  static void TearDownTestCase();

  static const base::FilePath& GetChromeUserDataDirectory();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // The on-disk path to our html test files.
  static base::FilePath test_file_path_;
  static base::FilePath results_dir_;
  static base::FilePath CFInstall_path_;
  static base::FilePath CFInstance_path_;
  static base::FilePath chrome_user_data_dir_;

  // The user data directory used for Chrome instances.
  static base::ScopedTempDir temp_dir_;

  // The web server from which we serve the web!
  static chrome_frame_test::TimedMsgLoop* loop_;
  static std::string local_address_;
  static testing::StrictMock<MockWebServerListener>* listener_mock_;
  static testing::StrictMock<MockWebServer>* server_mock_;

  BrowserKind browser_;
  base::win::ScopedHandle browser_handle_;
};

// A helper class for doing some bookkeeping when using the
// SimpleWebServer class.
class SimpleWebServerTest {
 public:
  SimpleWebServerTest(const std::string& address, int port)
      : server_(address, port), port_(port) {
  }

  ~SimpleWebServerTest() {
    server_.DeleteAllResponses();
  }

  template <class ResponseClass>
  void PopulateStaticFileListT(const wchar_t* pages[], int count,
                               const base::FilePath& directory) {
    for (int i = 0; i < count; ++i) {
      server_.AddResponse(new ResponseClass(
          base::StringPrintf("/%ls", pages[i]).c_str(),
                             directory.Append(pages[i])));
    }
  }

  std::wstring FormatHttpPath(const wchar_t* document_path) {
    return base::StringPrintf(L"http://%ls:%i/%ls",
                              ASCIIToWide(server_.host()).c_str(), port_,
                              document_path);
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
