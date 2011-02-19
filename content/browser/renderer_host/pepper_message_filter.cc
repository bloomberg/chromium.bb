// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper_message_filter.h"

#include "base/basictypes.h"
#include "base/process_util.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/common/pepper_messages.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/url_request/url_request_context.h"
#include "webkit/plugins/ppapi/ppb_flash_impl.h"

#if defined(ENABLE_FLAPPER_HACKS)
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// Make sure the storage in |PP_Flash_NetAddress| is big enough. (Do it here
// since the data is opaque elsewhere.)
COMPILE_ASSERT(sizeof(reinterpret_cast<PP_Flash_NetAddress*>(0)->data) >=
               sizeof(sockaddr_storage), PP_Flash_NetAddress_data_too_small);
#endif  // ENABLE_FLAPPER_HACKS

const PP_Flash_NetAddress kInvalidNetAddress = { 0 };

PepperMessageFilter::PepperMessageFilter(Profile* profile)
    : profile_(profile),
      request_context_(profile_->GetRequestContext()) {
}

PepperMessageFilter::~PepperMessageFilter() {}

bool PepperMessageFilter::OnMessageReceived(const IPC::Message& msg,
                                            bool* message_was_ok) {
#if defined(ENABLE_FLAPPER_HACKS)
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(PepperMessageFilter, msg, *message_was_ok)
    IPC_MESSAGE_HANDLER(PepperMsg_ConnectTcp, OnConnectTcp)
    IPC_MESSAGE_HANDLER(PepperMsg_ConnectTcpAddress, OnConnectTcpAddress)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
#else
  return false;
#endif  // ENABLE_FLAPPER_HACKS
}

#if defined(ENABLE_FLAPPER_HACKS)

namespace {

bool ValidateNetAddress(const PP_Flash_NetAddress& addr) {
  if (addr.size < sizeof(sa_family_t))
    return false;

  // TODO(viettrungluu): more careful validation?
  // Just do a size check for AF_INET.
  if (reinterpret_cast<const sockaddr*>(addr.data)->sa_family == AF_INET &&
      addr.size >= sizeof(sockaddr_in))
    return true;

  // Ditto for AF_INET6.
  if (reinterpret_cast<const sockaddr*>(addr.data)->sa_family == AF_INET6 &&
      addr.size >= sizeof(sockaddr_in6))
    return true;

  // Reject everything else.
  return false;
}

PP_Flash_NetAddress SockaddrToNetAddress(const struct sockaddr* sa,
                                         socklen_t sa_length) {
  PP_Flash_NetAddress addr;
  CHECK_LE(sa_length, sizeof(addr.data));
  addr.size = sa_length;
  memcpy(addr.data, sa, addr.size);
  return addr;
}

int ConnectTcpSocket(const PP_Flash_NetAddress& addr,
                     PP_Flash_NetAddress* local_addr_out,
                     PP_Flash_NetAddress* remote_addr_out) {
  *local_addr_out = kInvalidNetAddress;
  *remote_addr_out = kInvalidNetAddress;

  const struct sockaddr* sa =
      reinterpret_cast<const struct sockaddr*>(addr.data);
  socklen_t sa_len = addr.size;
  int fd = socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP);
  if (fd == -1)
    return -1;
  if (connect(fd, sa, sa_len) != 0) {
    close(fd);
    return -1;
  }

  // Get the local address.
  socklen_t local_length = sizeof(local_addr_out->data);
  if (getsockname(fd, reinterpret_cast<struct sockaddr*>(local_addr_out->data),
                  &local_length) == -1 ||
      local_length > sizeof(local_addr_out->data)) {
    close(fd);
    return -1;
  }

  // The remote address is just the address we connected to.
  *remote_addr_out = addr;

  return fd;
}

}  // namespace

class PepperMessageFilter::LookupRequest {
 public:
  LookupRequest(PepperMessageFilter* pepper_message_filter,
                net::HostResolver* resolver,
                int routing_id,
                int request_id,
                const net::HostResolver::RequestInfo& request_info)
      : ALLOW_THIS_IN_INITIALIZER_LIST(
            net_callback_(this, &LookupRequest::OnLookupFinished)),
        pepper_message_filter_(pepper_message_filter),
        resolver_(resolver),
        routing_id_(routing_id),
        request_id_(request_id),
        request_info_(request_info) {
  }

  void Start() {
    int result = resolver_.Resolve(request_info_, &addresses_, &net_callback_,
                                   net::BoundNetLog());
    if (result != net::ERR_IO_PENDING)
      OnLookupFinished(result);
  }

