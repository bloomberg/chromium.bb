// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

#include "api/impl/mdns_screen_listener_factory.h"
#include "api/impl/mdns_screen_publisher_factory.h"
#include "api/public/network_service_manager.h"
#include "api/public/screen_listener.h"
#include "api/public/screen_publisher.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace {

bool g_done = false;
bool g_dump_services = false;

void sigusr1_dump_services(int) {
  g_dump_services = true;
}

void sigint_stop(int) {
  LOG_INFO << "caught SIGINT, exiting...";
  g_done = true;
}

void SignalThings() {
  struct sigaction usr1_sa;
  struct sigaction int_sa;
  struct sigaction unused;

  usr1_sa.sa_handler = &sigusr1_dump_services;
  sigemptyset(&usr1_sa.sa_mask);
  usr1_sa.sa_flags = 0;

  int_sa.sa_handler = &sigint_stop;
  sigemptyset(&int_sa.sa_mask);
  int_sa.sa_flags = 0;

  sigaction(SIGUSR1, &usr1_sa, &unused);
  sigaction(SIGINT, &int_sa, &unused);

  LOG_INFO << "signal handlers setup";
  LOG_INFO << "pid: " << getpid();
}

class ListenerObserver final : public ScreenListener::Observer {
 public:
  ~ListenerObserver() override = default;
  void OnStarted() override { LOG_INFO << "listener started!"; }
  void OnStopped() override { LOG_INFO << "listener stopped!"; }
  void OnSuspended() override { LOG_INFO << "listener suspended!"; }
  void OnSearching() override { LOG_INFO << "listener searching!"; }

  void OnScreenAdded(const ScreenInfo& info) override {
    LOG_INFO << "found! " << info.friendly_name;
  }
  void OnScreenChanged(const ScreenInfo& info) override {
    LOG_INFO << "changed! " << info.friendly_name;
  }
  void OnScreenRemoved(const ScreenInfo& info) override {
    LOG_INFO << "removed! " << info.friendly_name;
  }
  void OnAllScreensRemoved() override { LOG_INFO << "all removed!"; }
  void OnError(ScreenListenerError) override {}
  void OnMetrics(ScreenListener::Metrics) override {}
};

class PublisherObserver final : public ScreenPublisher::Observer {
 public:
  ~PublisherObserver() override = default;

  void OnStarted() override { LOG_INFO << "publisher started!"; }
  void OnStopped() override { LOG_INFO << "publisher stopped!"; }
  void OnSuspended() override { LOG_INFO << "publisher suspended!"; }

  void OnError(ScreenPublisherError) override {}
  void OnMetrics(ScreenPublisher::Metrics) override {}
};

void ListenerDemo() {
  SignalThings();

  ListenerObserver listener_observer;
  MdnsScreenListenerConfig listener_config;
  auto mdns_listener =
      MdnsScreenListenerFactory::Create(listener_config, &listener_observer);

  auto* network_service = NetworkServiceManager::Create(
      std::move(mdns_listener), nullptr, nullptr, nullptr);

  network_service->GetMdnsScreenListener()->Start();

  while (!g_done) {
    network_service->RunEventLoopOnce();
  }

  network_service->GetMdnsScreenListener()->Stop();

  NetworkServiceManager::Dispose();
}

void PublisherDemo(const std::string& friendly_name) {
  SignalThings();

  PublisherObserver publisher_observer;
  // TODO(btolsch): aggregate initialization probably better?
  ScreenPublisher::Config publisher_config;
  publisher_config.friendly_name = friendly_name;
  publisher_config.connection_server_port = 6667;
  auto mdns_publisher =
      MdnsScreenPublisherFactory::Create(publisher_config, &publisher_observer);
  auto* network_service = NetworkServiceManager::Create(
      nullptr, std::move(mdns_publisher), nullptr, nullptr);

  network_service->GetMdnsScreenPublisher()->Start();

  while (!g_done) {
    network_service->RunEventLoopOnce();
  }

  network_service->GetMdnsScreenPublisher()->Stop();

  NetworkServiceManager::Dispose();
}

}  // namespace
}  // namespace openscreen

int main(int argc, char** argv) {
  openscreen::platform::SetLogLevel(openscreen::platform::LogLevel::kVerbose,
                                    0);
  if (argc == 1) {
    openscreen::ListenerDemo();
  } else {
    openscreen::PublisherDemo(argv[1]);
  }

  return 0;
}
