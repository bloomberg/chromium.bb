// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/external_mojo/public/cpp/external_mojo_broker.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/optional.h"
#include "base/token.h"
#include "chromecast/external_mojo/public/cpp/common.h"
#include "chromecast/external_mojo/public/mojom/connector.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_filter.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace chromecast {
namespace external_mojo {

namespace {

void OnRegisterServiceResult(const std::string& service_name,
                             service_manager::mojom::ConnectResult result) {
  // RegisterServiceInstance() currently returns INVALID_ARGUMENT on success.
  if (result == service_manager::mojom::ConnectResult::ACCESS_DENIED) {
    LOG(WARNING) << "Failed to register external service proxy for "
                 << service_name;
  }
}

void OnInternalBindResult(
    const std::string& service_name,
    const std::string& interface_name,
    service_manager::mojom::ConnectResult result,
    const base::Optional<service_manager::Identity>& identity) {
  if (result != service_manager::mojom::ConnectResult::SUCCEEDED) {
    LOG(ERROR) << "Failed to bind " << service_name << ":" << interface_name
               << ", result = " << result;
  }
}

}  // namespace

class ExternalMojoBroker::ConnectorImpl : public mojom::ExternalConnector {
 public:
  ConnectorImpl() : connector_facade_(this) {}

  void InitializeChromium(
      std::unique_ptr<service_manager::Connector> connector,
      const std::vector<std::string>& external_services_to_proxy) {
    DCHECK(connector);
    connector_ = std::move(connector);
    RegisterExternalServices(external_services_to_proxy);
  }

