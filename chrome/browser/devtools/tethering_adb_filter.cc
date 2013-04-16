// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/tethering_adb_filter.h"

#include <map>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/devtools/adb_client_socket.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/socket/tcp_client_socket.h"

namespace {

const int kBufferSize = 16 * 1024;

static const char kMethodAttribute[] = "method";
static const char kParamsAttribute[] = "params";
static const char kPortAttribute[] = "port";
static const char kLocationAttribute[] = "location";
static const char kConnectionIdAttribute[] = "connectionId";
static const char kTetheringAccepted[] = "Tethering.accepted";
static const char kTetheringBind[] = "Tethering.bind";
static const char kTetheringUnbind[] = "Tethering.unbind";

class SocketTunnel {
 public:
  SocketTunnel(const std::string& location)
      : location_(location) {
  }

  void Start(int result, net::StreamSocket* socket) {
    if (result < 0) {
      SelfDestruct();
      return;
    }
    remote_socket_.reset(socket);

    std::vector<std::string> tokens;
    Tokenize(location_, ":", &tokens);
    int port = 0;
    if (tokens.size() != 2 || !base::StringToInt(tokens[1], &port)) {
      SelfDestruct();
      return;
    }

    net::IPAddressNumber ip_number;
    if (!net::ParseIPLiteralToNumber(tokens[0], &ip_number)) {
      SelfDestruct();
      return;
    }

    net::AddressList address_list =
        net::AddressList::CreateFromIPAddress(ip_number, port);
    host_socket_.reset(new net::TCPClientSocket(address_list, NULL,
                                                net::NetLog::Source()));
    result = host_socket_->Connect(base::Bind(&SocketTunnel::OnConnected,
                                              base::Unretained(this)));
    if (result != net::ERR_IO_PENDING)
      OnConnected(result);
  }

  ~SocketTunnel() {
  }

 private:
  void OnConnected(int result) {
    if (result < 0) {
      SelfDestruct();
      return;
    }

    Pump(host_socket_.get(), remote_socket_.get());
    Pump(remote_socket_.get(), host_socket_.get());
  }

  void Pump(net::StreamSocket* from, net::StreamSocket* to) {
    scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(kBufferSize);

    int result = from->Read(buffer, kBufferSize,
          base::Bind(&SocketTunnel::OnRead, base::Unretained(this), from, to,
                     buffer));
    if (result != net::ERR_IO_PENDING)
      OnRead(from, to, buffer, result);
  }

  void OnRead(net::StreamSocket* from,
              net::StreamSocket* to,
              scoped_refptr<net::IOBuffer> buffer,
              int result) {
    if (result <= 0) {
      SelfDestruct();
      return;
    }
    to->Write(buffer, result, base::Bind(&SocketTunnel::OnWritten,
                                         base::Unretained(this)));
    Pump(from, to);
  }

  void OnWritten(int result) {
    if (result < 0)
      SelfDestruct();
  }

  void SelfDestruct() {
    if (host_socket_)
      host_socket_->Disconnect();
    if (remote_socket_)
      remote_socket_->Disconnect();
    delete this;
  }

  std::string location_;
  scoped_ptr<net::StreamSocket> remote_socket_;
  scoped_ptr<net::StreamSocket> host_socket_;
};

}  // namespace

TetheringAdbFilter::TetheringAdbFilter(int adb_port, const std::string& serial)
    : adb_port_(adb_port),
      serial_(serial) {
}

TetheringAdbFilter::~TetheringAdbFilter() {
}

bool TetheringAdbFilter::ProcessIncomingMessage(const std::string& message) {
  // Only parse messages that might be Tethering.accepted event.
  if (message.length() > 100 ||
      message.find(kTetheringAccepted) == std::string::npos)
    return false;

  scoped_ptr<base::Value> value(base::JSONReader::Read(message));

  DictionaryValue* dvalue;
  if (!value || !value->GetAsDictionary(&dvalue))
    return false;

  std::string method;
  if (!dvalue->GetString(kMethodAttribute, &method) ||
      method != kTetheringAccepted) {
    return false;
  }

  DictionaryValue* params = NULL;
  dvalue->GetDictionary(kParamsAttribute, &params);
  if (!params)
    return false;

  int port;
  std::string connection_id;
  if (!params->GetInteger(kPortAttribute, &port) ||
      !params->GetString(kConnectionIdAttribute, &connection_id))
    return false;

  std::map<int, std::string>::iterator it = requested_ports_.find(port);
  if (it == requested_ports_.end())
    return false;
  std::string location = it->second;

  SocketTunnel* tunnel = new SocketTunnel(location);
  AdbClientSocket::TransportQuery(
    adb_port_, serial_, connection_id,
    base::Bind(&SocketTunnel::Start, base::Unretained(tunnel)));
  return true;
}

void TetheringAdbFilter::ProcessOutgoingMessage(const std::string& message) {
  if (message.length() > 100)
    return;
  if (message.find(kTetheringBind) == std::string::npos &&
      message.find(kTetheringUnbind) == std::string::npos)
    return;

  scoped_ptr<base::Value> value(base::JSONReader::Read(message));

  DictionaryValue* dvalue;
  if (!value || !value->GetAsDictionary(&dvalue))
    return;

  std::string method;
  if (!dvalue->GetString(kMethodAttribute, &method))
    return;

  DictionaryValue* params = NULL;
  dvalue->GetDictionary(kParamsAttribute, &params);
  if (!params)
    return;

  int port;
  if (!params->GetInteger(kPortAttribute, &port))
    return;

  std::string location;
  if (!params->GetString(kLocationAttribute, &location))
    return;

  if (method == kTetheringBind)
    requested_ports_[port] = location;
  else
    requested_ports_.erase(port);
}