 private:
  void OnLookupFinished(int /*result*/) {
    pepper_message_filter_->ConnectTcpLookupFinished(
        routing_id_, request_id_, addresses_);
    delete this;
  }

  // HostResolver will call us using this callback when resolution is complete.
  net::CompletionCallbackImpl<LookupRequest> net_callback_;

  PepperMessageFilter* pepper_message_filter_;
  net::SingleRequestHostResolver resolver_;

  int routing_id_;
  int request_id_;
  net::HostResolver::RequestInfo request_info_;

  net::AddressList addresses_;

  DISALLOW_COPY_AND_ASSIGN(LookupRequest);
};

void PepperMessageFilter::OnConnectTcp(int routing_id,
                                       int request_id,
                                       const std::string& host,
                                       uint16 port) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::URLRequestContext* req_context =
      request_context_->GetURLRequestContext();
  net::HostResolver::RequestInfo request_info(net::HostPortPair(host, port));

  // The lookup request will delete itself on completion.
  LookupRequest* lookup_request =
      new LookupRequest(this, req_context->host_resolver(),
                        routing_id, request_id, request_info);
  lookup_request->Start();
}

void PepperMessageFilter::OnConnectTcpAddress(int routing_id,
                                              int request_id,
                                              const PP_Flash_NetAddress& addr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Validate the address and then continue (doing |connect()|) on a worker
  // thread.
  if (!ValidateNetAddress(addr) ||
      !base::WorkerPool::PostTask(FROM_HERE,
           NewRunnableMethod(
               this,
               &PepperMessageFilter::ConnectTcpAddressOnWorkerThread,
               routing_id, request_id, addr),
           true)) {
    SendConnectTcpACKError(routing_id, request_id);
  }
}

bool PepperMessageFilter::SendConnectTcpACKError(int routing_id,
                                                 int request_id) {
  return Send(
      new PepperMsg_ConnectTcpACK(routing_id, request_id,
                                  IPC::InvalidPlatformFileForTransit(),
                                  kInvalidNetAddress, kInvalidNetAddress));
}

void PepperMessageFilter::ConnectTcpLookupFinished(
    int routing_id,
    int request_id,
    const net::AddressList& addresses) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // If the lookup returned addresses, continue (doing |connect()|) on a worker
  // thread.
  if (!addresses.head() ||
      !base::WorkerPool::PostTask(FROM_HERE,
           NewRunnableMethod(
               this,
               &PepperMessageFilter::ConnectTcpOnWorkerThread,
               routing_id, request_id, addresses),
           true)) {
    SendConnectTcpACKError(routing_id, request_id);
  }
}

void PepperMessageFilter::ConnectTcpOnWorkerThread(int routing_id,
                                                   int request_id,
                                                   net::AddressList addresses) {
  IPC::PlatformFileForTransit socket_for_transit =
      IPC::InvalidPlatformFileForTransit();
  PP_Flash_NetAddress local_addr = kInvalidNetAddress;
  PP_Flash_NetAddress remote_addr = kInvalidNetAddress;

  for (const struct addrinfo* ai = addresses.head(); ai; ai = ai->ai_next) {
    PP_Flash_NetAddress addr = SockaddrToNetAddress(ai->ai_addr,
                                                    ai->ai_addrlen);
    int fd = ConnectTcpSocket(addr, &local_addr, &remote_addr);
    if (fd != -1) {
      socket_for_transit = base::FileDescriptor(fd, true);
      break;
    }
  }

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &PepperMessageFilter::Send,
          new PepperMsg_ConnectTcpACK(
              routing_id, request_id,
              socket_for_transit, local_addr, remote_addr)));
}

// TODO(vluu): Eliminate duplication between this and
// |ConnectTcpOnWorkerThread()|.
void PepperMessageFilter::ConnectTcpAddressOnWorkerThread(
    int routing_id,
    int request_id,
    PP_Flash_NetAddress addr) {
  IPC::PlatformFileForTransit socket_for_transit =
      IPC::InvalidPlatformFileForTransit();
  PP_Flash_NetAddress local_addr = kInvalidNetAddress;
  PP_Flash_NetAddress remote_addr = kInvalidNetAddress;

  int fd = ConnectTcpSocket(addr, &local_addr, &remote_addr);
  if (fd != -1)
    socket_for_transit = base::FileDescriptor(fd, true);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &PepperMessageFilter::Send,
          new PepperMsg_ConnectTcpACK(
              routing_id, request_id,
              socket_for_transit, local_addr, remote_addr)));
}

#endif  // ENABLE_FLAPPER_HACKS
