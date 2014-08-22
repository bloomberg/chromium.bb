// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"

class GURL;

namespace net {
class ServerSocket;
class URLRequestContextGetter;
}

namespace content {

class DevToolsHttpHandlerDelegate;

// This class is used for managing DevTools remote debugging server.
// Clients can connect to the specified ip:port and start debugging
// this browser.
class DevToolsHttpHandler {
 public:

  // Factory of net::ServerSocket. This is to separate instantiating dev tools
  // and instantiating server socket.
  class CONTENT_EXPORT ServerSocketFactory {
   public:
    ServerSocketFactory(const std::string& address, int port, int backlog);
    virtual ~ServerSocketFactory();

    // Returns a new instance of ServerSocket or NULL if an error occurred.
    // It calls ServerSocket::ListenWithAddressAndPort() with address, port and
    // backlog passed to constructor.
    virtual scoped_ptr<net::ServerSocket> CreateAndListen() const;

   protected:
    // Creates a server socket. ServerSocket::Listen() will be called soon
    // unless it returns NULL.
    virtual scoped_ptr<net::ServerSocket> Create() const = 0;

    const std::string address_;
    const int port_;
    const int backlog_;

   private:
    DISALLOW_COPY_AND_ASSIGN(ServerSocketFactory);
  };

  // Returns true if the given protocol version is supported.
  CONTENT_EXPORT static bool IsSupportedProtocolVersion(
      const std::string& version);

  // Returns frontend resource id for the given resource |name|.
  CONTENT_EXPORT static int GetFrontendResourceId(
      const std::string& name);

  // Takes ownership over |socket_factory| and |delegate|.
  // If |active_port_output_directory| is non-empty, it is assumed the
  // socket_factory was initialized with an ephemeral port (0). The
  // port selected by the OS will be written to a well-known file in
  // the output directory.
  CONTENT_EXPORT static DevToolsHttpHandler* Start(
      scoped_ptr<ServerSocketFactory> server_socket_factory,
      const std::string& frontend_url,
      DevToolsHttpHandlerDelegate* delegate,
      const base::FilePath& active_port_output_directory);

  // Called from the main thread in order to stop protocol handler.
  // Automatically destroys the handler instance.
  virtual void Stop() = 0;

  // Returns the URL for the address to debug |agent_host|.
  virtual GURL GetFrontendURL() = 0;

 protected:
  virtual ~DevToolsHttpHandler() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_H_
