// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/network_service_impl.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/network/network_context.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log_util.h"
#include "net/url_request/url_request_context_builder.h"

namespace content {

std::unique_ptr<NetworkService> NetworkService::Create() {
  return base::MakeUnique<NetworkServiceImpl>(nullptr);
}

class NetworkServiceImpl::MojoNetLog : public net::NetLog {
 public:
  MojoNetLog() {}

  // If specified by the command line, stream network events (NetLog) to a
  // file on disk. This will last for the duration of the process.
  void ProcessCommandLine(const base::CommandLine& command_line) {
    if (!command_line.HasSwitch(switches::kLogNetLog))
      return;

    base::FilePath log_path =
        command_line.GetSwitchValuePath(switches::kLogNetLog);

    // TODO(eroman): Should get capture mode from the command line.
    net::NetLogCaptureMode capture_mode =
        net::NetLogCaptureMode::IncludeCookiesAndCredentials();

    file_net_log_observer_ =
        net::FileNetLogObserver::CreateUnbounded(log_path, nullptr);
    file_net_log_observer_->StartObserving(this, capture_mode);
  }

  ~MojoNetLog() override {
    if (file_net_log_observer_)
      file_net_log_observer_->StopObserving(nullptr, base::OnceClosure());
  }

 private:
  std::unique_ptr<net::FileNetLogObserver> file_net_log_observer_;
  DISALLOW_COPY_AND_ASSIGN(MojoNetLog);
};

NetworkServiceImpl::NetworkServiceImpl(
    std::unique_ptr<service_manager::BinderRegistry> registry)
    : net_log_(new MojoNetLog), registry_(std::move(registry)), binding_(this) {
  // |registry_| is nullptr in tests and when an in-process NetworkService is
  // created directly. The latter is done in concert with using
  // CreateNetworkContextWithBuilder to ease the transition to using the network
  // service.
  if (registry_) {
    registry_->AddInterface<mojom::NetworkService>(
        base::Bind(&NetworkServiceImpl::Create, base::Unretained(this)));

    // Note: The command line switches are only checked when running out of
    // process, since in in-process mode other code may already be writing to
    // the destination log file.
    net_log_->ProcessCommandLine(*base::CommandLine::ForCurrentProcess());
  }
}

NetworkServiceImpl::~NetworkServiceImpl() {
  // Call each Network and ask it to release its net::URLRequestContext, as they
  // may have references to shared objects owned by the NetworkService. The
  // NetworkContexts deregister themselves in Cleanup(), so have to be careful.
  while (!network_contexts_.empty())
    (*network_contexts_.begin())->Cleanup();
}

std::unique_ptr<mojom::NetworkContext>
NetworkServiceImpl::CreateNetworkContextWithBuilder(
    content::mojom::NetworkContextRequest request,
    content::mojom::NetworkContextParamsPtr params,
    std::unique_ptr<net::URLRequestContextBuilder> builder,
    net::URLRequestContext** url_request_context) {
  std::unique_ptr<NetworkContext> network_context =
      base::MakeUnique<NetworkContext>(std::move(request), std::move(params),
                                       std::move(builder));
  *url_request_context = network_context->url_request_context();
  return network_context;
}

std::unique_ptr<NetworkServiceImpl> NetworkServiceImpl::CreateForTesting() {
  return base::WrapUnique(new NetworkServiceImpl(nullptr));
}

void NetworkServiceImpl::RegisterNetworkContext(
    NetworkContext* network_context) {
  DCHECK_EQ(0u, network_contexts_.count(network_context));
  network_contexts_.insert(network_context);
}

void NetworkServiceImpl::DeregisterNetworkContext(
    NetworkContext* network_context) {
  DCHECK_EQ(1u, network_contexts_.count(network_context));
  network_contexts_.erase(network_context);
}

void NetworkServiceImpl::CreateNetworkContext(
    mojom::NetworkContextRequest request,
    mojom::NetworkContextParamsPtr params) {
  // The NetworkContext will destroy itself on connection error, or when the
  // service is destroyed.
  new NetworkContext(this, std::move(request), std::move(params));
}

void NetworkServiceImpl::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_->BindInterface(interface_name, std::move(interface_pipe));
}

void NetworkServiceImpl::Create(mojom::NetworkServiceRequest request) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
}

}  // namespace content
