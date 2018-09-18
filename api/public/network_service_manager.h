// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_NETWORK_SERVICE_MANAGER_H_
#define API_PUBLIC_NETWORK_SERVICE_MANAGER_H_

#include <memory>

#include "api/public/screen_connection_client.h"
#include "api/public/screen_connection_server.h"
#include "api/public/screen_listener.h"
#include "api/public/screen_publisher.h"

namespace openscreen {

// Manages services run as part of the Open Screen Protocol Library.  Library
// embedders should pass instances of required services to Create(), which will
// instantiate the singleton instance of the NetworkServiceManager and take
// ownership of the services.
//
// NOTES: If we add more injectable services, consider implementing a struct to
// hold the config vs. passsing long argument lists.
class NetworkServiceManager final {
 public:
  // Creates the singleton instance of the NetworkServiceManager.  nullptr may
  // be passed for services not provided by the embedder.
  static NetworkServiceManager* Create(
      std::unique_ptr<ScreenListener> mdns_listener,
      std::unique_ptr<ScreenPublisher> mdns_publisher,
      std::unique_ptr<ScreenConnectionClient> connection_client,
      std::unique_ptr<ScreenConnectionServer> connection_server);

  // Returns the singleton instance of the NetworkServiceManager previously
  // created by Create().
  static NetworkServiceManager* Get();

  // Destroys the NetworkServiceManager singleton and its owned services.  The
  // services must be shut down and no I/O or asynchronous work should be done
  // by the service instance destructors.
  static void Dispose();

  void RunEventLoopOnce();

  // Returns an instance of the mDNS screen listener, or nullptr if
  // not provided.
  ScreenListener* GetMdnsScreenListener();

  // Returns an instance of the mDNS screen publisher, or nullptr
  // if not provided.
  ScreenPublisher* GetMdnsScreenPublisher();

  // Returns an instance of the screen connection client, or nullptr
  // if not provided.
  ScreenConnectionClient* GetScreenConnectionClient();

  // Returns an instance of the screen connection server, or nullptr if
  // not provided.
  ScreenConnectionServer* GetScreenConnectionServer();

 private:
  NetworkServiceManager(
      std::unique_ptr<ScreenListener> mdns_listener,
      std::unique_ptr<ScreenPublisher> mdns_publisher,
      std::unique_ptr<ScreenConnectionClient> connection_client,
      std::unique_ptr<ScreenConnectionServer> connection_server);

  ~NetworkServiceManager();

  std::unique_ptr<ScreenListener> mdns_listener_;
  std::unique_ptr<ScreenPublisher> mdns_publisher_;
  std::unique_ptr<ScreenConnectionClient> connection_client_;
  std::unique_ptr<ScreenConnectionServer> connection_server_;
};

}  // namespace openscreen

#endif  // API_PUBLIC_NETWORK_SERVICE_MANAGER_H_
