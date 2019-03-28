// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/broadcast_channel/broadcast_channel_provider.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

// There is a one-to-one mapping of BroadcastChannel instances in the renderer
// and Connection instances in the browser. The Connection is owned by a
// BroadcastChannelProvider.
class BroadcastChannelProvider::Connection
    : public blink::mojom::BroadcastChannelClient {
 public:
  Connection(const url::Origin& origin,
             const std::string& name,
             blink::mojom::BroadcastChannelClientAssociatedPtrInfo client,
             blink::mojom::BroadcastChannelClientAssociatedRequest connection,
             BroadcastChannelProvider* service);

  void OnMessage(blink::CloneableMessage message) override;
  void MessageToClient(const blink::CloneableMessage& message) const {
    client_->OnMessage(message.ShallowClone());
  }
  const url::Origin& origin() const { return origin_; }
  const std::string& name() const { return name_; }

  void set_connection_error_handler(const base::Closure& error_handler) {
    binding_.set_connection_error_handler(error_handler);
    client_.set_connection_error_handler(error_handler);
  }

 private:
  mojo::AssociatedBinding<blink::mojom::BroadcastChannelClient> binding_;
  blink::mojom::BroadcastChannelClientAssociatedPtr client_;

  BroadcastChannelProvider* service_;
  url::Origin origin_;
  std::string name_;
};

BroadcastChannelProvider::Connection::Connection(
    const url::Origin& origin,
    const std::string& name,
    blink::mojom::BroadcastChannelClientAssociatedPtrInfo client,
    blink::mojom::BroadcastChannelClientAssociatedRequest connection,
    BroadcastChannelProvider* service)
    : binding_(this, std::move(connection)),
      service_(service),
      origin_(origin),
      name_(name) {
  client_.Bind(std::move(client));
}

void BroadcastChannelProvider::Connection::OnMessage(
    blink::CloneableMessage message) {
  service_->ReceivedMessageOnConnection(this, message);
}

BroadcastChannelProvider::BroadcastChannelProvider() {}

mojo::BindingId BroadcastChannelProvider::Connect(
    RenderProcessHostId render_process_host_id,
    blink::mojom::BroadcastChannelProviderRequest request) {
  return bindings_.AddBinding(this, std::move(request), render_process_host_id);
}

void BroadcastChannelProvider::ConnectToChannel(
    const url::Origin& origin,
    const std::string& name,
    blink::mojom::BroadcastChannelClientAssociatedPtrInfo client,
    blink::mojom::BroadcastChannelClientAssociatedRequest connection) {
  RenderProcessHostId process_id = bindings_.dispatch_context();
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();

  // TODO(943887): Replace HasSecurityState() call with something that can
  // preserve security state after process shutdown. The security state check
  // is a temporary solution to avoid crashes when this method is run after the
  // process associated with |process_id| has been destroyed. It temporarily
  // restores the old behavior of always allowing access if the process is gone.
  // See https://crbug.com/943027 for details.
  if (!policy->CanAccessDataForOrigin(process_id, origin) &&
      policy->HasSecurityState(process_id)) {
    mojo::ReportBadMessage("BROADCAST_CHANNEL_INVALID_ORIGIN");
    return;
  }

  std::unique_ptr<Connection> c(new Connection(origin, name, std::move(client),
                                               std::move(connection), this));
  c->set_connection_error_handler(
      base::Bind(&BroadcastChannelProvider::UnregisterConnection,
                 base::Unretained(this), c.get()));
  connections_[origin].insert(std::make_pair(name, std::move(c)));
}

BroadcastChannelProvider::~BroadcastChannelProvider() {}

void BroadcastChannelProvider::UnregisterConnection(Connection* c) {
  url::Origin origin = c->origin();
  auto& connections = connections_[origin];
  for (auto it = connections.lower_bound(c->name()),
            end = connections.upper_bound(c->name());
       it != end; ++it) {
    if (it->second.get() == c) {
      connections.erase(it);
      break;
    }
  }
  if (connections.empty())
    connections_.erase(origin);
}

void BroadcastChannelProvider::ReceivedMessageOnConnection(
    Connection* c,
    const blink::CloneableMessage& message) {
  auto& connections = connections_[c->origin()];
  for (auto it = connections.lower_bound(c->name()),
            end = connections.upper_bound(c->name());
       it != end; ++it) {
    if (it->second.get() != c)
      it->second->MessageToClient(message);
  }
}

}  // namespace content