  void AddBinding(mojom::ExternalConnectorRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

 private:
  class ExternalServiceProxy : public ::service_manager::Service {
   public:
    ExternalServiceProxy(ConnectorImpl* connector,
                         std::string service_name,
                         ::service_manager::mojom::ServiceRequest request)
        : connector_(connector),
          service_name_(std::move(service_name)),
          service_binding_(this, std::move(request)) {
      DCHECK(connector_);
    }

   private:
    void OnBindInterface(
        const service_manager::BindSourceInfo& source,
        const std::string& interface_name,
        mojo::ScopedMessagePipeHandle interface_pipe) override {
      connector_->BindExternalInterface(service_name_, interface_name,
                                        std::move(interface_pipe));
    }

    ConnectorImpl* const connector_;
    const std::string service_name_;
    service_manager::ServiceBinding service_binding_;

    DISALLOW_COPY_AND_ASSIGN(ExternalServiceProxy);
  };

  class ServiceManagerConnectorFacade
      : public service_manager::mojom::Connector {
   public:
    explicit ServiceManagerConnectorFacade(
        ExternalMojoBroker::ConnectorImpl* connector)
        : connector_(connector) {
      DCHECK(connector_);
    }

    void AddBinding(service_manager::mojom::ConnectorRequest request) {
      bindings_.AddBinding(this, std::move(request));
    }

   private:
    void BindInterface(const ::service_manager::ServiceFilter& filter,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe,
                       service_manager::mojom::BindInterfacePriority priority,
                       BindInterfaceCallback callback) override {
      connector_->BindInterface(filter.service_name(), interface_name,
                                std::move(interface_pipe));
      std::move(callback).Run(service_manager::mojom::ConnectResult::SUCCEEDED,
                              base::nullopt);
    }

    void QueryService(const std::string& service_name,
                      QueryServiceCallback callback) override {
      // TODO(kmackay) Could add a wrapper as needed.
      NOTIMPLEMENTED();
    }

    void WarmService(const ::service_manager::ServiceFilter& filter,
                     WarmServiceCallback callback) override {
      std::move(callback).Run(service_manager::mojom::ConnectResult::SUCCEEDED,
                              base::nullopt);
    }

    void RegisterServiceInstance(
        const ::service_manager::Identity& identity,
        mojo::ScopedMessagePipeHandle service,
        service_manager::mojom::PIDReceiverRequest pid_receiver_request,
        RegisterServiceInstanceCallback callback) override {
      // TODO(kmackay) Could add a wrapper as needed.
      NOTIMPLEMENTED();
    }

    void Clone(service_manager::mojom::ConnectorRequest request) override {
      AddBinding(std::move(request));
    }

    void FilterInterfaces(
        const std::string& spec,
        const ::service_manager::Identity& source,
        ::service_manager::mojom::InterfaceProviderRequest source_request,
        ::service_manager::mojom::InterfaceProviderPtr target) override {
      NOTREACHED() << "Call to deprecated FilterInterfaces";
    }

    ExternalMojoBroker::ConnectorImpl* const connector_;

    mojo::BindingSet<service_manager::mojom::Connector> bindings_;
  };

  struct PendingBindRequest {
    PendingBindRequest(const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe)
        : interface_name(interface_name),
          interface_pipe(std::move(interface_pipe)) {}

    const std::string interface_name;
    mojo::ScopedMessagePipeHandle interface_pipe;
  };

  void RegisterExternalServices(
      const std::vector<std::string>& external_services_to_proxy) {
    if (external_services_to_proxy.empty()) {
      return;
    }

    for (const auto& service_name : external_services_to_proxy) {
      LOG(INFO) << "Register proxy for external " << service_name;
      service_manager::mojom::ServicePtr service_ptr;
      registered_external_services_[service_name] =
          std::make_unique<ExternalServiceProxy>(
              this, service_name, mojo::MakeRequest(&service_ptr));

      service_manager::mojom::PIDReceiverPtr pid_receiver;
      connector_->RegisterServiceInstance(
          service_manager::Identity(service_name,
                                    service_manager::kSystemInstanceGroup,
                                    base::Token{}, base::Token::CreateRandom()),
          std::move(service_ptr), mojo::MakeRequest(&pid_receiver),
          base::BindOnce(&OnRegisterServiceResult, service_name));
    }
  }

  // Helper for ExternalServiceProxy.
  void BindExternalInterface(const std::string& service_name,
                             const std::string& interface_name,
                             mojo::ScopedMessagePipeHandle interface_pipe) {
    LOG(INFO) << "Internal request for " << service_name << ":"
              << interface_name;
    auto it = services_.find(service_name);
    if (it != services_.end()) {
      it->second->OnBindInterface(interface_name, std::move(interface_pipe));
      return;
    }

    ServiceNotFound(service_name, interface_name, std::move(interface_pipe));
  }

  // standalone::mojom::Connector implementation:
  void RegisterServiceInstance(const std::string& service_name,
                               mojom::ExternalServicePtr service) override {
    if (services_.find(service_name) != services_.end()) {
      LOG(ERROR) << "Duplicate service " << service_name;
      return;
    }
    LOG(INFO) << "Register service " << service_name;
    service.set_connection_error_handler(base::BindOnce(
        &ConnectorImpl::OnServiceLost, base::Unretained(this), service_name));
    auto it = services_.emplace(service_name, std::move(service)).first;

    auto p = pending_bind_requests_.find(service_name);
    if (p != pending_bind_requests_.end()) {
      for (auto& request : p->second) {
        it->second->OnBindInterface(request.interface_name,
                                    std::move(request.interface_pipe));
      }
      pending_bind_requests_.erase(p);
    }
  }

  void BindInterface(const std::string& service_name,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe) override {
    LOG(INFO) << "Request for " << service_name << ":" << interface_name;
    auto it = services_.find(service_name);
    if (it != services_.end()) {
      LOG(INFO) << "Found externally-registered " << service_name;
      it->second->OnBindInterface(interface_name, std::move(interface_pipe));
      return;
    }

    if (!connector_) {
      ServiceNotFound(service_name, interface_name, std::move(interface_pipe));
      return;
    }

    connector_->QueryService(
        service_name,
        base::BindOnce(&ConnectorImpl::OnQueryResult, base::Unretained(this),
                       service_name, interface_name,
                       std::move(interface_pipe)));
  }

  void Clone(mojom::ExternalConnectorRequest request) override {
    AddBinding(std::move(request));
  }

  void BindChromiumConnector(
      mojo::ScopedMessagePipeHandle interface_pipe) override {
    if (!connector_) {
      connector_facade_.AddBinding(
          service_manager::mojom::ConnectorRequest(std::move(interface_pipe)));
      return;
    }

    connector_->BindConnectorRequest(
        service_manager::mojom::ConnectorRequest(std::move(interface_pipe)));
  }

  void OnQueryResult(const std::string& service_name,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     service_manager::mojom::ServiceInfoPtr info) {
    if (!info) {
      ServiceNotFound(service_name, interface_name, std::move(interface_pipe));
      return;
    }

    LOG(INFO) << "Found internal " << service_name;
    connector_->BindInterface(
        service_manager::ServiceFilter::ByName(service_name), interface_name,
        std::move(interface_pipe),
        service_manager::mojom::BindInterfacePriority::kImportant,
        base::BindOnce(&OnInternalBindResult, service_name, interface_name));
  }

  void ServiceNotFound(const std::string& service_name,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) {
    LOG(INFO) << service_name << " not found";
    // Assume the service will be registered later, and wait until then.
    pending_bind_requests_[service_name].emplace_back(
        interface_name, std::move(interface_pipe));
  }

  void OnServiceLost(const std::string& service_name) {
    LOG(INFO) << service_name << " disconnected";
    services_.erase(service_name);
  }

  ServiceManagerConnectorFacade connector_facade_;
  std::unique_ptr<service_manager::Connector> connector_;

  mojo::BindingSet<mojom::ExternalConnector> bindings_;
  std::map<std::string, std::unique_ptr<ExternalServiceProxy>>
      registered_external_services_;

  std::map<std::string, mojom::ExternalServicePtr> services_;
  std::map<std::string, std::vector<PendingBindRequest>> pending_bind_requests_;

  DISALLOW_COPY_AND_ASSIGN(ConnectorImpl);
};

class ExternalMojoBroker::ReadWatcher
    : public base::MessagePumpForIO::FdWatcher {
 public:
  ReadWatcher(ConnectorImpl* connector, mojo::PlatformHandle listen_handle)
      : connector_(connector),
        listen_handle_(std::move(listen_handle)),
        watch_controller_(FROM_HERE) {
    DCHECK(listen_handle_.is_valid());
    base::MessageLoopCurrentForIO::Get().WatchFileDescriptor(
        listen_handle_.GetFD().get(), true /* persistent */,
        base::MessagePumpForIO::WATCH_READ, &watch_controller_, this);
  }

  // base::MessagePumpForIO::FdWatcher implementation:
  void OnFileCanReadWithoutBlocking(int fd) override {
    base::ScopedFD accepted_fd;
    if (mojo::AcceptSocketConnection(fd, &accepted_fd,
                                     false /* check_peer_user */) &&
        accepted_fd.is_valid()) {
      mojo::OutgoingInvitation invitation;
      auto pipe = invitation.AttachMessagePipe(0);
      mojo::OutgoingInvitation::Send(
          std::move(invitation), base::kNullProcessHandle,
          mojo::PlatformChannelEndpoint(
              mojo::PlatformHandle(std::move(accepted_fd))));
      connector_->AddBinding(mojom::ExternalConnectorRequest(std::move(pipe)));
    }
  }

  void OnFileCanWriteWithoutBlocking(int /* fd */) override {}

 private:
  ConnectorImpl* const connector_;
  const mojo::PlatformHandle listen_handle_;
  base::MessagePumpForIO::FdWatchController watch_controller_;

  DISALLOW_COPY_AND_ASSIGN(ReadWatcher);
};

ExternalMojoBroker::ExternalMojoBroker() {
  connector_ = std::make_unique<ConnectorImpl>();

  mojo::NamedPlatformChannel::Options channel_options;
  channel_options.server_name = GetBrokerPath();
  mojo::NamedPlatformChannel named_channel(channel_options);

  mojo::PlatformChannelServerEndpoint server_endpoint =
      named_channel.TakeServerEndpoint();
  DCHECK(server_endpoint.is_valid());
  read_watcher_ = std::make_unique<ReadWatcher>(
      connector_.get(), server_endpoint.TakePlatformHandle());
}

void ExternalMojoBroker::InitializeChromium(
    std::unique_ptr<service_manager::Connector> connector,
    const std::vector<std::string>& external_services_to_proxy) {
  connector_->InitializeChromium(std::move(connector),
                                 external_services_to_proxy);
}

ExternalMojoBroker::~ExternalMojoBroker() = default;

}  // namespace external_mojo
}  // namespace chromecast
