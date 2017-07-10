// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/network_service.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "content/network/network_context.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log_util.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace content {

class NetworkService::MojoNetLog : public net::NetLog {
 public:
  MojoNetLog() {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();

    // If specified by the command line, stream network events (NetLog) to a
    // file on disk. This will last for the duration of the process.
    if (command_line->HasSwitch(switches::kLogNetLog)) {
      base::FilePath log_path =
          command_line->GetSwitchValuePath(switches::kLogNetLog);
      net::NetLogCaptureMode capture_mode =
          net::NetLogCaptureMode::IncludeCookiesAndCredentials();

      file_net_log_observer_ =
          net::FileNetLogObserver::CreateUnbounded(log_path, nullptr);
      file_net_log_observer_->StartObserving(this, capture_mode);
    }
  }
  ~MojoNetLog() override {
    if (file_net_log_observer_)
      file_net_log_observer_->StopObserving(nullptr, base::OnceClosure());
  }

 private:
  std::unique_ptr<net::FileNetLogObserver> file_net_log_observer_;
  DISALLOW_COPY_AND_ASSIGN(MojoNetLog);
};

NetworkService::NetworkService(
    std::unique_ptr<service_manager::BinderRegistry> registry)
    : net_log_(new MojoNetLog), registry_(std::move(registry)), binding_(this) {
  // |registry_| may be nullptr in tests.
  if (registry_) {
    registry_->AddInterface<mojom::NetworkService>(
        base::Bind(&NetworkService::Create, base::Unretained(this)));
  }
}

NetworkService::~NetworkService() {
  // Call each Network and ask it to release its net::URLRequestContext, as they
  // may have references to shared objects owned by the NetworkService. The
  // NetworkContexts deregister themselves in Cleanup(), so have to be careful.
  while (!network_contexts_.empty())
    (*network_contexts_.begin())->Cleanup();
}

std::unique_ptr<NetworkService> NetworkService::CreateForTesting() {
  return base::WrapUnique(new NetworkService());
}

void NetworkService::RegisterNetworkContext(NetworkContext* network_context) {
  DCHECK_EQ(0u, network_contexts_.count(network_context));
  network_contexts_.insert(network_context);
}

void NetworkService::DeregisterNetworkContext(NetworkContext* network_context) {
  DCHECK_EQ(1u, network_contexts_.count(network_context));
  network_contexts_.erase(network_context);
}

void NetworkService::CreateNetworkContext(
    mojom::NetworkContextRequest request,
    mojom::NetworkContextParamsPtr params) {
  // The NetworkContext will destroy itself on connection error, or when the
  // service is destroyed.
  new NetworkContext(this, std::move(request), std::move(params));
}

NetworkService::NetworkService() : NetworkService(nullptr) {}

void NetworkService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_->BindInterface(source_info, interface_name,
                           std::move(interface_pipe));
}

void NetworkService::Create(const service_manager::BindSourceInfo& source_info,
                            mojom::NetworkServiceRequest request) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
}

}  // namespace content
