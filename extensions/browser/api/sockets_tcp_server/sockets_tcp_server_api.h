// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKETS_TCP_SERVER_SOCKETS_TCP_SERVER_API_H_
#define EXTENSIONS_BROWSER_API_SOCKETS_TCP_SERVER_SOCKETS_TCP_SERVER_API_H_

#include "extensions/browser/api/socket/socket_api.h"
#include "extensions/common/api/sockets_tcp_server.h"

namespace extensions {
class ResumableTCPServerSocket;
}

namespace extensions {
namespace core_api {

class TCPServerSocketAsyncApiFunction : public SocketAsyncApiFunction {
 protected:
  virtual ~TCPServerSocketAsyncApiFunction();

  virtual scoped_ptr<SocketResourceManagerInterface>
      CreateSocketResourceManager() OVERRIDE;

  ResumableTCPServerSocket* GetTcpSocket(int socket_id);
};

class SocketsTcpServerCreateFunction : public TCPServerSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.create",
                             SOCKETS_TCP_SERVER_CREATE)

  SocketsTcpServerCreateFunction();

 protected:
  virtual ~SocketsTcpServerCreateFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(SocketsTcpServerUnitTest, Create);
  scoped_ptr<sockets_tcp_server::Create::Params> params_;
};

class SocketsTcpServerUpdateFunction : public TCPServerSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.update",
                             SOCKETS_TCP_SERVER_UPDATE)

  SocketsTcpServerUpdateFunction();

 protected:
  virtual ~SocketsTcpServerUpdateFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_tcp_server::Update::Params> params_;
};

class SocketsTcpServerSetPausedFunction
    : public TCPServerSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.setPaused",
                             SOCKETS_TCP_SERVER_SETPAUSED)

  SocketsTcpServerSetPausedFunction();

 protected:
  virtual ~SocketsTcpServerSetPausedFunction();

  // AsyncApiFunction
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_tcp_server::SetPaused::Params> params_;
  TCPServerSocketEventDispatcher* socket_event_dispatcher_;
};

class SocketsTcpServerListenFunction : public TCPServerSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.listen",
                             SOCKETS_TCP_SERVER_LISTEN)

  SocketsTcpServerListenFunction();

 protected:
  virtual ~SocketsTcpServerListenFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_tcp_server::Listen::Params> params_;
  TCPServerSocketEventDispatcher* socket_event_dispatcher_;
};

class SocketsTcpServerDisconnectFunction
    : public TCPServerSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.disconnect",
                             SOCKETS_TCP_SERVER_DISCONNECT)

  SocketsTcpServerDisconnectFunction();

 protected:
  virtual ~SocketsTcpServerDisconnectFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_tcp_server::Disconnect::Params> params_;
};

class SocketsTcpServerCloseFunction : public TCPServerSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.close",
                             SOCKETS_TCP_SERVER_CLOSE)

  SocketsTcpServerCloseFunction();

 protected:
  virtual ~SocketsTcpServerCloseFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_tcp_server::Close::Params> params_;
};

class SocketsTcpServerGetInfoFunction : public TCPServerSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.getInfo",
                             SOCKETS_TCP_SERVER_GETINFO)

  SocketsTcpServerGetInfoFunction();

 protected:
  virtual ~SocketsTcpServerGetInfoFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_tcp_server::GetInfo::Params> params_;
};

class SocketsTcpServerGetSocketsFunction
    : public TCPServerSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.getSockets",
                             SOCKETS_TCP_SERVER_GETSOCKETS)

  SocketsTcpServerGetSocketsFunction();

 protected:
  virtual ~SocketsTcpServerGetSocketsFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
};

}  // namespace core_api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKETS_TCP_SERVER_SOCKETS_TCP_SERVER_API_H_
