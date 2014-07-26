// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKETS_TCP_SOCKETS_TCP_API_H_
#define EXTENSIONS_BROWSER_API_SOCKETS_TCP_SOCKETS_TCP_API_H_

#include "extensions/browser/api/socket/socket_api.h"
#include "extensions/common/api/sockets_tcp.h"

namespace extensions {
class ResumableTCPSocket;
class TLSSocket;
}

namespace extensions {
namespace core_api {

class TCPSocketEventDispatcher;

class TCPSocketAsyncApiFunction : public SocketAsyncApiFunction {
 protected:
  virtual ~TCPSocketAsyncApiFunction();

  virtual scoped_ptr<SocketResourceManagerInterface>
      CreateSocketResourceManager() OVERRIDE;

  ResumableTCPSocket* GetTcpSocket(int socket_id);
};

class TCPSocketExtensionWithDnsLookupFunction
    : public SocketExtensionWithDnsLookupFunction {
 protected:
  virtual ~TCPSocketExtensionWithDnsLookupFunction();

  virtual scoped_ptr<SocketResourceManagerInterface>
      CreateSocketResourceManager() OVERRIDE;

  ResumableTCPSocket* GetTcpSocket(int socket_id);
};

class SocketsTcpCreateFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.create", SOCKETS_TCP_CREATE)

  SocketsTcpCreateFunction();

 protected:
  virtual ~SocketsTcpCreateFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(SocketsTcpUnitTest, Create);
  scoped_ptr<sockets_tcp::Create::Params> params_;
};

class SocketsTcpUpdateFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.update", SOCKETS_TCP_UPDATE)

  SocketsTcpUpdateFunction();

 protected:
  virtual ~SocketsTcpUpdateFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_tcp::Update::Params> params_;
};

class SocketsTcpSetPausedFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.setPaused", SOCKETS_TCP_SETPAUSED)

  SocketsTcpSetPausedFunction();

 protected:
  virtual ~SocketsTcpSetPausedFunction();

  // AsyncApiFunction
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_tcp::SetPaused::Params> params_;
  TCPSocketEventDispatcher* socket_event_dispatcher_;
};

class SocketsTcpSetKeepAliveFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.setKeepAlive",
                             SOCKETS_TCP_SETKEEPALIVE)

  SocketsTcpSetKeepAliveFunction();

 protected:
  virtual ~SocketsTcpSetKeepAliveFunction();

  // AsyncApiFunction
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_tcp::SetKeepAlive::Params> params_;
};

class SocketsTcpSetNoDelayFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.setNoDelay", SOCKETS_TCP_SETNODELAY)

  SocketsTcpSetNoDelayFunction();

 protected:
  virtual ~SocketsTcpSetNoDelayFunction();

  // AsyncApiFunction
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_tcp::SetNoDelay::Params> params_;
};

class SocketsTcpConnectFunction
    : public TCPSocketExtensionWithDnsLookupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.connect", SOCKETS_TCP_CONNECT)

  SocketsTcpConnectFunction();

 protected:
  virtual ~SocketsTcpConnectFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

  // SocketExtensionWithDnsLookupFunction:
  virtual void AfterDnsLookup(int lookup_result) OVERRIDE;

 private:
  void StartConnect();
  void OnCompleted(int net_result);

  scoped_ptr<sockets_tcp::Connect::Params> params_;
  TCPSocketEventDispatcher* socket_event_dispatcher_;
};

class SocketsTcpDisconnectFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.disconnect", SOCKETS_TCP_DISCONNECT)

  SocketsTcpDisconnectFunction();

 protected:
  virtual ~SocketsTcpDisconnectFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_tcp::Disconnect::Params> params_;
};

class SocketsTcpSendFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.send", SOCKETS_TCP_SEND)

  SocketsTcpSendFunction();

 protected:
  virtual ~SocketsTcpSendFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  void OnCompleted(int net_result);
  void SetSendResult(int net_result, int bytes_sent);

  scoped_ptr<sockets_tcp::Send::Params> params_;
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t io_buffer_size_;
};

class SocketsTcpCloseFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.close", SOCKETS_TCP_CLOSE)

  SocketsTcpCloseFunction();

 protected:
  virtual ~SocketsTcpCloseFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_tcp::Close::Params> params_;
};

class SocketsTcpGetInfoFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.getInfo", SOCKETS_TCP_GETINFO)

  SocketsTcpGetInfoFunction();

 protected:
  virtual ~SocketsTcpGetInfoFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_tcp::GetInfo::Params> params_;
};

class SocketsTcpGetSocketsFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.getSockets", SOCKETS_TCP_GETSOCKETS)

  SocketsTcpGetSocketsFunction();

 protected:
  virtual ~SocketsTcpGetSocketsFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
};

class SocketsTcpSecureFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.secure", SOCKETS_TCP_SECURE);

  SocketsTcpSecureFunction();

 protected:
  virtual ~SocketsTcpSecureFunction();
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  virtual void TlsConnectDone(scoped_ptr<extensions::TLSSocket> sock,
                              int result);

  bool paused_;
  bool persistent_;
  scoped_ptr<sockets_tcp::Secure::Params> params_;
  scoped_refptr<net::URLRequestContextGetter> url_request_getter_;

  DISALLOW_COPY_AND_ASSIGN(SocketsTcpSecureFunction);
};

}  // namespace core_api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKETS_TCP_SOCKETS_TCP_API_H_
