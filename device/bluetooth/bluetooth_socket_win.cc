// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_socket_win.h"

#include <objbase.h>

#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "device/bluetooth/bluetooth_init_win.h"
#include "device/bluetooth/bluetooth_service_record_win.h"
#include "device/bluetooth/bluetooth_socket_thread_win.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/winsock_init.h"

namespace {

const char kL2CAPNotSupported[] = "Bluetooth L2CAP protocal is not supported";
const char kSocketAlreadyConnected[] = "Socket is already connected.";
const char kSocketNotConnected[] = "Socket is not connected.";
const char kInvalidRfcommPort[] = "Invalid RFCCOMM port.";
const char kFailedToCreateSocket[] = "Failed to create socket.";
const char kFailedToBindSocket[] = "Failed to bind socket.";
const char kFailedToListenOnSocket[] = "Failed to listen on socket.";
const char kFailedToGetSockNameForSocket[] = "Failed to getsockname.";
const char kBadUuid[] = "Bad uuid.";
const char kWsaSetServiceError[] = "WSASetService error.";

using device::BluetoothSocketWin;

static void DeactivateSocket(
    const scoped_refptr<device::BluetoothSocketThreadWin>& socket_thread) {
  socket_thread->OnSocketDeactivate();
}

}  // namespace

namespace device {

struct BluetoothSocketWin::ServiceRegData {
  ServiceRegData() {
    ZeroMemory(&address, sizeof(address));
    ZeroMemory(&address_info, sizeof(address_info));
    ZeroMemory(&uuid, sizeof(uuid));
    ZeroMemory(&service, sizeof(service));
  }

  SOCKADDR_BTH address;
  CSADDR_INFO address_info;
  GUID uuid;
  base::string16 name;
  WSAQUERYSET service;
};

// static
scoped_refptr<BluetoothSocketWin> BluetoothSocketWin::CreateBluetoothSocket(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<BluetoothSocketThreadWin> socket_thread,
    net::NetLog* net_log,
    const net::NetLog::Source& source) {
  DCHECK(ui_task_runner->RunsTasksOnCurrentThread());

  return make_scoped_refptr(
      new BluetoothSocketWin(ui_task_runner, socket_thread, net_log, source));
}

BluetoothSocketWin::BluetoothSocketWin(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<BluetoothSocketThreadWin> socket_thread,
    net::NetLog* net_log,
    const net::NetLog::Source& source)
    : ui_task_runner_(ui_task_runner),
      socket_thread_(socket_thread),
      net_log_(net_log),
      source_(source),
      supports_rfcomm_(false),
      rfcomm_channel_(-1),
      bth_addr_(BTH_ADDR_NULL) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  socket_thread->OnSocketActivate();
}

BluetoothSocketWin::~BluetoothSocketWin() {
  ui_task_runner_->PostTask(FROM_HERE,
                            base::Bind(&DeactivateSocket, socket_thread_));
}

void BluetoothSocketWin::StartService(
    const BluetoothUUID& uuid,
    const std::string& name,
    int rfcomm_channel,
    const base::Closure& success_callback,
    const ErrorCompletionCallback& error_callback,
    const OnNewConnectionCallback& new_connection_callback) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  socket_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothSocketWin::DoStartService,
                 this,
                 uuid,
                 name,
                 rfcomm_channel,
                 success_callback,
                 error_callback,
                 new_connection_callback));
}

void BluetoothSocketWin::Close() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  socket_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&BluetoothSocketWin::DoClose, this));
}

void BluetoothSocketWin::Connect(
    const BluetoothServiceRecord& service_record,
    const base::Closure& success_callback,
    const ErrorCompletionCallback& error_callback) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  const BluetoothServiceRecordWin* service_record_win =
      static_cast<const BluetoothServiceRecordWin*>(&service_record);
  device_address_ = service_record_win->address();
  if (service_record.SupportsRfcomm()) {
    supports_rfcomm_ = true;
    rfcomm_channel_ = service_record_win->rfcomm_channel();
    bth_addr_ = service_record_win->bth_addr();
  }

  socket_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(
          &BluetoothSocketWin::DoConnect,
          this,
          base::Bind(&BluetoothSocketWin::PostSuccess, this, success_callback),
          base::Bind(
              &BluetoothSocketWin::PostErrorCompletion, this, error_callback)));
}

