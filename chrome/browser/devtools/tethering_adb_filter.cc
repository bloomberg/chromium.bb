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
#include "net/dns/host_resolver.h"
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
  explicit SocketTunnel(const std::string& location)
      : location_(location),
        pending_writes_(0),
        pending_destruction_(false) {
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

    host_resolver_ = net::HostResolver::CreateDefaultResolver(NULL);
    net::HostResolver::RequestInfo request_info(
        net::HostPortPair(tokens[0], port));
    result = host_resolver_->Resolve(
        request_info, &address_list_,
        base::Bind(&SocketTunnel::OnResolved, base::Unretained(this)),
        NULL, net::BoundNetLog());
    if (result != net::ERR_IO_PENDING)
      OnResolved(result);
  }

 private:
  void OnResolved(int result) {
    if (result < 0) {
      SelfDestruct();
      return;
    }

    host_socket_.reset(new net::TCPClientSocket(address_list_, NULL,
                                                net::NetLog::Source()));
    result = host_socket_->Connect(base::Bind(&SocketTunnel::OnConnected,
                                              base::Unretained(this)));
    if (result != net::ERR_IO_PENDING)
      OnConnected(result);
  }

  ~SocketTunnel() {
    if (host_socket_)
      host_socket_->Disconnect();
    if (remote_socket_)
      remote_socket_->Disconnect();
  }

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

    int total = result;
    scoped_refptr<net::DrainableIOBuffer> drainable =
        new net::DrainableIOBuffer(buffer, total);

    ++pending_writes_;
    result = to->Write(drainable, total,
                       base::Bind(&SocketTunnel::OnWritten,
                                  base::Unretained(this), drainable, from, to));
    if (result != net::ERR_IO_PENDING)
      OnWritten(drainable, from, to, result);
  }

  void OnWritten(scoped_refptr<net::DrainableIOBuffer> drainable,
                 net::StreamSocket* from,
                 net::StreamSocket* to,
                 int result) {
    --pending_writes_;
    if (result < 0) {
      SelfDestruct();
      return;
    }

    drainable->DidConsume(result);
    if (drainable->BytesRemaining() > 0) {
      ++pending_writes_;
      result = to->Write(drainable, drainable->BytesRemaining(),
                         base::Bind(&SocketTunnel::OnWritten,
                                    base::Unretained(this), drainable, from,
                                    to));
      if (result != net::ERR_IO_PENDING)
        OnWritten(drainable, from, to, result);
      return;
    }

    if (pending_destruction_) {
      SelfDestruct();
      return;
    }
    Pump(from, to);
  }

  void SelfDestruct() {
    if (pending_writes_ > 0) {
      pending_destruction_ = true;
      return;
    }
    delete this;
  }

  std::string location_;
  scoped_ptr<net::StreamSocket> remote_socket_;
  scoped_ptr<net::StreamSocket> host_socket_;
  scoped_ptr<net::HostResolver> host_resolver_;
  net::AddressList address_list_;
  int pending_writes_;
  bool pending_destruction_;
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
  if (message.length() > 200 ||
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
