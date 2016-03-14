// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/session/blimp_client_session.h"

#include <vector>

#include "base/command_line.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "blimp/client/app/blimp_client_switches.h"
#include "blimp/client/feature/navigation_feature.h"
#include "blimp/client/feature/render_widget_feature.h"
#include "blimp/client/feature/tab_control_feature.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_message_thread_pipe.h"
#include "blimp/net/browser_connection_handler.h"
#include "blimp/net/client_connection_manager.h"
#include "blimp/net/common.h"
#include "blimp/net/connection_handler.h"
#include "blimp/net/null_blimp_message_processor.h"
#include "blimp/net/ssl_client_transport.h"
#include "blimp/net/tcp_client_transport.h"
#include "blimp/net/thread_pipe_manager.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"

namespace blimp {
namespace client {
namespace {

// Posts network events to an observer across the IO/UI thread boundary.
class CrossThreadNetworkEventObserver : public NetworkEventObserver {
 public:
  CrossThreadNetworkEventObserver(
      const base::WeakPtr<NetworkEventObserver>& target,
      const scoped_refptr<base::TaskRunner>& task_runner)
      : target_(target), task_runner_(task_runner) {}

  ~CrossThreadNetworkEventObserver() override {}

  void OnConnected() override {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&NetworkEventObserver::OnConnected, target_));
  }

  void OnDisconnected(int result) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&NetworkEventObserver::OnDisconnected, target_, result));
  }

 private:
  base::WeakPtr<NetworkEventObserver> target_;
  scoped_refptr<base::TaskRunner> task_runner_;
};

}  // namespace

// This class's functions and destruction are all invoked on the IO thread by
// the BlimpClientSession.
class ClientNetworkComponents : public ConnectionHandler,
                                public ConnectionErrorObserver {
 public:
  // Can be created on any thread.
  explicit ClientNetworkComponents(scoped_ptr<NetworkEventObserver> observer);
  ~ClientNetworkComponents() override;

  // Sets up network components.
  void Initialize();

  // Starts the connection to the engine using the given |assignment|.
  // It is required to first call Initialize.
  void ConnectWithAssignment(const Assignment& assignment);

  BrowserConnectionHandler* GetBrowserConnectionHandler();

 private:
  // ConnectionHandler implementation.
  void HandleConnection(scoped_ptr<BlimpConnection> connection) override;

  // ConnectionErrorObserver implementation.
  void OnConnectionError(int error) override;

  scoped_ptr<BrowserConnectionHandler> connection_handler_;
  scoped_ptr<ClientConnectionManager> connection_manager_;
  scoped_ptr<NetworkEventObserver> network_observer_;

  DISALLOW_COPY_AND_ASSIGN(ClientNetworkComponents);
};

ClientNetworkComponents::ClientNetworkComponents(
    scoped_ptr<NetworkEventObserver> network_observer)
    : connection_handler_(new BrowserConnectionHandler),
      network_observer_(std::move(network_observer)) {}

ClientNetworkComponents::~ClientNetworkComponents() {}

void ClientNetworkComponents::Initialize() {
  DCHECK(!connection_manager_);
  connection_manager_ = make_scoped_ptr(new ClientConnectionManager(this));
}

void ClientNetworkComponents::ConnectWithAssignment(
    const Assignment& assignment) {
  DCHECK(connection_manager_);
  connection_manager_->set_client_token(assignment.client_token);

  switch (assignment.transport_protocol) {
    case Assignment::SSL:
      DCHECK(assignment.cert);
      connection_manager_->AddTransport(make_scoped_ptr(new SSLClientTransport(
          assignment.engine_endpoint, std::move(assignment.cert), nullptr)));
      break;
    case Assignment::TCP:
      connection_manager_->AddTransport(make_scoped_ptr(
          new TCPClientTransport(assignment.engine_endpoint, nullptr)));
      break;
    case Assignment::UNKNOWN:
      DLOG(FATAL) << "Uknown transport type.";
      break;
  }

  connection_manager_->Connect();
}