void BluetoothSocketWin::Disconnect(const base::Closure& success_callback) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  socket_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(
          &BluetoothSocketWin::DoDisconnect,
          this,
          base::Bind(
              &BluetoothSocketWin::PostSuccess, this, success_callback)));
}

void BluetoothSocketWin::Receive(
    int buffer_size,
    const ReceiveCompletionCallback& success_callback,
    const ReceiveErrorCompletionCallback& error_callback) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  socket_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothSocketWin::DoReceive,
                 this,
                 buffer_size,
                 base::Bind(&BluetoothSocketWin::PostReceiveCompletion,
                            this,
                            success_callback),
                 base::Bind(&BluetoothSocketWin::PostReceiveErrorCompletion,
                            this,
                            error_callback)));
}

void BluetoothSocketWin::Send(scoped_refptr<net::IOBuffer> buffer,
                              int buffer_size,
                              const SendCompletionCallback& success_callback,
                              const ErrorCompletionCallback& error_callback) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  socket_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(
          &BluetoothSocketWin::DoSend,
          this,
          buffer,
          buffer_size,
          base::Bind(
              &BluetoothSocketWin::PostSendCompletion, this, success_callback),
          base::Bind(
              &BluetoothSocketWin::PostErrorCompletion, this, error_callback)));
}

void BluetoothSocketWin::DoClose() {
  DCHECK(socket_thread_->task_runner()->RunsTasksOnCurrentThread());
  base::ThreadRestrictions::AssertIOAllowed();

  if (tcp_socket_) {
    tcp_socket_->Close();
    tcp_socket_.reset(NULL);
  }

  // Note: Closing |tcp_socket_| above released all potential pending
  // Send/Receive operations, so we can no safely release the state associated
  // to those pending operations.
  read_buffer_ = NULL;
  std::queue<linked_ptr<WriteRequest> > empty;
  write_queue_.swap(empty);

  if (service_reg_data_) {
    if (WSASetService(&service_reg_data_->service,RNRSERVICE_DELETE, 0) ==
        SOCKET_ERROR) {
      LOG(WARNING) << "Failed to unregister service.";
    }
    service_reg_data_.reset();
  }
}

void BluetoothSocketWin::DoConnect(
    const base::Closure& success_callback,
    const ErrorCompletionCallback& error_callback) {
  DCHECK(socket_thread_->task_runner()->RunsTasksOnCurrentThread());
  base::ThreadRestrictions::AssertIOAllowed();

  if (tcp_socket_) {
    error_callback.Run(kSocketAlreadyConnected);
    return;
  }

  if (!supports_rfcomm_) {
    // TODO(youngki) add support for L2CAP sockets as well.
    error_callback.Run(kL2CAPNotSupported);
    return;
  }

  tcp_socket_.reset(new net::TCPSocket(net_log_, source_));
  net::EnsureWinsockInit();
  SOCKET socket_fd = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
  SOCKADDR_BTH sa;
  ZeroMemory(&sa, sizeof(sa));
  sa.addressFamily = AF_BTH;
  sa.port = rfcomm_channel_;
  sa.btAddr = bth_addr_;

  // TODO(rpaquay): Condider making this call non-blocking.
  int status = connect(socket_fd, reinterpret_cast<SOCKADDR*>(&sa), sizeof(sa));
  DWORD error_code = WSAGetLastError();
  if (!(status == 0 || error_code == WSAEINPROGRESS)) {
    LOG(ERROR) << "Failed to connect bluetooth socket "
               << "(" << device_address_ << "): "
               << logging::SystemErrorCodeToString(error_code);
    error_callback.Run("Error connecting to socket: " +
                       logging::SystemErrorCodeToString(error_code));
    closesocket(socket_fd);
    return;
  }

  // Note: We don't have a meaningful |IPEndPoint|, but that is ok since the
  // TCPSocket implementation does not actually require one.
  int net_result =
      tcp_socket_->AdoptConnectedSocket(socket_fd, net::IPEndPoint());
  if (net_result != net::OK) {
    error_callback.Run("Error connecting to socket: " +
                       std::string(net::ErrorToString(net_result)));
    closesocket(socket_fd);
    return;
  }

  success_callback.Run();
}

