// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_socket_win.h"

#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/sys_string_conversions.h"
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

using device::BluetoothSocketWin;

std::string FormatErrorMessage(DWORD error_code) {
  TCHAR error_msg[1024];
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                0,
                error_code,
                0,
                error_msg,
                1024,
                NULL);
  return base::SysWideToUTF8(error_msg);
}

static void DeactivateSocket(
    const scoped_refptr<device::BluetoothSocketThreadWin>& socket_thread) {
  socket_thread->OnSocketDeactivate();
}

}  // namespace

namespace device {

// static
scoped_refptr<BluetoothSocketWin> BluetoothSocketWin::CreateBluetoothSocket(
    const BluetoothServiceRecord& service_record,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<BluetoothSocketThreadWin> socket_thread,
    net::NetLog* net_log,
    const net::NetLog::Source& source) {
  DCHECK(ui_task_runner->RunsTasksOnCurrentThread());
  const BluetoothServiceRecordWin* service_record_win =
      static_cast<const BluetoothServiceRecordWin*>(&service_record);

  scoped_refptr<BluetoothSocketWin> result(
      new BluetoothSocketWin(ui_task_runner, socket_thread, net_log, source));
  result->device_address_ = service_record_win->address();
  if (service_record.SupportsRfcomm()) {
    result->supports_rfcomm_ = true;
    result->rfcomm_channel_ = service_record_win->rfcomm_channel();
    result->bth_addr_ = service_record_win->bth_addr();
  }

  return result;
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

void BluetoothSocketWin::Close() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  socket_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&BluetoothSocketWin::DoClose, this));
}

void BluetoothSocketWin::Connect(
    const base::Closure& success_callback,
    const ErrorCompletionCallback& error_callback) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
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
    int count,
    const ReceiveCompletionCallback& success_callback,
    const ReceiveErrorCompletionCallback& error_callback) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  socket_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothSocketWin::DoReceive,
                 this,
                 count,
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
               << "(" << error_code << ")" << FormatErrorMessage(error_code);
    error_callback.Run("Error connecting to socket: " +
                       FormatErrorMessage(error_code));
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
    int count,
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

  scoped_refptr<net::IOBufferWithSize> buffer(new net::IOBufferWithSize(count));
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

}  // namespace device
