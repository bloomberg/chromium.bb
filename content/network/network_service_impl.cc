// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/network_service_impl.h"

#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "content/network/network_context.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/logging_network_change_observer.h"
#include "net/base/network_change_notifier.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log.h"
#include "net/log/net_log_util.h"
#include "net/url_request/url_request_context_builder.h"

#if defined(OS_ANDROID)
#include "net/android/network_change_notifier_factory_android.h"
#endif

namespace content {

std::unique_ptr<NetworkService> NetworkService::Create(net::NetLog* net_log) {
  return base::MakeUnique<NetworkServiceImpl>(nullptr, net_log);
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
    std::unique_ptr<service_manager::BinderRegistry> registry,
    net::NetLog* net_log)
    : registry_(std::move(registry)), binding_(this) {
  // |registry_| is nullptr when an in-process NetworkService is
  // created directly. The latter is done in concert with using
  // CreateNetworkContextWithBuilder to ease the transition to using the
  // network service.
  if (registry_) {
    registry_->AddInterface<mojom::NetworkService>(
        base::Bind(&NetworkServiceImpl::Create, base::Unretained(this)));

    // Set up net::NetworkChangeNotifier to watch for network change events.
    std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier;
    /* disabled temporarily
#if defined(OS_ANDROID)
    network_change_notifier_factory_ =
        std::make_unique<net::NetworkChangeNotifierFactoryAndroid>();
    network_change_notifier =
        base::WrapUnique(network_change_notifier_factory_->CreateInstance());
#elif defined(OS_CHROMEOS) || defined(OS_IOS)
    // ChromeOS has its own implementation of NetworkChangeNotifier that lives
    // outside of //net and iOS doesn't embed //content.
    // TODO(xunjieli): Figure out what to do for these two platforms.
    NOTIMPLEMENTED();
#else
    network_change_notifier =
        base::WrapUnique(net::NetworkChangeNotifier::Create());
#endif
    */
    network_change_manager_ = std::make_unique<NetworkChangeManagerImpl>(
        std::move(network_change_notifier));
  } else {
    network_change_manager_ =
        std::make_unique<NetworkChangeManagerImpl>(nullptr);
  }

  if (net_log) {
    net_log_ = net_log;
  } else {
    owned_net_log_ = std::make_unique<MojoNetLog>();
    // Note: The command line switches are only checked when not using the
    // embedder's NetLog, as it may already be writing to the destination log
    // file.
    owned_net_log_->ProcessCommandLine(*base::CommandLine::ForCurrentProcess());
    net_log_ = owned_net_log_.get();
  }

  // Add an observer that will emit network change events to the ChromeNetLog.
  // Assuming NetworkChangeNotifier dispatches in FIFO order, we should be
  // logging the network change before other IO thread consumers respond to it.
  network_change_observer_.reset(
      new net::LoggingNetworkChangeObserver(net_log_));
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
      base::MakeUnique<NetworkContext>(this, std::move(request),
                                       std::move(params), std::move(builder));
  *url_request_context = network_context->url_request_context();
  return network_context;
}

std::unique_ptr<NetworkServiceImpl> NetworkServiceImpl::CreateForTesting() {
  return base::WrapUnique(new NetworkServiceImpl(
      base::MakeUnique<service_manager::BinderRegistry>()));
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

void NetworkServiceImpl::DisableQuic() {
  quic_disabled_ = true;

  for (auto* network_context : network_contexts_) {
    network_context->DisableQuic();
  }
}

void NetworkServiceImpl::SetRawHeadersAccess(uint32_t process_id, bool allow) {
  DCHECK(process_id);
  if (allow)
    processes_with_raw_headers_access_.insert(process_id);
  else
    processes_with_raw_headers_access_.erase(process_id);
}

bool NetworkServiceImpl::HasRawHeadersAccess(uint32_t process_id) const {
  // Allow raw headers for browser-initiated requests.
  if (!process_id)
    return true;
  return processes_with_raw_headers_access_.find(process_id) !=
         processes_with_raw_headers_access_.end();
}

net::NetLog* NetworkServiceImpl::net_log() const {
  return net_log_;
}

void NetworkServiceImpl::GetNetworkChangeManager(
    mojom::NetworkChangeManagerRequest request) {
  network_change_manager_->AddRequest(std::move(request));
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