void BluetoothSocketWin::DoDisconnect(const base::Closure& success_callback) {
  DCHECK(socket_thread_->task_runner()->RunsTasksOnCurrentThread());
  base::ThreadRestrictions::AssertIOAllowed();

  DoClose();
  success_callback.Run();
}

void BluetoothSocketWin::DoReceive(
    int buffer_size,
    const ReceiveCompletionCallback& success_callback,
    const ReceiveErrorCompletionCallback& error_callback) {
  DCHECK(socket_thread_->task_runner()->RunsTasksOnCurrentThread());
  base::ThreadRestrictions::AssertIOAllowed();

  if (!tcp_socket_) {
    error_callback.Run(BluetoothSocketWin::kDisconnected, kSocketNotConnected);
    return;
  }

  // Only one pending read at a time
  if (read_buffer_.get()) {
    error_callback.Run(BluetoothSocketWin::kIOPending,
                       net::ErrorToString(net::ERR_IO_PENDING));
    return;
  }

  scoped_refptr<net::IOBufferWithSize> buffer(
      new net::IOBufferWithSize(buffer_size));
  int read_result =
      tcp_socket_->Read(buffer.get(),
                        buffer->size(),
                        base::Bind(&BluetoothSocketWin::OnSocketReadComplete,
                                   this,
                                   success_callback,
                                   error_callback));

  if (read_result > 0) {
    success_callback.Run(read_result, buffer);
  } else if (read_result == net::OK ||
             read_result == net::ERR_CONNECTION_CLOSED) {
    error_callback.Run(BluetoothSocketWin::kDisconnected,
                       net::ErrorToString(net::ERR_CONNECTION_CLOSED));
  } else if (read_result == net::ERR_IO_PENDING) {
    read_buffer_ = buffer;
  } else {
    error_callback.Run(BluetoothSocketWin::kSystemError,
                       net::ErrorToString(read_result));
  }
}

void BluetoothSocketWin::OnSocketReadComplete(
    const ReceiveCompletionCallback& success_callback,
    const ReceiveErrorCompletionCallback& error_callback,
    int read_result) {
  DCHECK(socket_thread_->task_runner()->RunsTasksOnCurrentThread());
  base::ThreadRestrictions::AssertIOAllowed();

  scoped_refptr<net::IOBufferWithSize> buffer;
  buffer.swap(read_buffer_);
  if (read_result > 0) {
    success_callback.Run(read_result, buffer);
  } else if (read_result == net::OK ||
             read_result == net::ERR_CONNECTION_CLOSED) {
    error_callback.Run(BluetoothSocketWin::kDisconnected,
                       net::ErrorToString(net::ERR_CONNECTION_CLOSED));
  } else {
    error_callback.Run(BluetoothSocketWin::kSystemError,
                       net::ErrorToString(read_result));
  }
}

void BluetoothSocketWin::DoSend(scoped_refptr<net::IOBuffer> buffer,
                                int buffer_size,
                                const SendCompletionCallback& success_callback,
                                const ErrorCompletionCallback& error_callback) {
  DCHECK(socket_thread_->task_runner()->RunsTasksOnCurrentThread());
  base::ThreadRestrictions::AssertIOAllowed();

  if (!tcp_socket_) {
    error_callback.Run(kSocketNotConnected);
    return;
  }

  linked_ptr<WriteRequest> request(new WriteRequest());
  request->buffer = buffer;
  request->buffer_size = buffer_size;
  request->success_callback = success_callback;
  request->error_callback = error_callback;

  write_queue_.push(request);
  if (write_queue_.size() == 1) {
    SendFrontWriteRequest();
  }
}

void BluetoothSocketWin::SendFrontWriteRequest() {
  DCHECK(socket_thread_->task_runner()->RunsTasksOnCurrentThread());
  base::ThreadRestrictions::AssertIOAllowed();

  if (!tcp_socket_)
    return;

  if (write_queue_.size() == 0)
    return;

  linked_ptr<WriteRequest> request = write_queue_.front();
  net::CompletionCallback callback =
      base::Bind(&BluetoothSocketWin::OnSocketWriteComplete,
                 this,
                 request->success_callback,
                 request->error_callback);
  int send_result =
      tcp_socket_->Write(request->buffer, request->buffer_size, callback);
  if (send_result != net::ERR_IO_PENDING) {
    callback.Run(send_result);
  }
}

