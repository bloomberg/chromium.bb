// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/tethering_adb_filter.h"

#include <map>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/devtools/adb_client_socket.h"
#include "chrome/browser/devtools/adb_web_socket.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/dns/host_resolver.h"
#include "net/socket/tcp_client_socket.h"

using content::BrowserThread;

namespace {

const int kAdbPort = 5037;
const int kBufferSize = 16 * 1024;

static const char kIdAttribute[] = "id";
static const char kMethodAttribute[] = "method";
static const char kParamsAttribute[] = "params";
static const char kPortAttribute[] = "port";
static const char kConnectionIdAttribute[] = "connectionId";
static const char kTetheringAccepted[] = "Tethering.accepted";
static const char kTetheringBind[] = "Tethering.bind";
static const char kTetheringUnbind[] = "Tethering.unbind";
static const char kLocalAbstractCommand[] = "localabstract:%s";

static const char kDevToolsRemoteSocketName[] = "chrome_devtools_remote";
static const char kDevToolsRemoteBrowserTarget[] = "/devtools/browser";

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
    int result = from->Read(
        buffer.get(),
        kBufferSize,
        base::Bind(
            &SocketTunnel::OnRead, base::Unretained(this), from, to, buffer));
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
        new net::DrainableIOBuffer(buffer.get(), total);

    ++pending_writes_;
    result = to->Write(drainable.get(),
                       total,
                       base::Bind(&SocketTunnel::OnWritten,
                                  base::Unretained(this),
                                  drainable,
                                  from,
                                  to));
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
      result = to->Write(drainable.get(),
                         drainable->BytesRemaining(),
                         base::Bind(&SocketTunnel::OnWritten,
                                    base::Unretained(this),
                                    drainable,
                                    from,
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

TetheringAdbFilter::TetheringAdbFilter(int adb_port,
                                       const std::string& serial,
                                       base::MessageLoop* adb_message_loop,
                                       PrefService* pref_service,
                                       scoped_refptr<AdbWebSocket> web_socket)
    : adb_port_(adb_port),
      serial_(serial),
      adb_message_loop_(adb_message_loop),
      web_socket_(web_socket),
      command_id_(0),
      weak_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  pref_change_registrar_.Init(pref_service);

  OnPrefsChange();

  base::Closure pref_callback = base::Bind(
      &TetheringAdbFilter::OnPrefsChange, base::Unretained(this));
  pref_change_registrar_.Add(
      prefs::kDevToolsPortForwardingEnabled, pref_callback);
  pref_change_registrar_.Add(
      prefs::kDevToolsPortForwardingConfig, pref_callback);
}

TetheringAdbFilter::~TetheringAdbFilter() {
}

void TetheringAdbFilter::OnPrefsChange() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ForwardingMap new_forwarding_map;

  PrefService* pref_service = pref_change_registrar_.prefs();
  bool enabled =
      pref_service->GetBoolean(prefs::kDevToolsPortForwardingEnabled);
  if (enabled) {
    const DictionaryValue* dict =
        pref_service->GetDictionary(prefs::kDevToolsPortForwardingConfig);
    for (DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
      int port_num;
      std::string location;
      if (base::StringToInt(it.key(), &port_num) &&
          dict->GetString(it.key(), &location))
        new_forwarding_map[port_num] = location;
    }
  }

  adb_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&TetheringAdbFilter::ChangeForwardingMap,
                 weak_factory_.GetWeakPtr(),
                 new_forwarding_map));
}

void TetheringAdbFilter::ChangeForwardingMap(ForwardingMap new_forwarding_map) {
  DCHECK_EQ(base::MessageLoop::current(), adb_message_loop_);

  SerializeChanges(kTetheringUnbind, new_forwarding_map, forwarding_map_);
  SerializeChanges(kTetheringBind, forwarding_map_, new_forwarding_map);
  forwarding_map_ = new_forwarding_map;
}

void TetheringAdbFilter::SerializeChanges(const std::string& method,
                                          const ForwardingMap& old_map,
                                          const ForwardingMap& new_map) {
  for (ForwardingMap::const_iterator new_it(new_map.begin());
      new_it != new_map.end(); ++new_it) {
    int port = new_it->first;
    const std::string& location = new_it->second;
    ForwardingMap::const_iterator old_it = old_map.find(port);
    if (old_it != old_map.end() && old_it->second == location)
      continue;  // The port points to the same location in both configs, skip.

    SendCommand(method, port);
  }
}

