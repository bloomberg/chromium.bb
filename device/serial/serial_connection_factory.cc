// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/serial_connection_factory.h"

#include "base/bind.h"
#include "base/location.h"
#include "device/serial/serial_connection.h"
#include "device/serial/serial_io_handler.h"

namespace device {
namespace {

void FillDefaultConnectionOptions(serial::ConnectionOptions* options) {
  if (!options->bitrate)
    options->bitrate = 9600;
  if (options->data_bits == serial::DATA_BITS_NONE)
    options->data_bits = serial::DATA_BITS_EIGHT;
  if (options->stop_bits == serial::STOP_BITS_NONE)
    options->stop_bits = serial::STOP_BITS_ONE;
  if (options->parity_bit == serial::PARITY_BIT_NONE)
    options->parity_bit = serial::PARITY_BIT_NO;
  if (!options->has_cts_flow_control) {
    options->has_cts_flow_control = true;
    options->cts_flow_control = false;
  }
}

}  // namespace

class SerialConnectionFactory::ConnectTask
    : public base::RefCountedThreadSafe<SerialConnectionFactory::ConnectTask> {
 public:
  ConnectTask(scoped_refptr<SerialConnectionFactory> factory,
              const std::string& path,
              serial::ConnectionOptionsPtr options,
              mojo::InterfaceRequest<serial::Connection> connection_request);
  void Run();

 private:
  friend class base::RefCountedThreadSafe<SerialConnectionFactory::ConnectTask>;
  virtual ~ConnectTask();
  void Connect();
  void OnConnected(bool success);

  scoped_refptr<SerialConnectionFactory> factory_;
  const std::string path_;
  serial::ConnectionOptionsPtr options_;
  mojo::InterfaceRequest<serial::Connection> connection_request_;
  scoped_refptr<SerialIoHandler> io_handler_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTask);
};

SerialConnectionFactory::SerialConnectionFactory(
    const IoHandlerFactory& io_handler_factory,
    scoped_refptr<base::MessageLoopProxy> connect_message_loop)
    : io_handler_factory_(io_handler_factory),
      connect_message_loop_(connect_message_loop) {
}

void SerialConnectionFactory::CreateConnection(
    const std::string& path,
    serial::ConnectionOptionsPtr options,
    mojo::InterfaceRequest<serial::Connection> connection_request) {
  scoped_refptr<ConnectTask> task(
      new ConnectTask(this, path, options.Pass(), connection_request.Pass()));
  task->Run();
}

SerialConnectionFactory::~SerialConnectionFactory() {
}

SerialConnectionFactory::ConnectTask::ConnectTask(
    scoped_refptr<SerialConnectionFactory> factory,
    const std::string& path,
    serial::ConnectionOptionsPtr options,
    mojo::InterfaceRequest<serial::Connection> connection_request)
    : factory_(factory),
      path_(path),
      options_(options.Pass()),
      connection_request_(connection_request.Pass()) {
}

void SerialConnectionFactory::ConnectTask::Run() {
  factory_->connect_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&SerialConnectionFactory::ConnectTask::Connect, this));
}

SerialConnectionFactory::ConnectTask::~ConnectTask() {
}

void SerialConnectionFactory::ConnectTask::Connect() {
  io_handler_ = factory_->io_handler_factory_.Run();
  io_handler_->Open(
      path_,
      base::Bind(&SerialConnectionFactory::ConnectTask::OnConnected, this));
}

void SerialConnectionFactory::ConnectTask::OnConnected(bool success) {
  DCHECK(io_handler_);
  if (!success)
    return;
  if (!options_)
    options_ = serial::ConnectionOptions::New();
  FillDefaultConnectionOptions(options_.get());
  if (!io_handler_->ConfigurePort(*options_))
    return;
  mojo::BindToRequest(new SerialConnection(io_handler_), &connection_request_);
}

}  // namespace device
