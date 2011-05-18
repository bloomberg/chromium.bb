// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper_message_filter.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/process_util.h"
#include "base/threading/worker_pool.h"
#include "base/values.h"
#include "content/browser/browser_thread.h"
#include "content/browser/font_list_async.h"
#include "content/browser/renderer_host/browser_render_process_host.h"
#include "content/browser/resource_context.h"
#include "content/common/pepper_messages.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/url_request/url_request_context.h"
#include "ppapi/c/private/ppb_flash_net_connector.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "webkit/plugins/ppapi/ppb_flash_net_connector_impl.h"

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

PepperMessageFilter::PepperMessageFilter(
    const content::ResourceContext* resource_context)
    : resource_context_(resource_context),
      host_resolver_(NULL) {
  DCHECK(resource_context_);
}

PepperMessageFilter::PepperMessageFilter(net::HostResolver* host_resolver)
    : resource_context_(NULL),
      host_resolver_(host_resolver) {
  DCHECK(host_resolver);
}

PepperMessageFilter::~PepperMessageFilter() {}

bool PepperMessageFilter::OnMessageReceived(const IPC::Message& msg,
                                            bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(PepperMessageFilter, msg, *message_was_ok)
#if defined(ENABLE_FLAPPER_HACKS)
    IPC_MESSAGE_HANDLER(PepperMsg_ConnectTcp, OnConnectTcp)
    IPC_MESSAGE_HANDLER(PepperMsg_ConnectTcpAddress, OnConnectTcpAddress)
#endif  // ENABLE_FLAPPER_HACKS
    IPC_MESSAGE_HANDLER(PepperMsg_GetLocalTimeZoneOffset,
                        OnGetLocalTimeZoneOffset)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PpapiHostMsg_PPBFont_GetFontFamilies,
                                    OnGetFontFamilies)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
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

  net::HostResolver::RequestInfo request_info(net::HostPortPair(host, port));

  // The lookup request will delete itself on completion.
  LookupRequest* lookup_request =
      new LookupRequest(this, GetHostResolver(),
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

void PepperMessageFilter::OnGetLocalTimeZoneOffset(base::Time t,
                                                   double* result) {
  // Explode it to local time and then unexplode it as if it were UTC. Also
  // explode it to UTC and unexplode it (this avoids mismatching rounding or
  // lack thereof). The time zone offset is their difference.
  //
  // The reason for this processing being in the browser process is that on
  // Linux, the localtime calls require filesystem access prohibited by the
  // sandbox.
  base::Time::Exploded exploded;
  t.LocalExplode(&exploded);
  base::Time adj_time = base::Time::FromUTCExploded(exploded);
  t.UTCExplode(&exploded);
  base::Time cur = base::Time::FromUTCExploded(exploded);
  *result = (adj_time - cur).InSecondsF();
}

void PepperMessageFilter::OnGetFontFamilies(IPC::Message* reply_msg) {
  content::GetFontListAsync(
      base::Bind(&PepperMessageFilter::GetFontFamiliesComplete,
                 this, reply_msg));
}

void PepperMessageFilter::GetFontFamiliesComplete(
    IPC::Message* reply_msg,
    scoped_refptr<content::FontListResult> result) {
  ListValue* input = result->list.get();

  std::string output;
  for (size_t i = 0; i < input->GetSize(); i++) {
    ListValue* cur_font;
    if (!input->GetList(i, &cur_font))
      continue;

    // Each entry in the list is actually a list of (font name, localized name).
    // We only care about the regular name.
    std::string font_name;
    if (!cur_font->GetString(0, &font_name))
      continue;

    // Font names are separated with nulls. We also want an explicit null at
    // the end of the string (Pepper strings aren't null terminated so since
    // we specify there will be a null, it should actually be in the string).
    output.append(font_name);
    output.push_back(0);
  }

  PpapiHostMsg_PPBFont_GetFontFamilies::WriteReplyParams(reply_msg, output);
  Send(reply_msg);
}

net::HostResolver* PepperMessageFilter::GetHostResolver() {
  if (resource_context_)
    return resource_context_->host_resolver();
  return host_resolver_;
}