void BluetoothSocketWin::OnSocketWriteComplete(
    const SendCompletionCallback& success_callback,
    const ErrorCompletionCallback& error_callback,
    int send_result) {
  DCHECK(socket_thread_->task_runner()->RunsTasksOnCurrentThread());
  base::ThreadRestrictions::AssertIOAllowed();

  write_queue_.pop();

  if (send_result >= net::OK) {
    success_callback.Run(send_result);
  } else {
    error_callback.Run(net::ErrorToString(send_result));
  }

  // Don't call directly to avoid potentail large recursion.
  socket_thread_->task_runner()->PostNonNestableTask(
      FROM_HERE, base::Bind(&BluetoothSocketWin::SendFrontWriteRequest, this));
}

void BluetoothSocketWin::PostSuccess(const base::Closure& callback) {
  ui_task_runner_->PostTask(FROM_HERE, callback);
}

void BluetoothSocketWin::PostErrorCompletion(
    const ErrorCompletionCallback& callback,
    const std::string& error) {
  ui_task_runner_->PostTask(FROM_HERE, base::Bind(callback, error));
}

void BluetoothSocketWin::PostReceiveCompletion(
    const ReceiveCompletionCallback& callback,
    int io_buffer_size,
    scoped_refptr<net::IOBuffer> io_buffer) {
  ui_task_runner_->PostTask(FROM_HERE,
                            base::Bind(callback, io_buffer_size, io_buffer));
}

void BluetoothSocketWin::PostReceiveErrorCompletion(
    const ReceiveErrorCompletionCallback& callback,
    ErrorReason reason,
    const std::string& error_message) {
  ui_task_runner_->PostTask(FROM_HERE,
                            base::Bind(callback, reason, error_message));
}

void BluetoothSocketWin::PostSendCompletion(
    const SendCompletionCallback& callback,
    int bytes_written) {
  ui_task_runner_->PostTask(FROM_HERE, base::Bind(callback, bytes_written));
}

