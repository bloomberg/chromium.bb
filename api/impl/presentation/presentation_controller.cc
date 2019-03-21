// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/presentation/presentation_controller.h"

#include <algorithm>

#include "api/impl/presentation/url_availability_requester.h"
#include "api/public/message_demuxer.h"
#include "api/public/network_service_manager.h"
#include "api/public/protocol_connection_client.h"
#include "msgs/osp_messages.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace presentation {

Controller::ReceiverWatch::ReceiverWatch() = default;
Controller::ReceiverWatch::ReceiverWatch(Controller* controller,
                                         const std::vector<std::string>& urls,
                                         ReceiverObserver* observer)
    : urls_(urls), observer_(observer), controller_(controller) {}

Controller::ReceiverWatch::ReceiverWatch(Controller::ReceiverWatch&& other) {
  swap(*this, other);
}

Controller::ReceiverWatch::~ReceiverWatch() {
  if (observer_) {
    controller_->CancelReceiverWatch(urls_, observer_);
  }
  observer_ = nullptr;
}

Controller::ReceiverWatch& Controller::ReceiverWatch::operator=(
    Controller::ReceiverWatch other) {
  swap(*this, other);
  return *this;
}

void swap(Controller::ReceiverWatch& a, Controller::ReceiverWatch& b) {
  using std::swap;
  swap(a.urls_, b.urls_);
  swap(a.observer_, b.observer_);
  swap(a.controller_, b.controller_);
}

Controller::ConnectRequest::ConnectRequest() = default;
Controller::ConnectRequest::ConnectRequest(Controller* controller,
                                           const std::string& service_id,
                                           bool is_reconnect,
                                           absl::optional<uint64_t> request_id)
    : service_id_(service_id),
      is_reconnect_(is_reconnect),
      request_id_(request_id),
      controller_(controller) {}

Controller::ConnectRequest::ConnectRequest(ConnectRequest&& other) {
  swap(*this, other);
}

Controller::ConnectRequest::~ConnectRequest() {
  if (request_id_) {
    controller_->CancelConnectRequest(service_id_, is_reconnect_,
                                      request_id_.value());
  }
  request_id_ = 0;
}

Controller::ConnectRequest& Controller::ConnectRequest::operator=(
    ConnectRequest other) {
  swap(*this, other);
  return *this;
}

void swap(Controller::ConnectRequest& a, Controller::ConnectRequest& b) {
  using std::swap;
  swap(a.service_id_, b.service_id_);
  swap(a.is_reconnect_, b.is_reconnect_);
  swap(a.request_id_, b.request_id_);
  swap(a.controller_, b.controller_);
}

Controller::Controller(Clock* clock) {
  availability_requester_ = std::make_unique<UrlAvailabilityRequester>(clock);
  const std::vector<ServiceInfo>& receivers =
      NetworkServiceManager::Get()->GetMdnsServiceListener()->GetReceivers();
  for (const auto& info : receivers) {
    // TODO(issue/33): Replace service_id with endpoint_id when endpoint_id is
    // more than just an IPEndpoint counter and actually relates to a device's
    // identity.
    receiver_endpoints_.emplace(info.service_id, info.v4_endpoint.port
                                                     ? info.v4_endpoint
                                                     : info.v6_endpoint);
    availability_requester_->AddReceiver(info);
  }
  // TODO(btolsch): This is for |receiver_endpoints_|, but this should really be
  // tracked elsewhere so it's available to other protocols as well.
  NetworkServiceManager::Get()->GetMdnsServiceListener()->AddObserver(this);
}

Controller::~Controller() {
  connection_manager_.reset();
  NetworkServiceManager::Get()->GetMdnsServiceListener()->RemoveObserver(this);
}

Controller::ReceiverWatch Controller::RegisterReceiverWatch(
    const std::vector<std::string>& urls,
    ReceiverObserver* observer) {
  availability_requester_->AddObserver(urls, observer);
  return ReceiverWatch(this, urls, observer);
}

Controller::ConnectRequest Controller::StartPresentation(
    const std::string& url,
    const std::string& service_id,
    RequestDelegate* delegate,
    Connection::Delegate* conn_delegate) {
  OSP_UNIMPLEMENTED();
  return ConnectRequest();
}

Controller::ConnectRequest Controller::ReconnectPresentation(
    const std::vector<std::string>& urls,
    const std::string& presentation_id,
    const std::string& service_id,
    RequestDelegate* delegate,
    Connection::Delegate* conn_delegate) {
  OSP_UNIMPLEMENTED();
  return ConnectRequest();
}

Controller::ConnectRequest Controller::ReconnectConnection(
    std::unique_ptr<Connection> connection,
    RequestDelegate* delegate) {
  OSP_UNIMPLEMENTED();
  return ConnectRequest();
}

void Controller::OnPresentationTerminated(const std::string& presentation_id,
                                          TerminationReason reason) {
  OSP_UNIMPLEMENTED();
}

std::string Controller::GetServiceIdForPresentationId(
    const std::string& presentation_id) const {
  auto presentation_entry = presentations_.find(presentation_id);
  if (presentation_entry == presentations_.end()) {
    return "";
  }
  return presentation_entry->second.service_id;
}

ProtocolConnection* Controller::GetConnectionRequestGroupStream(
    const std::string& service_id) {
  OSP_UNIMPLEMENTED();
  return nullptr;
}

uint64_t Controller::GetNextRequestId() {
  return next_request_id_++;
}

void Controller::OnError(ServiceListenerError) {}
void Controller::OnMetrics(ServiceListener::Metrics) {}

// static
std::string Controller::MakePresentationId(const std::string& url,
                                           const std::string& service_id) {
  OSP_UNIMPLEMENTED();
  return {};
}

// TODO(btolsch): This should be per-endpoint since spec now omits presentation
// ID in many places.
uint64_t Controller::GetNextConnectionId(const std::string& id) {
  return next_connection_id_[id]++;
}

void Controller::OnConnectionDestroyed(Connection* connection) {
  OSP_UNIMPLEMENTED();
}

void Controller::CancelReceiverWatch(const std::vector<std::string>& urls,
                                     ReceiverObserver* observer) {
  availability_requester_->RemoveObserverUrls(urls, observer);
}

void Controller::CancelConnectRequest(const std::string& service_id,
                                      bool is_reconnect,
                                      uint64_t request_id) {
  OSP_UNIMPLEMENTED();
}

void Controller::OnStarted() {}
void Controller::OnStopped() {}
void Controller::OnSuspended() {}
void Controller::OnSearching() {}

void Controller::OnReceiverAdded(const ServiceInfo& info) {
  receiver_endpoints_.emplace(info.service_id, info.v4_endpoint.port
                                                   ? info.v4_endpoint
                                                   : info.v6_endpoint);
  availability_requester_->AddReceiver(info);
}

void Controller::OnReceiverChanged(const ServiceInfo& info) {
  receiver_endpoints_[info.service_id] =
      info.v4_endpoint.port ? info.v4_endpoint : info.v6_endpoint;
  availability_requester_->ChangeReceiver(info);
}

void Controller::OnReceiverRemoved(const ServiceInfo& info) {
  receiver_endpoints_.erase(info.service_id);
  availability_requester_->RemoveReceiver(info);
}

void Controller::OnAllReceiversRemoved() {
  receiver_endpoints_.clear();
  availability_requester_->RemoveAllReceivers();
}

}  // namespace presentation
}  // namespace openscreen
