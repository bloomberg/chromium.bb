// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_H_

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/api_function.h"
#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/api/socket.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/socket/tcp_client_socket.h"

#include <string>

class IOThread;

namespace net {
class IOBuffer;
}

namespace extensions {

class Socket;

class SocketAsyncApiFunction : public AsyncApiFunction {
 public:
  SocketAsyncApiFunction();

 protected:
  virtual ~SocketAsyncApiFunction();

  // AsyncApiFunction:
  virtual bool PrePrepare() OVERRIDE;
  virtual bool Respond() OVERRIDE;

  Socket* GetSocket(int api_resource_id);
  void RemoveSocket(int api_resource_id);

  ApiResourceManager<Socket>* manager_;
};

class SocketExtensionWithDnsLookupFunction : public SocketAsyncApiFunction {
 protected:
  SocketExtensionWithDnsLookupFunction();
  virtual ~SocketExtensionWithDnsLookupFunction();

  void StartDnsLookup(const std::string& hostname);
  virtual void AfterDnsLookup(int lookup_result) = 0;

  std::string resolved_address_;

 private:
  void OnDnsLookup(int resolve_result);

  // This instance is widely available through BrowserProcess, but we need to
  // acquire it on the UI thread and then use it on the IO thread, so we keep a
  // plain pointer to it here as we move from thread to thread.
  IOThread* io_thread_;

  scoped_ptr<net::HostResolver::RequestHandle> request_handle_;
  scoped_ptr<net::AddressList> addresses_;
};

class SocketCreateFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.create", SOCKET_CREATE)

  SocketCreateFunction();

 protected:
  virtual ~SocketCreateFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  enum SocketType {
    kSocketTypeInvalid = -1,
    kSocketTypeTCP,
    kSocketTypeUDP
  };

  scoped_ptr<api::socket::Create::Params> params_;
  SocketType socket_type_;
};

class SocketDestroyFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.destroy", SOCKET_DESTROY)

 protected:
  virtual ~SocketDestroyFunction() {}

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  int socket_id_;
};

class SocketConnectFunction : public SocketExtensionWithDnsLookupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.connect", SOCKET_CONNECT)

  SocketConnectFunction();

 protected:
  virtual ~SocketConnectFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

  // SocketExtensionWithDnsLookupFunction:
  virtual void AfterDnsLookup(int lookup_result) OVERRIDE;

 private:
  void StartConnect();
  void OnConnect(int result);

  int socket_id_;
  std::string hostname_;
  int port_;
  Socket* socket_;
};

class SocketDisconnectFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.disconnect", SOCKET_DISCONNECT)

 protected:
  virtual ~SocketDisconnectFunction() {}

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  int socket_id_;
};

class SocketBindFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.bind", SOCKET_BIND)

 protected:
  virtual ~SocketBindFunction() {}

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  int socket_id_;
  std::string address_;
  int port_;
};

class SocketListenFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.listen", SOCKET_LISTEN)

  SocketListenFunction();

 protected:
  virtual ~SocketListenFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<api::socket::Listen::Params> params_;
};

class SocketAcceptFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.accept", SOCKET_ACCEPT)

  SocketAcceptFunction();

 protected:
  virtual ~SocketAcceptFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  void OnAccept(int result_code, net::TCPClientSocket *socket);
  scoped_ptr<api::socket::Accept::Params> params_;
};

class SocketReadFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.read", SOCKET_READ)

  SocketReadFunction();

 protected:
  virtual ~SocketReadFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;
  void OnCompleted(int result, scoped_refptr<net::IOBuffer> io_buffer);

 private:
  scoped_ptr<api::socket::Read::Params> params_;
};

class SocketWriteFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.write", SOCKET_WRITE)

  SocketWriteFunction();

 protected:
  virtual ~SocketWriteFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;
  void OnCompleted(int result);

 private:
  int socket_id_;
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t io_buffer_size_;
};

class SocketRecvFromFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.recvFrom", SOCKET_RECVFROM)

  SocketRecvFromFunction();

 protected:
  virtual ~SocketRecvFromFunction();

  // AsyncApiFunction
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;
  void OnCompleted(int result,
                   scoped_refptr<net::IOBuffer> io_buffer,
                   const std::string& address,
                   int port);

 private:
  scoped_ptr<api::socket::RecvFrom::Params> params_;
};

class SocketSendToFunction : public SocketExtensionWithDnsLookupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.sendTo", SOCKET_SENDTO)

  SocketSendToFunction();

 protected:
  virtual ~SocketSendToFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;
  void OnCompleted(int result);

  // SocketExtensionWithDnsLookupFunction:
  virtual void AfterDnsLookup(int lookup_result) OVERRIDE;

 private:
  void StartSendTo();

  int socket_id_;
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t io_buffer_size_;
  std::string hostname_;
  int port_;
  Socket* socket_;
};

class SocketSetKeepAliveFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.setKeepAlive", SOCKET_SETKEEPALIVE)

  SocketSetKeepAliveFunction();

 protected:
  virtual ~SocketSetKeepAliveFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<api::socket::SetKeepAlive::Params> params_;
};

class SocketSetNoDelayFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.setNoDelay", SOCKET_SETNODELAY)

  SocketSetNoDelayFunction();

 protected:
  virtual ~SocketSetNoDelayFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<api::socket::SetNoDelay::Params> params_;
};

class SocketGetInfoFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.getInfo", SOCKET_GETINFO)

  SocketGetInfoFunction();

 protected:
  virtual ~SocketGetInfoFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<api::socket::GetInfo::Params> params_;
};

class SocketGetNetworkListFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.getNetworkList", SOCKET_GETNETWORKLIST)

 protected:
  virtual ~SocketGetNetworkListFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  void GetNetworkListOnFileThread();
  void HandleGetNetworkListError();
  void SendResponseOnUIThread(const net::NetworkInterfaceList& interface_list);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKET_SOCKET_API_H_
