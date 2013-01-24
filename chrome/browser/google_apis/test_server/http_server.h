// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_TEST_SERVER_HTTP_SERVER_H_
#define CHROME_BROWSER_GOOGLE_APIS_TEST_SERVER_HTTP_SERVER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "googleurl/src/gurl.h"
#include "net/base/tcp_listen_socket.h"

namespace google_apis {
namespace test_server {

class HttpConnection;
class HttpResponse;
struct HttpRequest;

// This class is required to be able to have composition instead of inheritance,
class HttpListenSocket: public net::TCPListenSocket {
 public:
  HttpListenSocket(const SocketDescriptor socket_descriptor,
                   net::StreamListenSocket::Delegate* delegate);
  virtual void Listen();

 private:
  virtual ~HttpListenSocket();
};

// Class providing an HTTP server for testing purpose. This is a basic server
// providing only an essential subset of HTTP/1.1 protocol. Especially,
// it assumes that the request syntax is correct. It *does not* support
// a Chunked Transfer Encoding.
//
// The common use case is below:
//
// scoped_ptr<HttpServer> test_server_;
//
// void SetUp() {
//   test_server_.reset(new HttpServer());
//   DCHECK(test_server_.InitializeAndWaitUntilReady());
//   test_server_->RegisterRequestHandler(
//       base::Bind(&FooTest::HandleRequest, base::Unretained(this)));
// }
//
// scoped_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
//   GURL absolute_url = test_server_->GetURL(request.relative_url);
//   if (absolute_url.path() != "/test")
//     return scoped_ptr<HttpResponse>();
//
//   scoped_ptr<HttpResponse> http_response(new HttpResponse());
//   http_response->set_code(test_server::SUCCESS);
//   http_response->set_content("hello");
//   http_response->set_content_type("text/plain");
//   return http_response.Pass();
// }
//
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

  // Returns the base URL to the server, which looks like
  // http://127.0.0.1:<port>/, where <port> is the actual port number used by
  // the server.
  const GURL& base_url() const { return base_url_; }

  // Returns a URL to the server based on the given relative URL, which
  // should start with '/'. For example: GetURL("/path?query=foo") =>
  // http://127.0.0.1:<port>/path?query=foo.
  GURL GetURL(const std::string& relative_url) const;

  // Returns the port number used by the server.
  int port() const { return port_; }

  // The most general purpose method. Any request processing can be added using
  // this method. Takes ownership of the object. The |callback| is called
  // on UI thread.
  void RegisterRequestHandler(const HandleRequestCallback& callback);

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
}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_TEST_SERVER_HTTP_SERVER_H_