void BluetoothSocketWin::DoStartService(
    const BluetoothUUID& uuid,
    const std::string& name,
    int rfcomm_channel,
    const base::Closure& success_callback,
    const ErrorCompletionCallback& error_callback,
    const OnNewConnectionCallback& new_connection_callback) {
  DCHECK(socket_thread_->task_runner()->RunsTasksOnCurrentThread());
  DCHECK(!tcp_socket_ &&
         !service_reg_data_ &&
         on_new_connection_callback_.is_null());

  // The valid range is 0-30. 0 means BT_PORT_ANY and 1-30 are the
  // valid RFCOMM port numbers of SOCKADDR_BTH.
  if (rfcomm_channel < 0 || rfcomm_channel > 30) {
    LOG(WARNING) << "Failed to start service: "
                 << "Invalid RFCCOMM port " << rfcomm_channel
                 << ", uuid=" << uuid.value();
    PostErrorCompletion(error_callback, kInvalidRfcommPort);
    return;
  }

  SOCKET socket_fd = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
  if (socket_fd == INVALID_SOCKET) {
    LOG(WARNING) << "Failed to start service: create socket, "
                 << "winsock err=" << WSAGetLastError();
    PostErrorCompletion(error_callback, kFailedToCreateSocket);
    return;
  }

  // Note that |socket_fd| belongs to a non-TCP address family (i.e. AF_BTH),
  // TCPSocket methods that involve address could not be called. So bind()
  // is called on |socket_fd| directly.
  scoped_ptr<net::TCPSocket> scoped_socket(
      new net::TCPSocket(NULL, net::NetLog::Source()));
  scoped_socket->AdoptListenSocket(socket_fd);

  SOCKADDR_BTH sa;
  struct sockaddr* sock_addr = reinterpret_cast<struct sockaddr*>(&sa);
  int sock_addr_len = sizeof(sa);
  ZeroMemory(&sa, sock_addr_len);
  sa.addressFamily = AF_BTH;
  sa.port = rfcomm_channel ? rfcomm_channel : BT_PORT_ANY;
  if (bind(socket_fd, sock_addr, sock_addr_len) < 0) {
    LOG(WARNING) << "Failed to start service: create socket, "
                 << "winsock err=" << WSAGetLastError();
    PostErrorCompletion(error_callback, kFailedToBindSocket);
    return;
  }

  const int kListenBacklog = 5;
  if (scoped_socket->Listen(kListenBacklog) < 0) {
    LOG(WARNING) << "Failed to start service: Listen"
                 << "winsock err=" << WSAGetLastError();
    PostErrorCompletion(error_callback, kFailedToListenOnSocket);
    return;
  }

  scoped_ptr<ServiceRegData> reg_data(new ServiceRegData);
  reg_data->name = base::UTF8ToUTF16(name);

  if (getsockname(socket_fd, sock_addr, &sock_addr_len)) {
    LOG(WARNING) << "Failed to start service: getsockname, "
                 << "winsock err=" << WSAGetLastError();
    PostErrorCompletion(error_callback, kFailedToGetSockNameForSocket);
    return;
  }
  reg_data->address = sa;

  reg_data->address_info.LocalAddr.iSockaddrLength = sizeof(sa);
  reg_data->address_info.LocalAddr.lpSockaddr =
      reinterpret_cast<struct sockaddr*>(&reg_data->address);
  reg_data->address_info.iSocketType = SOCK_STREAM;
  reg_data->address_info.iProtocol = BTHPROTO_RFCOMM;

  base::string16 cannonical_uuid = L"{" + base::ASCIIToUTF16(
      uuid.canonical_value()) + L"}";
  if (!SUCCEEDED(CLSIDFromString(cannonical_uuid.c_str(), &reg_data->uuid))) {
    LOG(WARNING) << "Failed to start service: "
                 << ", bad uuid=" << cannonical_uuid;
    PostErrorCompletion(error_callback, kBadUuid);
    return;
  }

  reg_data->service.dwSize = sizeof(WSAQUERYSET);
  reg_data->service.lpszServiceInstanceName =
      const_cast<LPWSTR>(reg_data->name.c_str());
  reg_data->service.lpServiceClassId = &reg_data->uuid;
  reg_data->service.dwNameSpace = NS_BTH;
  reg_data->service.dwNumberOfCsAddrs = 1;
  reg_data->service.lpcsaBuffer = &reg_data->address_info;

  if (WSASetService(&reg_data->service,
                    RNRSERVICE_REGISTER, 0) == SOCKET_ERROR) {
    LOG(WARNING) << "Failed to register profile: WSASetService"
                 << "winsock err=" << WSAGetLastError();
    PostErrorCompletion(error_callback, kWsaSetServiceError);
    return;
  }

  tcp_socket_ = scoped_socket.Pass();
  service_reg_data_ = reg_data.Pass();
  on_new_connection_callback_ = new_connection_callback;
  DoAccept();

  PostSuccess(success_callback);
}

void BluetoothSocketWin::DoAccept() {
  DCHECK(socket_thread_->task_runner()->RunsTasksOnCurrentThread());
  int result = tcp_socket_->Accept(
      &accept_socket_,
      &accept_address_,
      base::Bind(&BluetoothSocketWin::OnAcceptOnSocketThread, this));
  if (result != net::OK && result != net::ERR_IO_PENDING)
    LOG(WARNING) << "Failed to accept, net err=" << result;
}

void BluetoothSocketWin::OnAcceptOnSocketThread(int accept_result) {
  DCHECK(socket_thread_->task_runner()->RunsTasksOnCurrentThread());
  if (accept_result != net::OK) {
    LOG(WARNING) << "OnAccept error, net err=" << accept_result;
    return;
  }

  ui_task_runner_->PostTask(
    FROM_HERE,
    base::Bind(&BluetoothSocketWin::OnAcceptOnUI,
               this,
               base::Passed(&accept_socket_),
               accept_address_));
  DoAccept();
}

void BluetoothSocketWin::OnAcceptOnUI(
    scoped_ptr<net::TCPSocket> accept_socket,
    const net::IPEndPoint& peer_address) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  scoped_refptr<BluetoothSocketWin> peer = CreateBluetoothSocket(
          ui_task_runner_,
          socket_thread_,
          net_log_,
          source_);
  peer->tcp_socket_ = accept_socket.Pass();

  on_new_connection_callback_.Run(peer, peer_address);
}

}  // namespace device