void TetheringAdbFilter::SendCommand(const std::string& method, int port) {
  base::DictionaryValue command;
  command.SetInteger(kIdAttribute, ++command_id_);
  command.SetString(kMethodAttribute, method);

  base::DictionaryValue params;
  params.SetInteger(kPortAttribute, port);
  command.Set(kParamsAttribute, params.DeepCopy());

  std::string json_command;
  base::JSONWriter::Write(&command, &json_command);
  web_socket_->SendFrameOnHandlerThread(json_command);
}

bool TetheringAdbFilter::ProcessIncomingMessage(const std::string& message) {
  DCHECK_EQ(base::MessageLoop::current(), adb_message_loop_);

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

  std::map<int, std::string>::iterator it = forwarding_map_.find(port);
  if (it == forwarding_map_.end())
    return false;

  std::string location = it->second;

  SocketTunnel* tunnel = new SocketTunnel(location);
  std::string command = base::StringPrintf(kLocalAbstractCommand,
                                           connection_id.c_str());
  AdbClientSocket::TransportQuery(
      adb_port_, serial_, command,
      base::Bind(&SocketTunnel::Start, base::Unretained(tunnel)));
  return true;
}

class PortForwardingController::Connection : public AdbWebSocket::Delegate {
 public:
  Connection(
      Registry* registry,
      scoped_refptr<DevToolsAdbBridge::AndroidDevice> device,
      base::MessageLoop* adb_message_loop,
      PrefService* pref_service);

  void Shutdown();

 private:
  virtual ~Connection();

  virtual void OnSocketOpened() OVERRIDE;
  virtual void OnFrameRead(const std::string& message) OVERRIDE;
  virtual void OnSocketClosed(bool closed_by_device) OVERRIDE;
  virtual bool ProcessIncomingMessage(const std::string& message) OVERRIDE;

  Registry* registry_;
  const std::string serial_;
  base::MessageLoop* adb_message_loop_;
  PrefService* pref_service_;

  scoped_ptr<TetheringAdbFilter> tethering_adb_filter_;
  scoped_refptr<AdbWebSocket> web_socket_;
};

PortForwardingController::Connection::Connection(
    Registry* registry,
    scoped_refptr<DevToolsAdbBridge::AndroidDevice> device,
    base::MessageLoop* adb_message_loop,
    PrefService* pref_service)
    : registry_(registry),
      serial_(device->serial()),
      adb_message_loop_(adb_message_loop),
      pref_service_(pref_service) {
   (*registry_)[serial_] = this;
   web_socket_ = new AdbWebSocket(
        device, kDevToolsRemoteSocketName, kDevToolsRemoteBrowserTarget,
        adb_message_loop_, this);
}

void PortForwardingController::Connection::Shutdown() {
  registry_ = NULL;
  // This will have no effect if the socket is not connected yet.
  web_socket_->Disconnect();
}

PortForwardingController::Connection::~Connection() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (registry_) {
    DCHECK(registry_->find(serial_) != registry_->end());
    registry_->erase(serial_);
  }
}

void PortForwardingController::Connection::OnSocketOpened() {
  if (!registry_) {
    // Socket was created after Shutdown was called. Disconnect immediately.
    web_socket_->Disconnect();
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  tethering_adb_filter_.reset(new TetheringAdbFilter(
      kAdbPort, serial_, adb_message_loop_, pref_service_, web_socket_));
}

void PortForwardingController::Connection::OnFrameRead(
    const std::string& message) {
}

void PortForwardingController::Connection::OnSocketClosed(
    bool closed_by_device) {
  delete this;
}

bool PortForwardingController::Connection::ProcessIncomingMessage(
    const std::string& message) {
  return tethering_adb_filter_->ProcessIncomingMessage(message);
}


PortForwardingController::PortForwardingController(
    base::MessageLoop* adb_message_loop,
    PrefService* pref_service)
    : adb_message_loop_(adb_message_loop),
      pref_service_(pref_service) {
}

PortForwardingController::~PortForwardingController() {
  for (Registry::iterator it = registry_.begin(); it != registry_.end(); ++it)
    it->second->Shutdown();
}

void PortForwardingController::UpdateDeviceList(
    const DevToolsAdbBridge::RemoteDevices& devices) {
  for (DevToolsAdbBridge::RemoteDevices::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    if (registry_.find((*it)->serial()) == registry_.end()) {
      // Will delete itself when disconnected.
      new Connection(
          &registry_, (*it)->device(), adb_message_loop_, pref_service_);
    }
  }
}
