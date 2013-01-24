// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_TEST_SERVER_HTTP_CONNECTION_H_
#define CHROME_BROWSER_GOOGLE_APIS_TEST_SERVER_HTTP_CONNECTION_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/string_piece.h"
#include "chrome/browser/google_apis/test_server/http_request.h"

namespace net {
class StreamListenSocket;
}

namespace google_apis {
namespace test_server {

class HttpConnection;
class HttpResponse;

// Calblack called when a request is parsed. Response should be sent
// using HttpConnection::SendResponse() on the |connection| argument.
typedef base::Callback<void(HttpConnection* connection,
                            scoped_ptr<HttpRequest> request)>
    HandleRequestCallback;

// Wraps the connection socket. Accepts incoming data and sends responses.
// If a valid request is parsed, then |callback_| is invoked.
class HttpConnection {
 public:
  HttpConnection(net::StreamListenSocket* socket,
                 const HandleRequestCallback& callback);
  ~HttpConnection();

  // Sends the HTTP response to the client.
  void SendResponse(scoped_ptr<HttpResponse> response) const;

 private:
  friend class HttpServer;

  // Accepts raw chunk of data from the client. Internally, passes it to the
  // HttpRequestParser class. If a request is parsed, then |callback_| is
  // called.
  void ReceiveData(const base::StringPiece& data);

  scoped_refptr<net::StreamListenSocket> socket_;
  const HandleRequestCallback callback_;
  HttpRequestParser request_parser_;

  DISALLOW_COPY_AND_ASSIGN(HttpConnection);
};

}  // namespace test_server
}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_TEST_SERVER_HTTP_CONNECTION_H_
