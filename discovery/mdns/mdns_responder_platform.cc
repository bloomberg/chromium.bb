// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_responder_platform.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

#include "base/ip_address.h"
#include "platform/api/logging.h"
#include "platform/api/network_interface.h"
#include "platform/api/socket.h"
#include "platform/api/time.h"
#include "third_party/mDNSResponder/src/mDNSCore/mDNSEmbeddedAPI.h"

extern "C" {

const char ProgramName[] = "openscreen";

mDNSs32 mDNSPlatformOneSecond = 1000;

mStatus mDNSPlatformInit(mDNS* m) {
  mDNSCoreInitComplete(m, mStatus_NoError);
  return mStatus_NoError;
}

void mDNSPlatformClose(mDNS* m) {}

mStatus mDNSPlatformSendUDP(const mDNS* m,
                            const void* msg,
                            const mDNSu8* last,
                            mDNSInterfaceID InterfaceID,
                            UDPSocket* src,
                            const mDNSAddr* dst,
                            mDNSIPPort dstport) {
  const auto* socket =
      reinterpret_cast<openscreen::platform::UdpSocketPtr>(InterfaceID);
  const auto socket_it =
      std::find(m->p->sockets.begin(), m->p->sockets.end(), socket);
  if (socket_it == m->p->sockets.end())
    return mStatus_BadInterfaceErr;

  openscreen::IPEndpoint dest{
      openscreen::IPAddress{dst->type == mDNSAddrType_IPv4
                                ? openscreen::IPAddress::Version::kV4
                                : openscreen::IPAddress::Version::kV6,
                            dst->ip.v4.b},
      static_cast<uint16_t>((dstport.b[0] << 8) | dstport.b[1])};
  int64_t length = last - static_cast<const uint8_t*>(msg);
  openscreen::platform::SendUdp(*socket_it, msg, length, dest);
  return mStatus_NoError;
}

void mDNSPlatformLock(const mDNS* m) {
  // We're single threaded.
}

void mDNSPlatformUnlock(const mDNS* m) {}

void mDNSPlatformStrCopy(void* dst, const void* src) {
  std::strcpy(static_cast<char*>(dst), static_cast<const char*>(src));
}

mDNSu32 mDNSPlatformStrLen(const void* src) {
  return std::strlen(static_cast<const char*>(src));
}

void mDNSPlatformMemCopy(void* dst, const void* src, mDNSu32 len) {
  std::memcpy(dst, src, len);
}

mDNSBool mDNSPlatformMemSame(const void* dst, const void* src, mDNSu32 len) {
  return std::memcmp(dst, src, len) == 0 ? mDNStrue : mDNSfalse;
}

void mDNSPlatformMemZero(void* dst, mDNSu32 len) {
  std::memset(dst, 0, len);
}

void* mDNSPlatformMemAllocate(mDNSu32 len) {
  return malloc(len);
}

void mDNSPlatformMemFree(void* mem) {
  free(mem);
}

mDNSu32 mDNSPlatformRandomSeed() {
  return std::chrono::steady_clock::now().time_since_epoch().count();
}

mStatus mDNSPlatformTimeInit() {
  return mStatus_NoError;
}

mDNSs32 mDNSPlatformRawTime() {
  return static_cast<int32_t>(
      openscreen::platform::GetMonotonicTimeNow().AsMilliseconds());
}

mDNSs32 mDNSPlatformUTC() {
  return static_cast<int32_t>(openscreen::platform::GetUTCNow().AsSeconds());
}

void mDNSPlatformWriteDebugMsg(const char* msg) {
  DVLOG(3) << __func__ << ": " << msg;
}

void mDNSPlatformWriteLogMsg(const char* ident,
                             const char* msg,
                             mDNSLogLevel_t loglevel) {
  VLOG(2) << __func__ << ": " << msg;
}

TCPSocket* mDNSPlatformTCPSocket(mDNS* const m,
                                 TCPSocketFlags flags,
                                 mDNSIPPort* port) {
  UNIMPLEMENTED();
  return nullptr;
}

TCPSocket* mDNSPlatformTCPAccept(TCPSocketFlags flags, int sd) {
  UNIMPLEMENTED();
  return nullptr;
}

int mDNSPlatformTCPGetFD(TCPSocket* sock) {
  UNIMPLEMENTED();
  return 0;
}

mStatus mDNSPlatformTCPConnect(TCPSocket* sock,
                               const mDNSAddr* dst,
                               mDNSOpaque16 dstport,
                               domainname* hostname,
                               mDNSInterfaceID InterfaceID,
                               TCPConnectionCallback callback,
                               void* context) {
  UNIMPLEMENTED();
  return mStatus_NoError;
}

void mDNSPlatformTCPCloseConnection(TCPSocket* sock) {
  UNIMPLEMENTED();
}

long mDNSPlatformReadTCP(TCPSocket* sock,
                         void* buf,
                         unsigned long buflen,
                         mDNSBool* closed) {
  UNIMPLEMENTED();
  return 0;
}

long mDNSPlatformWriteTCP(TCPSocket* sock, const char* msg, unsigned long len) {
  UNIMPLEMENTED();
  return 0;
}

UDPSocket* mDNSPlatformUDPSocket(mDNS* const m,
                                 const mDNSIPPort requestedport) {
  UNIMPLEMENTED();
  return nullptr;
}

void mDNSPlatformUDPClose(UDPSocket* sock) {
  UNIMPLEMENTED();
}

void mDNSPlatformReceiveBPF_fd(mDNS* const m, int fd) {
  UNIMPLEMENTED();
}

void mDNSPlatformUpdateProxyList(mDNS* const m,
                                 const mDNSInterfaceID InterfaceID) {
  UNIMPLEMENTED();
}

void mDNSPlatformSendRawPacket(const void* const msg,
                               const mDNSu8* const end,
                               mDNSInterfaceID InterfaceID) {
  UNIMPLEMENTED();
}

void mDNSPlatformSetLocalAddressCacheEntry(mDNS* const m,
                                           const mDNSAddr* const tpa,
                                           const mDNSEthAddr* const tha,
                                           mDNSInterfaceID InterfaceID) {}

void mDNSPlatformSourceAddrForDest(mDNSAddr* const src,
                                   const mDNSAddr* const dst) {}

mStatus mDNSPlatformTLSSetupCerts(void) {
  UNIMPLEMENTED();
  return mStatus_NoError;
}

void mDNSPlatformTLSTearDownCerts(void) {
  UNIMPLEMENTED();
}

void mDNSPlatformSetDNSConfig(mDNS* const m,
                              mDNSBool setservers,
                              mDNSBool setsearch,
                              domainname* const fqdn,
                              DNameListElem** RegDomains,
                              DNameListElem** BrowseDomains) {}

mStatus mDNSPlatformGetPrimaryInterface(mDNS* const m,
                                        mDNSAddr* v4,
                                        mDNSAddr* v6,
                                        mDNSAddr* router) {
  return mStatus_NoError;
}

void mDNSPlatformDynDNSHostNameStatusChanged(const domainname* const dname,
                                             const mStatus status) {}

void mDNSPlatformSetAllowSleep(mDNS* const m,
                               mDNSBool allowSleep,
                               const char* reason) {}

void mDNSPlatformSendWakeupPacket(mDNS* const m,
                                  mDNSInterfaceID InterfaceID,
                                  char* EthAddr,
                                  char* IPAddr,
                                  int iteration) {
  UNIMPLEMENTED();
}

mDNSBool mDNSPlatformValidRecordForInterface(AuthRecord* rr,
                                             const NetworkInterfaceInfo* intf) {
  UNIMPLEMENTED();
  return mDNStrue;
}

}  // extern "C"
