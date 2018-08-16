// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_TRAFFIC_DETECTOR_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_TRAFFIC_DETECTOR_H_

#include <memory>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/address_family.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/mojom/udp_socket.mojom.h"

namespace content {
class BrowserContext;
}

namespace cloud_print {

// Detects mDns traffic that looks like "Privet" protocol.
// Can produce false positives results, but main task of the class is to avoid
// running full mDns listener if user doesn't have devices.
// When traffic is detected, class fires callback and shutdowns itself.
class PrivetTrafficDetector
    : public base::RefCountedThreadSafe<
          PrivetTrafficDetector,
          content::BrowserThread::DeleteOnIOThread>,
      private network::NetworkConnectionTracker::NetworkConnectionObserver,
      public network::mojom::UDPSocketReceiver {
 public:
  PrivetTrafficDetector(net::AddressFamily address_family,
                        content::BrowserContext* profile,
                        const base::Closure& on_traffic_detected);

  // Called on the UI thread.
  void Start();
  void Stop();

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;
  friend class base::DeleteHelper<PrivetTrafficDetector>;

  ~PrivetTrafficDetector() override;

  // Unless otherwise noted, all methods are called on the IO thread.
  void HandleConnectionChanged(network::mojom::ConnectionType type);
  void ScheduleRestart();
  void Restart(const net::NetworkInterfaceList& networks);
  void Bind();

  // Called on the UI thread.
  void CreateUDPSocketOnUIThread(
      network::mojom::UDPSocketRequest request,
      network::mojom::UDPSocketReceiverPtr receiver_ptr);

  void OnBindComplete(net::IPEndPoint multicast_addr,
                      int rv,
                      const base::Optional<net::IPEndPoint>& ip_address);
  bool IsSourceAcceptable() const;
  bool IsPrivetPacket(base::span<const uint8_t> data) const;
  void OnJoinGroupComplete(int rv);
  void ResetConnection();

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  // Called on the UI thread.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // network::mojom::UDPSocketReceiver implementation
  void OnReceived(int32_t result,
                  const base::Optional<net::IPEndPoint>& src_addr,
                  base::Optional<base::span<const uint8_t>> data) override;

  // Initialized on the UI thread, but only accessed on the IO thread.
  base::Closure on_traffic_detected_;
  scoped_refptr<base::TaskRunner> callback_runner_;
  net::NetworkInterfaceList networks_;
  net::AddressFamily address_family_;
  int restart_attempts_;

  // Only accessed on the IO thread.
  net::IPEndPoint recv_addr_;
  base::Time start_time_;
  network::mojom::UDPSocketPtr socket_;

  // Implementation of socket receiver callback.
  // Initialized on the UI thread, but only accessed on the IO thread.
  mojo::Binding<network::mojom::UDPSocketReceiver> receiver_binding_;

  // Only accessed on the UI thread
  content::BrowserContext* const profile_;

  base::WeakPtrFactory<PrivetTrafficDetector> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrivetTrafficDetector);
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_TRAFFIC_DETECTOR_H_
