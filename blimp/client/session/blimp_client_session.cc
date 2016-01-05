// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/session/blimp_client_session.h"

#include "blimp/client/session/navigation_feature.h"
#include "blimp/client/session/render_widget_feature.h"
#include "blimp/client/session/tab_control_feature.h"
#include "blimp/net/browser_connection_handler.h"
#include "blimp/net/client_connection_manager.h"
#include "blimp/net/common.h"
#include "blimp/net/null_blimp_message_processor.h"
#include "blimp/net/tcp_client_transport.h"

namespace blimp {
namespace {

// TODO(kmarshall): Take values from configuration data.
const char kDummyClientToken[] = "MyVoiceIsMyPassport";
const uint16_t kDefaultTcpPort = 25467;

net::IPAddressNumber GetEndpointIPAddress() {
  // Just connect to localhost for now.
  // TODO(kmarshall): Take IP address from configuration data.
  net::IPAddressNumber output;
  output.push_back(0);
  output.push_back(0);
  output.push_back(0);
  output.push_back(0);
  return output;
}

}  // namespace

class ClientNetworkComponents {
 public:
  // Can be created on any thread.
  ClientNetworkComponents() {}

  // Must be destroyed on the IO thread.
  ~ClientNetworkComponents() {}

  // Creates instances for |browser_connection_handler_| and
  // |connection_manager_|.
  // Must be called on the IO thread.
  void Initialize();

 private:
  scoped_ptr<BrowserConnectionHandler> browser_connection_handler_;
  scoped_ptr<ClientConnectionManager> connection_manager_;

  DISALLOW_COPY_AND_ASSIGN(ClientNetworkComponents);
};

void ClientNetworkComponents::Initialize() {
  browser_connection_handler_ = make_scoped_ptr(new BrowserConnectionHandler);
  connection_manager_ = make_scoped_ptr(
      new ClientConnectionManager(browser_connection_handler_.get()));
  connection_manager_->set_client_token(kDummyClientToken);

  connection_manager_->AddTransport(make_scoped_ptr(
      new TCPClientTransport(net::AddressList(net::IPEndPoint(
                                 GetEndpointIPAddress(), kDefaultTcpPort)),
                             nullptr)));

  connection_manager_->Connect();
}

BlimpClientSession::BlimpClientSession()
    : io_thread_("BlimpIOThread"),
      tab_control_feature_(new TabControlFeature),
      navigation_feature_(new NavigationFeature),
      render_widget_feature_(new RenderWidgetFeature),
      network_components_(new ClientNetworkComponents) {
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread_.StartWithOptions(options);
  io_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&ClientNetworkComponents::Initialize,
                            base::Unretained(network_components_.get())));

  // Register features.
  // TODO(haibinlu): connect the features with the network layer, with support
  // for thread hopping.
  tab_control_feature_->set_outgoing_message_processor(
      make_scoped_ptr(new NullBlimpMessageProcessor));
  navigation_feature_->set_outgoing_message_processor(
      make_scoped_ptr(new NullBlimpMessageProcessor));
  render_widget_feature_->set_outgoing_input_message_processor(
      make_scoped_ptr(new NullBlimpMessageProcessor));
  render_widget_feature_->set_outgoing_compositor_message_processor(
      make_scoped_ptr(new NullBlimpMessageProcessor));
}

BlimpClientSession::~BlimpClientSession() {
  io_thread_.task_runner()->DeleteSoon(FROM_HERE,
                                       network_components_.release());
}

TabControlFeature* BlimpClientSession::GetTabControlFeature() const {
  return tab_control_feature_.get();
}

NavigationFeature* BlimpClientSession::GetNavigationFeature() const {
  return navigation_feature_.get();
}

RenderWidgetFeature* BlimpClientSession::GetRenderWidgetFeature() const {
  return render_widget_feature_.get();
}

}  // namespace blimp
