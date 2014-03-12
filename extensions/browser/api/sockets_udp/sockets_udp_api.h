// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKETS_UDP_SOCKETS_UDP_API_H_
#define EXTENSIONS_BROWSER_API_SOCKETS_UDP_SOCKETS_UDP_API_H_

#include "extensions/browser/api/socket/socket_api.h"
#include "extensions/common/api/sockets_udp.h"

namespace extensions {
class ResumableUDPSocket;
}

namespace extensions {
namespace core_api {

class UDPSocketEventDispatcher;

class UDPSocketAsyncApiFunction : public SocketAsyncApiFunction {
 protected:
  virtual ~UDPSocketAsyncApiFunction();

  virtual scoped_ptr<SocketResourceManagerInterface>
      CreateSocketResourceManager() OVERRIDE;

  ResumableUDPSocket* GetUdpSocket(int socket_id);
};

class UDPSocketExtensionWithDnsLookupFunction
    : public SocketExtensionWithDnsLookupFunction {
 protected:
  virtual ~UDPSocketExtensionWithDnsLookupFunction();

  virtual scoped_ptr<SocketResourceManagerInterface>
      CreateSocketResourceManager() OVERRIDE;

  ResumableUDPSocket* GetUdpSocket(int socket_id);
};

class SocketsUdpCreateFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.create", SOCKETS_UDP_CREATE)

  SocketsUdpCreateFunction();

 protected:
  virtual ~SocketsUdpCreateFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(SocketsUdpUnitTest, Create);
  scoped_ptr<sockets_udp::Create::Params> params_;
};

class SocketsUdpUpdateFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.update", SOCKETS_UDP_UPDATE)

  SocketsUdpUpdateFunction();

 protected:
  virtual ~SocketsUdpUpdateFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_udp::Update::Params> params_;
};

class SocketsUdpSetPausedFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.setPaused", SOCKETS_UDP_SETPAUSED)

  SocketsUdpSetPausedFunction();

 protected:
  virtual ~SocketsUdpSetPausedFunction();

  // AsyncApiFunction
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_udp::SetPaused::Params> params_;
  UDPSocketEventDispatcher* socket_event_dispatcher_;
};

class SocketsUdpBindFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.bind", SOCKETS_UDP_BIND)

  SocketsUdpBindFunction();

 protected:
  virtual ~SocketsUdpBindFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_udp::Bind::Params> params_;
  UDPSocketEventDispatcher* socket_event_dispatcher_;
};

class SocketsUdpSendFunction : public UDPSocketExtensionWithDnsLookupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.send", SOCKETS_UDP_SEND)

  SocketsUdpSendFunction();

 protected:
  virtual ~SocketsUdpSendFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;
  void OnCompleted(int net_result);
  void SetSendResult(int net_result, int bytes_sent);

  // SocketExtensionWithDnsLookupFunction:
  virtual void AfterDnsLookup(int lookup_result) OVERRIDE;

 private:
  void StartSendTo();

  scoped_ptr<sockets_udp::Send::Params> params_;
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t io_buffer_size_;
};

class SocketsUdpCloseFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.close", SOCKETS_UDP_CLOSE)

  SocketsUdpCloseFunction();

 protected:
  virtual ~SocketsUdpCloseFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_udp::Close::Params> params_;
};

class SocketsUdpGetInfoFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.getInfo", SOCKETS_UDP_GETINFO)

  SocketsUdpGetInfoFunction();

 protected:
  virtual ~SocketsUdpGetInfoFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_udp::GetInfo::Params> params_;
};

class SocketsUdpGetSocketsFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.getSockets", SOCKETS_UDP_GETSOCKETS)

  SocketsUdpGetSocketsFunction();

 protected:
  virtual ~SocketsUdpGetSocketsFunction();

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
};

class SocketsUdpJoinGroupFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.joinGroup", SOCKETS_UDP_JOINGROUP)

  SocketsUdpJoinGroupFunction();

 protected:
  virtual ~SocketsUdpJoinGroupFunction();

  // AsyncApiFunction
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_udp::JoinGroup::Params> params_;
};

class SocketsUdpLeaveGroupFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.leaveGroup", SOCKETS_UDP_LEAVEGROUP)

  SocketsUdpLeaveGroupFunction();

 protected:
  virtual ~SocketsUdpLeaveGroupFunction();

  // AsyncApiFunction
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_udp::LeaveGroup::Params> params_;
};

class SocketsUdpSetMulticastTimeToLiveFunction
    : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.setMulticastTimeToLive",
                             SOCKETS_UDP_SETMULTICASTTIMETOLIVE)

  SocketsUdpSetMulticastTimeToLiveFunction();

 protected:
  virtual ~SocketsUdpSetMulticastTimeToLiveFunction();

  // AsyncApiFunction
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_udp::SetMulticastTimeToLive::Params> params_;
};

class SocketsUdpSetMulticastLoopbackModeFunction
    : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.setMulticastLoopbackMode",
                             SOCKETS_UDP_SETMULTICASTLOOPBACKMODE)

  SocketsUdpSetMulticastLoopbackModeFunction();

 protected:
  virtual ~SocketsUdpSetMulticastLoopbackModeFunction();

  // AsyncApiFunction
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_udp::SetMulticastLoopbackMode::Params> params_;
};

class SocketsUdpGetJoinedGroupsFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.getJoinedGroups",
                             SOCKETS_UDP_GETJOINEDGROUPS)

  SocketsUdpGetJoinedGroupsFunction();

 protected:
  virtual ~SocketsUdpGetJoinedGroupsFunction();

  // AsyncApiFunction
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  scoped_ptr<sockets_udp::GetJoinedGroups::Params> params_;
};

}  // namespace core_api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKETS_UDP_SOCKETS_UDP_API_H_
