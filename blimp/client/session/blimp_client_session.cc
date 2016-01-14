// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/session/blimp_client_session.h"

#include "base/command_line.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "blimp/client/blimp_client_switches.h"
#include "blimp/client/session/navigation_feature.h"
#include "blimp/client/session/render_widget_feature.h"
#include "blimp/client/session/tab_control_feature.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_message_thread_pipe.h"
#include "blimp/net/browser_connection_handler.h"
#include "blimp/net/client_connection_manager.h"
#include "blimp/net/common.h"
#include "blimp/net/null_blimp_message_processor.h"
#include "blimp/net/tcp_client_transport.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"

namespace blimp {
namespace client {
namespace {

// TODO(kmarshall): Take values from configuration data.
const char kDummyClientToken[] = "MyVoiceIsMyPassport";
const std::string kDefaultBlimpletIPAddress = "127.0.0.1";
const uint16_t kDefaultBlimpletTCPPort = 25467;

// A BlimpletAssignment contains the configuration data needed for a client
// to connect to the engine.
struct BlimpletAssignment {
  net::IPEndPoint ip_endpoint;
};

net::IPAddress GetBlimpletIPAddress() {
  std::string host;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kBlimpletHost)) {
    host = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kBlimpletHost);
  } else {
    host = kDefaultBlimpletIPAddress;
  }
  net::IPAddress ip_address;
  if (!net::IPAddress::FromIPLiteral(host, &ip_address))
    CHECK(false) << "Invalid BlimpletAssignment host " << host;
  return ip_address;
}

uint16_t GetBlimpletTCPPort() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kBlimpletTCPPort)) {
    std::string port_str =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kBlimpletTCPPort);
    uint port_64t;
    if (!base::StringToUint(port_str, &port_64t) ||
        !base::IsValueInRangeForNumericType<uint16_t>(port_64t)) {
      CHECK(false) << "Invalid BlimpletAssignment port " << port_str;
    }
    return base::checked_cast<uint16_t>(port_64t);
  } else {
    return kDefaultBlimpletTCPPort;
  }
}

}  // namespace

// This class's functions and destruction are all invoked on the IO thread by
// the BlimpClientSession.
// TODO(haibinlu): crbug/574884
class ClientNetworkComponents {
 public:
  // Can be created on any thread.
  ClientNetworkComponents() {}

  ~ClientNetworkComponents() {}

  // Sets up network components and starts to connect to the engine.
  void Initialize(const net::AddressList& address_list);

  // Invoked by BlimpEngineSession to finish feature registration on IO thread:
  // using |incoming_proxy| as the incoming message processor, and connecting
  // |outgoing_pipe| to the actual message sender.
  void RegisterFeature(BlimpMessage::Type type,
                       scoped_ptr<BlimpMessageThreadPipe> outgoing_pipe,
                       scoped_ptr<BlimpMessageProcessor> incoming_proxy);

 private:
  scoped_ptr<BrowserConnectionHandler> browser_connection_handler_;
  scoped_ptr<ClientConnectionManager> connection_manager_;

  // Container for the feature-specific MessageProcessors.
  std::vector<scoped_ptr<BlimpMessageProcessor>> incoming_proxies_;

  // Containers for the MessageProcessors used to write feature-specific
  // messages to the network, and the thread-pipe endpoints through which
  // they are used from the UI thread.
  std::vector<scoped_ptr<BlimpMessageThreadPipe>> outgoing_pipes_;
  std::vector<scoped_ptr<BlimpMessageProcessor>> outgoing_message_processors_;

  DISALLOW_COPY_AND_ASSIGN(ClientNetworkComponents);
};

void ClientNetworkComponents::Initialize(const net::AddressList& address_list) {
  DCHECK(!connection_manager_);
  connection_manager_ = make_scoped_ptr(
      new ClientConnectionManager(browser_connection_handler_.get()));
  connection_manager_->set_client_token(kDummyClientToken);

  connection_manager_->AddTransport(
      make_scoped_ptr(new TCPClientTransport(address_list, nullptr)));

  connection_manager_->Connect();
}

