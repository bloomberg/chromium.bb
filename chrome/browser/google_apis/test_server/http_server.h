// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_TEST_SERVER_HTTP_SERVER_H_
#define CHROME_BROWSER_GOOGLE_APIS_TEST_SERVER_HTTP_SERVER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/google_apis/test_server/http_connection.h"
#include "chrome/browser/google_apis/test_server/http_response.h"
#include "net/base/tcp_listen_socket.h"

namespace drive {
namespace test_server {

// This class is required to be able to have composition instead of inheritance,
class HttpListenSocket: public net::TCPListenSocket {
 public:
  HttpListenSocket(const SocketDescriptor socket_descriptor,
                   net::StreamListenSocket::Delegate* delegate);
  virtual void Listen();

 private:
  virtual ~HttpListenSocket();
};

// Class providing a HTTP server for testing purpose. This is a basic server
// providing only an essential subset of HTTP/1.1 protocol. Especially,
// it assumes that the request syntax is correct. It *does not* support
// a Chunked Transfer Encoding.
//
// The common use case is below:
//
// scoped_ptr<HttpServer> test_server_;
// GURL hello_world_url_;
// GURL file_url_;
// (...)
// void SetUp() {
//   test_server_.reset(new HttpServer());
//   DCHECK(test_server_.InitializeAndWaitUntilReady());
//   hello_world_url = test_server->RegisterTextResponse(
//       "/abc",
//       "<b>Hello world!</b>",
//       "text/html");
//   metadata_url = test_server->RegisterFileResponse(
//       "metadata/file.doc")
//       "testing/data/metadata.xml",
//       "text/xml",
//       200);
// }
class HttpServer : private net::StreamListenSocket::Delegate {
 public:
  typedef base::Callback<scoped_ptr<HttpResponse>(const HttpRequest& request)>
      HandleRequestCallback;

  // Creates a http test server. InitializeAndWaitUntilReady() must be called
  // to start the server.
  HttpServer();
  virtual ~HttpServer();

  // Initializes and waits until the server is ready to accept requests.
  bool InitializeAndWaitUntilReady();

  // Shuts down the http server and waits until the shutdown is complete.
  void ShutdownAndWaitUntilComplete();

  // Checks if the server is started.
  bool Started() const {
    return listen_socket_.get() != NULL;
  }

  // Provides URL to the server which is useful when general purpose provider
  // is registered.
  GURL GetBaseURL();

  // The most general purpose method. Any request processing can be added using
  // this method. Takes ownership of the object. The |callback| is called
  // on UI thread.
  void RegisterRequestHandler(const HandleRequestCallback& callback);

  // Used to provide the same predefined response for the requests matching
  // the |relative_path|. Should be used if any custom data, such as additional
  // headers should be send from the server.
  GURL RegisterDefaultResponse(
      const std::string& relative_path,
      const HttpResponse& default_response);

  // Registers a simple text response.
  GURL RegisterTextResponse(
      const std::string& relative_path,
      const std::string& content,
      const std::string& content_type,
      const ResponseCode response_code);

  // Registers a simple file response. The file is loaded into memory.
  GURL RegisterFileResponse(
      const std::string& relative_path,
      const FilePath& file_path,
      const std::string& content_type,
      const ResponseCode response_code);

 private:
  // Initializes and starts the server. If initialization succeeds, Starts()
  // will return true.
  void InitializeOnIOThread();

  // Shuts down the server.
  void ShutdownOnIOThread();

  // Handles a request when it is parsed. It passes the request to registed
  // request handlers and sends a http response.
  void HandleRequest(HttpConnection* connection,
                     scoped_ptr<HttpRequest> request);

  // net::StreamListenSocket::Delegate overrides:
  virtual void DidAccept(net::StreamListenSocket* server,
                         net::StreamListenSocket* connection) OVERRIDE;
  virtual void DidRead(net::StreamListenSocket* connection,
                       const char* data,
                       int length) OVERRIDE;
  virtual void DidClose(net::StreamListenSocket* connection) OVERRIDE;

  HttpConnection* FindConnection(net::StreamListenSocket* socket);

  scoped_refptr<HttpListenSocket> listen_socket_;
  int port_;
  GURL base_url_;

  // Owns the HttpConnection objects.
  std::map<net::StreamListenSocket*, HttpConnection*> connections_;

  // Vector of registered request handlers.
  std::vector<HandleRequestCallback> request_handlers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<HttpServer> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(HttpServer);
};

}  // namespace test_servers
}  // namespace drive

#endif  // CHROME_BROWSER_GOOGLE_APIS_TEST_SERVER_HTTP_SERVER_H_
