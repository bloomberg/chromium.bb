// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/base/async_dns_lookup.h"

#include "build/build_config.h"

#if defined(OS_POSIX)
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif  // defined(OS_POSIX)

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "chrome/browser/sync/notifier/base/nethelpers.h"
#include "chrome/browser/sync/notifier/gaia_auth/inet_aton.h"
#include "talk/base/byteorder.h"
#include "talk/base/common.h"
#include "talk/base/socketaddress.h"
#include "talk/base/thread.h"

enum { MSG_TIMEOUT = talk_base::SignalThread::ST_MSG_FIRST_AVAILABLE };

#ifndef WIN32
const int WSAHOST_NOT_FOUND = 11001;  // Follows the format in winsock2.h.
#endif  // WIN32

namespace notifier {

AsyncDNSLookup::AsyncDNSLookup(const talk_base::SocketAddress& server)
  : server_(new talk_base::SocketAddress(server)),
    error_(0) {
  // Timeout after 5 seconds.
  talk_base::Thread::Current()->PostDelayed(5000, this, MSG_TIMEOUT);
}

AsyncDNSLookup::~AsyncDNSLookup() {
}

void AsyncDNSLookup::DoWork() {
  std::string hostname(server_->IPAsString());

  in_addr addr;
  if (inet_aton(hostname.c_str(), &addr)) {
    talk_base::CritScope scope(&cs_);
    ip_list_.push_back(talk_base::NetworkToHost32(addr.s_addr));
  } else {
    LOG(INFO) << "(" << hostname << ")";
    hostent ent;
    char buffer[8192];
    int errcode = 0;
    hostent* host = SafeGetHostByName(hostname.c_str(), &ent,
                                      buffer, sizeof(buffer),
                                      &errcode);
    talk_base::Thread::Current()->Clear(this, MSG_TIMEOUT);
    if (host) {
      talk_base::CritScope scope(&cs_);

      // Check to see if this already timed out.
      if (error_ == 0) {
        for (int index = 0; true; ++index) {
          uint32* addr = reinterpret_cast<uint32*>(host->h_addr_list[index]);
          if (addr == 0) {  // 0 = end of list.
            break;
          }
          uint32 ip = talk_base::NetworkToHost32(*addr);
          LOG(INFO) << "(" << hostname << ") resolved to: "
                    << talk_base::SocketAddress::IPToString(ip);
          ip_list_.push_back(ip);
        }
        // Maintain the invariant that either the list is not empty or the
        // error is non zero when we are done with processing the dnslookup.
        if (ip_list_.empty() && error_ == 0) {
          error_ = WSAHOST_NOT_FOUND;
        }
      }
      FreeHostEnt(host);
    } else {
      {  // Scoping for the critical section.
        talk_base::CritScope scope(&cs_);

        // Check to see if this already timed out.
        if (error_ == 0) {
          error_ = errcode;
        }
      }
      LOG(ERROR) << "(" << hostname << ") error: " << error_;
    }
  }
}

void AsyncDNSLookup::OnMessage(talk_base::Message* message) {
  ASSERT(message);
  if (message->message_id == MSG_TIMEOUT) {
    OnTimeout();
  } else {
    talk_base::SignalThread::OnMessage(message);
  }
}

void AsyncDNSLookup::OnTimeout() {
  // Allow the scope for the critical section to be the whole method, just to
  // be sure that the worker thread can't exit while we are doing
  // SignalWorkDone (because that could possibly cause the class to be
  // deleted).
  talk_base::CritScope scope(&cs_);

  // Check to see if the ip list was already filled (or errored out).
  if (!ip_list_.empty() || error_ != 0) {
    return;
  }

  // Worker thread is taking too long so timeout.
  error_ = WSAHOST_NOT_FOUND;

  // Rely on the caller to do the Release/Destroy.
  //
  // Doing this signal while holding cs_ won't cause a deadlock because the
  // AsyncDNSLookup::DoWork thread doesn't have any locks at this point, and it
  // is the only thread being held up by this.
  SignalWorkDone(this);

  // Ensure that no more "WorkDone" signaling is done.
  // Don't call Release or Destroy since that was already done by the callback.
  SignalWorkDone.disconnect_all();
}

}  // namespace notifier
