// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_TRAFFIC_DETECTOR_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_TRAFFIC_DETECTOR_H_

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/address_family.h"
#include "net/base/network_change_notifier.h"

namespace net {
class DatagramServerSocket;
class IOBufferWithSize;
class IPEndPoint;
}

namespace local_discovery {

// Detects mDns traffic that looks like "Privet" protocol.
// Can produce false positives results, but main task of the class is to avoid
// running full mDns listener if user doesn't have devices.
// When traffic is detected, class fires callback and shutdowns itself.
class PrivetTrafficDetector
    : public base::RefCountedThreadSafe<
          PrivetTrafficDetector, content::BrowserThread::DeleteOnIOThread>,
      private net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  PrivetTrafficDetector(net::AddressFamily address_family,
                        const base::Closure& on_traffic_detected);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;
  friend class base::DeleteHelper<PrivetTrafficDetector>;
  virtual ~PrivetTrafficDetector();

    // net::NetworkChangeNotifier::NetworkChangeObserver implementation.
  virtual void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  void Start();
  void ScheduleRestart();
  void Restart();
  int Bind();
  int DoLoop(int rv);

  base::Closure on_traffic_detected_;
  scoped_refptr<base::TaskRunner> callback_runner_;
  net::AddressFamily address_family_;
  scoped_ptr<net::IPEndPoint> recv_addr_;
  scoped_ptr<net::DatagramServerSocket> socket_;
  scoped_refptr<net::IOBufferWithSize> io_buffer_;
  base::CancelableClosure restart_callback_;

  DISALLOW_COPY_AND_ASSIGN(PrivetTrafficDetector);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_TRAFFIC_DETECTOR_H_
