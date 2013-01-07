// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LOAD_TIMING_INFO_H_
#define NET_BASE_LOAD_TIMING_INFO_H_

#include "base/basictypes.h"
#include "base/time.h"
#include "net/base/net_export.h"

namespace net {

// All events that do not apply to a request have null times.  For non-HTTP
// requests, all times other than the request_start times are null.
//
// The general order for events is:
// request_start
// proxy_start
// proxy_end
// *dns_start
// *dns_end
// *connect_start
// *ssl_start
// *ssl_end
// *connect_end
// send_start
// send_end
// receive_headers_end
//
// Those times without an asterisk are computed by the URLRequest, or by objects
// it directly creates and always owns.  Those with an asterisk are computed
// by the connection attempt itself.  Since the connection attempt may be
// started before a URLRequest, the starred times may occur before, during, or
// after the request_start and proxy events.
struct NET_EXPORT LoadTimingInfo {
  // Contains the LoadTimingInfo events related to establishing a connection.
  // These are all set by ConnectJobs.
  struct NET_EXPORT_PRIVATE ConnectTiming {
    ConnectTiming();
    ~ConnectTiming();

    // The time spent looking up the host's DNS address.  Null for requests that
    // used proxies to look up the DNS address.  Also null for SOCKS4 proxies,
    // since the DNS address is only looked up after the connection is
    // established, which results in unexpected event ordering.
    // TODO(mmenke):  The SOCKS4 event ordering could be refactored to allow
    //                these times to be non-null.
    base::TimeTicks dns_start;
    base::TimeTicks dns_end;

    // The time spent establishing the connection. Connect time includes proxy
    // connect times (Though not proxy_resolve times), DNS lookup times, time
    // spent waiting in certain queues, TCP, and SSL time.
    // TODO(mmenke):  For proxies, this includes time spent blocking on higher
    //                level socket pools.  Fix this.
    // TODO(mmenke):  Retried connections to the same server should apparently
    //                be included in this time.  Consider supporting that.
    //                Since the network stack has multiple notions of a "retry",
    //                handled at different levels, this may not be worth
    //                worrying about - backup jobs, reused socket failure,
    //                multiple round authentication.
    base::TimeTicks connect_start;
    base::TimeTicks connect_end;

    // The time when the SSL handshake started / completed. For non-HTTPS
    // requests these are null.  These times are only for the SSL connection to
    // the final destination server, not an SSL/SPDY proxy.
    base::TimeTicks ssl_start;
    base::TimeTicks ssl_end;
  };

  LoadTimingInfo();
  ~LoadTimingInfo();

  // True if the socket was reused.  When true, DNS, connect, and SSL times
  // will all be null.  When false, those times may be null, too, for non-HTTP
  // requests, or when they don't apply to a request.
  bool socket_reused;

  // Unique socket ID, can be used to identify requests served by the same
  // socket.
  // TODO(mmenke):  Do something reasonable for SPDY proxies.
  uint32 socket_log_id;

  // Start time as a base::Time, so times can be coverted into actual times.
  // Other times are recorded as TimeTicks so they are not affected by clock
  // changes.
  base::Time request_start_time;

  base::TimeTicks request_start;

  // The time spent determing which proxy to use.  Null when there is no PAC.
  base::TimeTicks proxy_resolve_start;
  base::TimeTicks proxy_resolve_end;

  ConnectTiming connect_timing;

  // The time that sending HTTP request started / ended.
  base::TimeTicks send_start;
  base::TimeTicks send_end;

  // The time at which the end of the HTTP headers were received.
  base::TimeTicks receive_headers_end;
};

}  // namespace net

#endif  // NET_BASE_LOAD_TIMING_INFO_H_