BrowserConnectionHandler*
ClientNetworkComponents::GetBrowserConnectionHandler() {
  return connection_handler_.get();
}

void ClientNetworkComponents::HandleConnection(
    scoped_ptr<BlimpConnection> connection) {
  connection->AddConnectionErrorObserver(this);
  network_observer_->OnConnected();
  connection_handler_->HandleConnection(std::move(connection));
}

void ClientNetworkComponents::OnConnectionError(int result) {
  network_observer_->OnDisconnected(result);
}

BlimpClientSession::BlimpClientSession()
    : io_thread_("BlimpIOThread"),
      tab_control_feature_(new TabControlFeature),
      navigation_feature_(new NavigationFeature),
      render_widget_feature_(new RenderWidgetFeature),
      weak_factory_(this) {
  net_components_.reset(new ClientNetworkComponents(
      make_scoped_ptr(new CrossThreadNetworkEventObserver(
          weak_factory_.GetWeakPtr(),
          base::SequencedTaskRunnerHandle::Get()))));
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread_.StartWithOptions(options);

  assignment_source_.reset(
      new AssignmentSource(io_thread_.task_runner(), io_thread_.task_runner()));

  RegisterFeatures();

  // Initialize must only be posted after the RegisterFeature calls have
  // completed.
  io_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&ClientNetworkComponents::Initialize,
                            base::Unretained(net_components_.get())));
}

BlimpClientSession::~BlimpClientSession() {
  io_thread_.task_runner()->DeleteSoon(FROM_HERE, net_components_.release());
}

void BlimpClientSession::Connect(const std::string& client_auth_token) {
  assignment_source_->GetAssignment(
      client_auth_token, base::Bind(&BlimpClientSession::ConnectWithAssignment,
                                    weak_factory_.GetWeakPtr()));
}

void BlimpClientSession::ConnectWithAssignment(AssignmentSource::Result result,
                                               const Assignment& assignment) {
  OnAssignmentConnectionAttempted(result);

  if (result != AssignmentSource::Result::RESULT_OK) {
    VLOG(1) << "Assignment request failed: " << result;
    return;
  }

  io_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ClientNetworkComponents::ConnectWithAssignment,
                 base::Unretained(net_components_.get()), assignment));
}

void BlimpClientSession::OnAssignmentConnectionAttempted(
    AssignmentSource::Result result) {}

void BlimpClientSession::RegisterFeatures() {
  thread_pipe_manager_ = make_scoped_ptr(new ThreadPipeManager(
      io_thread_.task_runner(), base::SequencedTaskRunnerHandle::Get(),
      net_components_->GetBrowserConnectionHandler()));

  // Register features' message senders and receivers.
  tab_control_feature_->set_outgoing_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::TAB_CONTROL,
                                            tab_control_feature_.get()));
  navigation_feature_->set_outgoing_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::NAVIGATION,
                                            navigation_feature_.get()));
  render_widget_feature_->set_outgoing_input_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::INPUT,
                                            render_widget_feature_.get()));
  render_widget_feature_->set_outgoing_compositor_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::COMPOSITOR,
                                            render_widget_feature_.get()));

  // Client will not send send any RenderWidget messages, so don't save the
  // outgoing BlimpMessageProcessor in the RenderWidgetFeature.
  thread_pipe_manager_->RegisterFeature(BlimpMessage::RENDER_WIDGET,
                                        render_widget_feature_.get());
}

void BlimpClientSession::OnConnected() {}

void BlimpClientSession::OnDisconnected(int result) {}

TabControlFeature* BlimpClientSession::GetTabControlFeature() const {
  return tab_control_feature_.get();
}

NavigationFeature* BlimpClientSession::GetNavigationFeature() const {
  return navigation_feature_.get();
}

RenderWidgetFeature* BlimpClientSession::GetRenderWidgetFeature() const {
  return render_widget_feature_.get();
}

}  // namespace client
}  // namespace blimp
