// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_bridge/socket_tunnel_server.h"

#include "base/bind.h"
#include "base/location.h"
#include "components/devtools_bridge/abstract_data_channel.h"
#include "components/devtools_bridge/session_dependency_factory.h"
#include "components/devtools_bridge/socket_tunnel_connection.h"
#include "components/devtools_bridge/socket_tunnel_packet_handler.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/unix_domain_client_socket_posix.h"

namespace devtools_bridge {

class SocketTunnelServer::Connection : public SocketTunnelConnection {
 public:
  class Delegate {
   public:
    virtual void RemoveConnection(int index) = 0;
    virtual void SendPacket(
        const void* data, size_t length) = 0;
  };

  Connection(Delegate* delegate, int index, const std::string& socket_name)
      : SocketTunnelConnection(index),
        delegate_(delegate),
        socket_(socket_name, true) {
  }

  void Connect() {
    int result = socket()->Connect(base::Bind(
        &Connection::OnConnectionComplete, base::Unretained(this)));
    if (result != net::ERR_IO_PENDING)
      OnConnectionComplete(result);
  }

  void ClosedByClient() {
    if (socket()->IsConnected()) {
      socket()->Disconnect();
      SendControlPacket(SERVER_CLOSE);
    }
    delegate_->RemoveConnection(index_);
  }

 protected:
  net::StreamSocket* socket() override {
    return &socket_;
  }

  void OnDataPacketRead(const void* data, size_t length) override {
    delegate_->SendPacket(data, length);
    ReadNextChunk();
  }

  void OnReadError(int error) override {
    socket()->Disconnect();
    SendControlPacket(SERVER_CLOSE);
    delegate_->RemoveConnection(index_);
    delegate_ = NULL;
  }

 private:
  void OnConnectionComplete(int result) {
    if (result == net::OK) {
      SendControlPacket(SERVER_OPEN_ACK);
      ReadNextChunk();
    } else {
      SendControlPacket(SERVER_CLOSE);
      delegate_->RemoveConnection(index_);
      delegate_ = NULL;
    }
  }

  void SendControlPacket(ServerOpCode op_code) {
    char buffer[kControlPacketSizeBytes];
    BuildControlPacket(buffer, op_code);
    delegate_->SendPacket(buffer, kControlPacketSizeBytes);
  }

  Delegate* delegate_;
  net::UnixDomainClientSocket socket_;
};

/**
 * Lives on the IO thread.
 */
class SocketTunnelServer::ConnectionController
    : private Connection::Delegate {
 public:
  ConnectionController(
      scoped_refptr<base::TaskRunner> io_task_runner,
      scoped_refptr<AbstractDataChannel::Proxy> data_channel,
      const std::string& socket_name)
      : io_task_runner_(io_task_runner),
        data_channel_(data_channel),
        socket_name_(socket_name) {
    DCHECK(data_channel_.get());
  }

  void HandleControlPacket(int connection_index, int op_code) {
    DCHECK(connection_index < kMaxConnectionCount);
    switch (op_code) {
      case SocketTunnelConnection::CLIENT_OPEN:
        if (connections_[connection_index].get() != NULL) {
          DLOG(ERROR) << "Opening connection which already open: "
                      << connection_index;
          HandleProtocolError();
          return;
        }
        connections_[connection_index].reset(
            new Connection(this, connection_index, socket_name_));
        connections_[connection_index]->Connect();
        break;

      case SocketTunnelConnection::CLIENT_CLOSE:
        if (connections_[connection_index].get() == NULL) {
          // Ignore. Client may close the connection before received
          // notification from the server.
          return;
        }
        connections_[connection_index]->ClosedByClient();
        break;

      default:
        DLOG(ERROR) << "Invalid op_code: " << op_code;
        HandleProtocolError();
        return;
    }
  }

  void HandleDataPacket(int connection_index,
                        scoped_refptr<net::IOBufferWithSize> packet) {
    Connection* connection = connections_[connection_index].get();
    if (connection != NULL)
      connection->Write(packet);
  }

  void HandleProtocolError() {
    data_channel_->Close();
  }

  void CloseAllConnections() {
    for (int i = 0; i < kMaxConnectionCount; i++) {
      connections_[i].reset();
    }
  }

 private:
  static void DeleteConnectionImpl(Connection*) {}

  // Connection::Delegate implementation
  void RemoveConnection(int connection_index) override {
    // Remove immediately, delete later to preserve this of the caller.
    Connection* connection = connections_[connection_index].release();
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ConnectionController::DeleteConnectionImpl,
                              base::Owned(connection)));
  }

  void SendPacket(const void* data, size_t length) override {
    data_channel_->SendBinaryMessage(data, length);
  }

  static const int kMaxConnectionCount =
      SocketTunnelConnection::kMaxConnectionCount;

  scoped_refptr<base::TaskRunner> io_task_runner_;
  scoped_refptr<AbstractDataChannel::Proxy> data_channel_;
  scoped_ptr<Connection> connections_[kMaxConnectionCount];
  const std::string socket_name_;
};

class SocketTunnelServer::DataChannelObserver
    : public AbstractDataChannel::Observer,
      private SocketTunnelPacketHandler {
 public:
  DataChannelObserver(scoped_refptr<base::TaskRunner> io_task_runner,
                      scoped_ptr<ConnectionController> controller)
      : io_task_runner_(io_task_runner),
        controller_(controller.Pass()) {
  }

  ~DataChannelObserver() override {
    // Deleting on IO thread allows post tasks with base::Unretained
    // because all of them will be processed before deletion.
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DataChannelObserver::DeleteControllerOnIOThread,
                              base::Passed(&controller_)));
  }

  void OnOpen() override {
    // Nothing to do. Activity could only be initiated by a control packet.
  }

  void OnClose() override {
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(
            &ConnectionController::CloseAllConnections,
            base::Unretained(controller_.get())));
  }

  void OnMessage(const void* data, size_t length) override {
    DecodePacket(data, length);
  }

 private:
  static void DeleteControllerOnIOThread(
      scoped_ptr<ConnectionController> controller) {}

  // SocketTunnelPacketHandler implementation.

  void HandleControlPacket(int connection_index, int op_code) override {
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(
            &ConnectionController::HandleControlPacket,
            base::Unretained(controller_.get()),
            connection_index,
            op_code));
  }

  void HandleDataPacket(int connection_index,
                        scoped_refptr<net::IOBufferWithSize> data) override {
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(
            &ConnectionController::HandleDataPacket,
            base::Unretained(controller_.get()),
            connection_index,
            data));
  }

  void HandleProtocolError() override {
      io_task_runner_->PostTask(
          FROM_HERE, base::Bind(
              &ConnectionController::HandleProtocolError,
              base::Unretained(controller_.get())));
  }

  const scoped_refptr<base::TaskRunner> io_task_runner_;
  scoped_ptr<ConnectionController> controller_;
};

SocketTunnelServer::SocketTunnelServer(SessionDependencyFactory* factory,
                                       AbstractDataChannel* data_channel,
                                       const std::string& socket_name)
    : data_channel_(data_channel) {
  scoped_ptr<ConnectionController> controller(
      new ConnectionController(factory->io_thread_task_runner(),
                               data_channel->proxy(),
                               socket_name));

  scoped_ptr<DataChannelObserver> data_channel_observer(
      new DataChannelObserver(factory->io_thread_task_runner(),
                              controller.Pass()));

  data_channel_->RegisterObserver(data_channel_observer.Pass());
}

SocketTunnelServer::~SocketTunnelServer() {
  data_channel_->UnregisterObserver();
}

}  // namespace devtools_bridge
