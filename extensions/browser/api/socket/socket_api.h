// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKET_SOCKET_API_H_
#define EXTENSIONS_BROWSER_API_SOCKET_SOCKET_API_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/async_api_function.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/socket.h"
#include "net/base/address_list.h"
#include "net/dns/host_resolver.h"
#include "net/socket/tcp_client_socket.h"

namespace chromeos {
class FirewallHole;
}

namespace content {
class BrowserContext;
class ResourceContext;
}

namespace net {
class IOBuffer;
class URLRequestContextGetter;
class SSLClientSocket;
}

namespace extensions {
class TLSSocket;
class Socket;

// A simple interface to ApiResourceManager<Socket> or derived class. The goal
// of this interface is to allow Socket API functions to use distinct instances
// of ApiResourceManager<> depending on the type of socket (old version in
// "socket" namespace vs new version in "socket.xxx" namespaces).
class SocketResourceManagerInterface {
 public:
  virtual ~SocketResourceManagerInterface() {}

  virtual bool SetBrowserContext(content::BrowserContext* context) = 0;
  virtual int Add(Socket* socket) = 0;
  virtual Socket* Get(const std::string& extension_id, int api_resource_id) = 0;
  virtual void Remove(const std::string& extension_id, int api_resource_id) = 0;
  virtual void Replace(const std::string& extension_id,
                       int api_resource_id,
                       Socket* socket) = 0;
  virtual base::hash_set<int>* GetResourceIds(
      const std::string& extension_id) = 0;
};

// Implementation of SocketResourceManagerInterface using an
// ApiResourceManager<T> instance (where T derives from Socket).
template <typename T>
class SocketResourceManager : public SocketResourceManagerInterface {
 public:
  SocketResourceManager() : manager_(NULL) {}

  virtual bool SetBrowserContext(content::BrowserContext* context) override {
    manager_ = ApiResourceManager<T>::Get(context);
    DCHECK(manager_)
        << "There is no socket manager. "
           "If this assertion is failing during a test, then it is likely that "
           "TestExtensionSystem is failing to provide an instance of "
           "ApiResourceManager<Socket>.";
    return manager_ != NULL;
  }

  virtual int Add(Socket* socket) override {
    // Note: Cast needed here, because "T" may be a subclass of "Socket".
    return manager_->Add(static_cast<T*>(socket));
  }

  virtual Socket* Get(const std::string& extension_id,
                      int api_resource_id) override {
    return manager_->Get(extension_id, api_resource_id);
  }

  virtual void Replace(const std::string& extension_id,
                       int api_resource_id,
                       Socket* socket) override {
    manager_->Replace(extension_id, api_resource_id, static_cast<T*>(socket));
  }

  virtual void Remove(const std::string& extension_id,
                      int api_resource_id) override {
    manager_->Remove(extension_id, api_resource_id);
  }

  virtual base::hash_set<int>* GetResourceIds(const std::string& extension_id)
      override {
    return manager_->GetResourceIds(extension_id);
  }

 private:
  ApiResourceManager<T>* manager_;
};

class SocketAsyncApiFunction : public AsyncApiFunction {
 public:
  SocketAsyncApiFunction();

 protected:
  ~SocketAsyncApiFunction() override;

  // AsyncApiFunction:
  bool PrePrepare() override;
  bool Respond() override;

  virtual scoped_ptr<SocketResourceManagerInterface>
      CreateSocketResourceManager();

  int AddSocket(Socket* socket);
  Socket* GetSocket(int api_resource_id);
  void ReplaceSocket(int api_resource_id, Socket* socket);
  void RemoveSocket(int api_resource_id);
  base::hash_set<int>* GetSocketIds();

  // Only implemented on Chrome OS.
  void OpenFirewallHole(const std::string& address,
                        int socket_id,
                        Socket* socket);

 private:
#if defined(OS_CHROMEOS)
  void OnFirewallHoleOpenedOnUIThread(int socket_id,
                                      scoped_ptr<chromeos::FirewallHole> hole);
  void OnFirewallHoleOpened(int socket_id,
                            scoped_ptr<chromeos::FirewallHole> hole);
#endif  // OS_CHROMEOS

  scoped_ptr<SocketResourceManagerInterface> manager_;
};

class SocketExtensionWithDnsLookupFunction : public SocketAsyncApiFunction {
 protected:
  SocketExtensionWithDnsLookupFunction();
  ~SocketExtensionWithDnsLookupFunction() override;

  // AsyncApiFunction:
  bool PrePrepare() override;

  void StartDnsLookup(const std::string& hostname);
  virtual void AfterDnsLookup(int lookup_result) = 0;

  std::string resolved_address_;

 private:
  void OnDnsLookup(int resolve_result);

  // Weak pointer to the resource context.
  content::ResourceContext* resource_context_;

  scoped_ptr<net::HostResolver::RequestHandle> request_handle_;
  scoped_ptr<net::AddressList> addresses_;
};

class SocketCreateFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.create", SOCKET_CREATE)

  SocketCreateFunction();

 protected:
  ~SocketCreateFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SocketUnitTest, Create);
  enum SocketType { kSocketTypeInvalid = -1, kSocketTypeTCP, kSocketTypeUDP };

  scoped_ptr<core_api::socket::Create::Params> params_;
  SocketType socket_type_;
};

class SocketDestroyFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.destroy", SOCKET_DESTROY)

 protected:
  ~SocketDestroyFunction() override {}

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  int socket_id_;
};

class SocketConnectFunction : public SocketExtensionWithDnsLookupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.connect", SOCKET_CONNECT)

  SocketConnectFunction();

 protected:
  ~SocketConnectFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;

  // SocketExtensionWithDnsLookupFunction:
  void AfterDnsLookup(int lookup_result) override;

 private:
  void StartConnect();
  void OnConnect(int result);

  int socket_id_;
  std::string hostname_;
  uint16 port_;
};

class SocketDisconnectFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.disconnect", SOCKET_DISCONNECT)

 protected:
  ~SocketDisconnectFunction() override {}

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  int socket_id_;
};

class SocketBindFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.bind", SOCKET_BIND)

 protected:
  ~SocketBindFunction() override {}

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  int socket_id_;
  std::string address_;
  uint16 port_;
};

class SocketListenFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.listen", SOCKET_LISTEN)

  SocketListenFunction();

 protected:
  ~SocketListenFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  scoped_ptr<core_api::socket::Listen::Params> params_;
};

class SocketAcceptFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.accept", SOCKET_ACCEPT)

  SocketAcceptFunction();

 protected:
  ~SocketAcceptFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  void OnAccept(int result_code, net::TCPClientSocket* socket);

  scoped_ptr<core_api::socket::Accept::Params> params_;
};

class SocketReadFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.read", SOCKET_READ)

  SocketReadFunction();

 protected:
  ~SocketReadFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;
  void OnCompleted(int result, scoped_refptr<net::IOBuffer> io_buffer);

 private:
  scoped_ptr<core_api::socket::Read::Params> params_;
};

class SocketWriteFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.write", SOCKET_WRITE)

  SocketWriteFunction();

 protected:
  ~SocketWriteFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;
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
  ~SocketRecvFromFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void AsyncWorkStart() override;
  void OnCompleted(int result,
                   scoped_refptr<net::IOBuffer> io_buffer,
                   const std::string& address,
                   uint16 port);

 private:
  scoped_ptr<core_api::socket::RecvFrom::Params> params_;
};

class SocketSendToFunction : public SocketExtensionWithDnsLookupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.sendTo", SOCKET_SENDTO)

  SocketSendToFunction();

 protected:
  ~SocketSendToFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;
  void OnCompleted(int result);

  // SocketExtensionWithDnsLookupFunction:
  void AfterDnsLookup(int lookup_result) override;

 private:
  void StartSendTo();

  int socket_id_;
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t io_buffer_size_;
  std::string hostname_;
  uint16 port_;
};

class SocketSetKeepAliveFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.setKeepAlive", SOCKET_SETKEEPALIVE)

  SocketSetKeepAliveFunction();

 protected:
  ~SocketSetKeepAliveFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  scoped_ptr<core_api::socket::SetKeepAlive::Params> params_;
};

class SocketSetNoDelayFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.setNoDelay", SOCKET_SETNODELAY)

  SocketSetNoDelayFunction();

 protected:
  ~SocketSetNoDelayFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  scoped_ptr<core_api::socket::SetNoDelay::Params> params_;
};

class SocketGetInfoFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.getInfo", SOCKET_GETINFO)

  SocketGetInfoFunction();

 protected:
  ~SocketGetInfoFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  scoped_ptr<core_api::socket::GetInfo::Params> params_;
};

class SocketGetNetworkListFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.getNetworkList", SOCKET_GETNETWORKLIST)

 protected:
  ~SocketGetNetworkListFunction() override {}
  bool RunAsync() override;

 private:
  void GetNetworkListOnFileThread();
  void HandleGetNetworkListError();
  void SendResponseOnUIThread(const net::NetworkInterfaceList& interface_list);
};

class SocketJoinGroupFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.joinGroup", SOCKET_MULTICAST_JOIN_GROUP)

  SocketJoinGroupFunction();

 protected:
  ~SocketJoinGroupFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void Work() override;

 private:
  scoped_ptr<core_api::socket::JoinGroup::Params> params_;
};

class SocketLeaveGroupFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.leaveGroup", SOCKET_MULTICAST_LEAVE_GROUP)

  SocketLeaveGroupFunction();

 protected:
  ~SocketLeaveGroupFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void Work() override;

 private:
  scoped_ptr<core_api::socket::LeaveGroup::Params> params_;
};

class SocketSetMulticastTimeToLiveFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.setMulticastTimeToLive",
                             SOCKET_MULTICAST_SET_TIME_TO_LIVE)

  SocketSetMulticastTimeToLiveFunction();

 protected:
  ~SocketSetMulticastTimeToLiveFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void Work() override;

 private:
  scoped_ptr<core_api::socket::SetMulticastTimeToLive::Params> params_;
};

class SocketSetMulticastLoopbackModeFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.setMulticastLoopbackMode",
                             SOCKET_MULTICAST_SET_LOOPBACK_MODE)

  SocketSetMulticastLoopbackModeFunction();

 protected:
  ~SocketSetMulticastLoopbackModeFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void Work() override;

 private:
  scoped_ptr<core_api::socket::SetMulticastLoopbackMode::Params> params_;
};

class SocketGetJoinedGroupsFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.getJoinedGroups",
                             SOCKET_MULTICAST_GET_JOINED_GROUPS)

  SocketGetJoinedGroupsFunction();

 protected:
  ~SocketGetJoinedGroupsFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void Work() override;

 private:
  scoped_ptr<core_api::socket::GetJoinedGroups::Params> params_;
};

class SocketSecureFunction : public SocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("socket.secure", SOCKET_SECURE);
  SocketSecureFunction();

 protected:
  ~SocketSecureFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  // Callback from TLSSocket::UpgradeSocketToTLS().
  void TlsConnectDone(scoped_ptr<TLSSocket> socket, int result);

  scoped_ptr<core_api::socket::Secure::Params> params_;
  scoped_refptr<net::URLRequestContextGetter> url_request_getter_;

  DISALLOW_COPY_AND_ASSIGN(SocketSecureFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKET_SOCKET_API_H_