void ClientNetworkComponents::RegisterFeature(
    BlimpMessage::Type type,
    scoped_ptr<BlimpMessageThreadPipe> outgoing_pipe,
    scoped_ptr<BlimpMessageProcessor> incoming_proxy) {
  if (!browser_connection_handler_) {
    browser_connection_handler_ = make_scoped_ptr(new BrowserConnectionHandler);
  }

  // Registers |incoming_proxy| as the message processor for incoming
  // messages with |type|. Sets the returned outgoing message processor as the
  // actual sender of the |outgoing_pipe|.
  scoped_ptr<BlimpMessageProcessor> outgoing_message_processor =
      browser_connection_handler_->RegisterFeature(type, incoming_proxy.get());
  outgoing_pipe->set_target_processor(outgoing_message_processor.get());

  incoming_proxies_.push_back(std::move(incoming_proxy));
  outgoing_pipes_.push_back(std::move(outgoing_pipe));
  outgoing_message_processors_.push_back(std::move(outgoing_message_processor));
}

BlimpClientSession::BlimpClientSession()
    : io_thread_("BlimpIOThread"),
      tab_control_feature_(new TabControlFeature),
      navigation_feature_(new NavigationFeature),
      render_widget_feature_(new RenderWidgetFeature),
      net_components_(new ClientNetworkComponents) {
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread_.StartWithOptions(options);

  // Register features' message senders and receivers.
  tab_control_feature_->set_outgoing_message_processor(
      RegisterFeature(BlimpMessage::TAB_CONTROL, tab_control_feature_.get()));
  navigation_feature_->set_outgoing_message_processor(
      RegisterFeature(BlimpMessage::NAVIGATION, navigation_feature_.get()));
  render_widget_feature_->set_outgoing_input_message_processor(
      RegisterFeature(BlimpMessage::INPUT, render_widget_feature_.get()));
  render_widget_feature_->set_outgoing_compositor_message_processor(
      RegisterFeature(BlimpMessage::COMPOSITOR, render_widget_feature_.get()));

  // We don't expect to send any RenderWidget messages, so don't save the
  // outgoing BlimpMessageProcessor in the RenderWidgetFeature.
  RegisterFeature(BlimpMessage::RENDER_WIDGET, render_widget_feature_.get());

  // Initialize must only be posted after the RegisterFeature calls have
  // completed.
  io_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&ClientNetworkComponents::Initialize,
                            base::Unretained(net_components_.get()),
                            net::AddressList(GetBlimpletIPEndpoint())));
}

BlimpClientSession::~BlimpClientSession() {
  io_thread_.task_runner()->DeleteSoon(FROM_HERE, net_components_.release());
}

scoped_ptr<BlimpMessageProcessor> BlimpClientSession::RegisterFeature(
    BlimpMessage::Type type,
    BlimpMessageProcessor* incoming_processor) {
  // Creates an outgoing pipe and a proxy for forwarding messages
  // from features on the UI thread to network components on the IO thread.
  scoped_ptr<BlimpMessageThreadPipe> outgoing_pipe(
      new BlimpMessageThreadPipe(io_thread_.task_runner()));
  scoped_ptr<BlimpMessageProcessor> outgoing_message_proxy =
      outgoing_pipe->CreateProxy();

  // Creates an incoming pipe and a proxy for receiving messages
  // from network components on the IO thread.
  scoped_ptr<BlimpMessageThreadPipe> incoming_pipe(
      new BlimpMessageThreadPipe(base::SequencedTaskRunnerHandle::Get()));
  incoming_pipe->set_target_processor(incoming_processor);
  scoped_ptr<BlimpMessageProcessor> incoming_proxy =
      incoming_pipe->CreateProxy();

  // Finishes registration on IO thread.
  io_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&ClientNetworkComponents::RegisterFeature,
                            base::Unretained(net_components_.get()), type,
                            base::Passed(std::move(outgoing_pipe)),
                            base::Passed(std::move(incoming_proxy))));

  incoming_pipes_.push_back(std::move(incoming_pipe));
  return outgoing_message_proxy;
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

net::IPEndPoint BlimpClientSession::GetBlimpletIPEndpoint() {
  return net::IPEndPoint(GetBlimpletIPAddress(), GetBlimpletTCPPort());
}

}  // namespace client
}  // namespace blimp
