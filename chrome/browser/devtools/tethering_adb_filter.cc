// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/tethering_adb_filter.h"

#include <map>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/devtools/adb_client_socket.h"
#include "chrome/browser/devtools/adb_web_socket.h"
#include "chrome/browser/devtools/devtools_protocol.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/dns/host_resolver.h"
#include "net/socket/tcp_client_socket.h"

using content::BrowserThread;

namespace {

const int kBufferSize = 16 * 1024;

enum {
  kStatusError = -3,
  kStatusDisconnecting = -2,
  kStatusConnecting = -1,
  kStatusOK = 0,
  // Positive values are used to count open connections.
};

static const char kPortAttribute[] = "port";
static const char kConnectionIdAttribute[] = "connectionId";
static const char kTetheringAccepted[] = "Tethering.accepted";
static const char kTetheringBind[] = "Tethering.bind";
static const char kTetheringUnbind[] = "Tethering.unbind";

static const char kDevToolsRemoteSocketName[] = "chrome_devtools_remote";
static const char kDevToolsRemoteBrowserTarget[] = "/devtools/browser";

class SocketTunnel {
 public:
  typedef base::Callback<void(int)> CounterCallback;

  SocketTunnel(const std::string& location, const CounterCallback& callback)
      : location_(location),
        pending_writes_(0),
        pending_destruction_(false),
        callback_(callback),
        about_to_destroy_(false) {
    callback_.Run(1);
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
        net::HostPortPair(tokens[0], port), net::DEFAULT_PRIORITY);
    result = host_resolver_->Resolve(
        request_info,
        &address_list_,
        base::Bind(&SocketTunnel::OnResolved, base::Unretained(this)),
        NULL,
        net::BoundNetLog());
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
    about_to_destroy_ = true;
    if (host_socket_)
      host_socket_->Disconnect();
    if (remote_socket_)
      remote_socket_->Disconnect();
    callback_.Run(-1);
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
    // In case one of the connections closes, we could get here
    // from another one due to Disconnect firing back on all
    // read callbacks.
    if (about_to_destroy_)
      return;
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
  CounterCallback callback_;
  bool about_to_destroy_;
};

}  // namespace

TetheringAdbFilter::TetheringAdbFilter(
    scoped_refptr<DevToolsAdbBridge::AndroidDevice> device,
    base::MessageLoop* adb_message_loop,
    PrefService* pref_service,
    scoped_refptr<AdbWebSocket> web_socket)
    : device_(device),
      adb_message_loop_(adb_message_loop),
      web_socket_(web_socket),
      command_id_(0) {
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
                 this, new_forwarding_map));
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
  base::DictionaryValue params;
  params.SetInteger(kPortAttribute, port);
  DevToolsProtocol::Command command(++command_id_, method, &params);

  if (method == kTetheringBind) {
    pending_responses_[command.id()] =
        base::Bind(&TetheringAdbFilter::ProcessBindResponse,
                   base::Unretained(this), port);
#if defined(DEBUG_DEVTOOLS)
    port_status_[port] = kStatusConnecting;
    UpdatePortStatusMap();
#endif  // defined(DEBUG_DEVTOOLS)
  } else {
    DCHECK_EQ(kTetheringUnbind, method);

    PortStatusMap::iterator it = port_status_.find(port);
    if (it != port_status_.end() && it->second == kStatusError) {
      // The bind command failed on this port, do not attempt unbind.
      port_status_.erase(it);
      UpdatePortStatusMap();
      return;
    }

    pending_responses_[command.id()] =
        base::Bind(&TetheringAdbFilter::ProcessUnbindResponse,
                   base::Unretained(this), port);
#if defined(DEBUG_DEVTOOLS)
    port_status_[port] = kStatusDisconnecting;
    UpdatePortStatusMap();
#endif  // defined(DEBUG_DEVTOOLS)
  }

  web_socket_->SendFrameOnHandlerThread(command.Serialize());
}

bool TetheringAdbFilter::ProcessResponse(const std::string& message) {
  scoped_ptr<DevToolsProtocol::Response> response(
      DevToolsProtocol::ParseResponse(message));
  if (!response)
    return false;

  CommandCallbackMap::iterator it = pending_responses_.find(response->id());
  if (it == pending_responses_.end())
    return false;

  it->second.Run(response->error_code() ? kStatusError : kStatusOK);
  pending_responses_.erase(it);
  return true;
}

void TetheringAdbFilter::ProcessBindResponse(int port, PortStatus status) {
  port_status_[port] = status;
  UpdatePortStatusMap();
}

void TetheringAdbFilter::ProcessUnbindResponse(int port, PortStatus status) {
  PortStatusMap::iterator it = port_status_.find(port);
  if (it == port_status_.end())
    return;
  if (status == kStatusError)
    it->second = status;
  else
    port_status_.erase(it);
  UpdatePortStatusMap();
}

void TetheringAdbFilter::UpdateSocketCount(int port, int increment) {
#if defined(DEBUG_DEVTOOLS)
  PortStatusMap::iterator it = port_status_.find(port);
  if (it == port_status_.end())
    return;
  if (it->second < 0 || (it->second == 0 && increment < 0))
    return;
  it->second += increment;
  UpdatePortStatusMap();
#endif  // defined(DEBUG_DEVTOOLS)
}

void TetheringAdbFilter::UpdatePortStatusMap() {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
     base::Bind(&TetheringAdbFilter::UpdatePortStatusMapOnUIThread,
        this, port_status_));
}

void TetheringAdbFilter::UpdatePortStatusMapOnUIThread(
    const PortStatusMap& status_map) {
  port_status_on_ui_thread_ = status_map;
}

const TetheringAdbFilter::PortStatusMap&
TetheringAdbFilter::GetPortStatusMap() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return port_status_on_ui_thread_;
}

bool TetheringAdbFilter::ProcessIncomingMessage(const std::string& message) {
  DCHECK_EQ(base::MessageLoop::current(), adb_message_loop_);

  if (ProcessResponse(message))
    return true;

  scoped_ptr<DevToolsProtocol::Notification> notification(
      DevToolsProtocol::ParseNotification(message));
  if (!notification)
    return false;

  if (notification->method() != kTetheringAccepted)
    return false;

  DictionaryValue* params = notification->params();
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

  SocketTunnel* tunnel = new SocketTunnel(location,
      base::Bind(&TetheringAdbFilter::UpdateSocketCount, this, port));

  device_->OpenSocket(connection_id.c_str(),
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

  TetheringAdbFilter::PortStatusMap port_status();

 private:
  virtual ~Connection();

  virtual void OnSocketOpened() OVERRIDE;
  virtual void OnFrameRead(const std::string& message) OVERRIDE;
  virtual void OnSocketClosed(bool closed_by_device) OVERRIDE;
  virtual bool ProcessIncomingMessage(const std::string& message) OVERRIDE;

  Registry* registry_;
  scoped_refptr<DevToolsAdbBridge::AndroidDevice> device_;
  base::MessageLoop* adb_message_loop_;
  PrefService* pref_service_;

  scoped_refptr<TetheringAdbFilter> tethering_adb_filter_;
  scoped_refptr<AdbWebSocket> web_socket_;
};

PortForwardingController::Connection::Connection(
    Registry* registry,
    scoped_refptr<DevToolsAdbBridge::AndroidDevice> device,
    base::MessageLoop* adb_message_loop,
    PrefService* pref_service)
    : registry_(registry),
      device_(device),
      adb_message_loop_(adb_message_loop),
      pref_service_(pref_service) {
   (*registry_)[device_->serial()] = this;
   web_socket_ = new AdbWebSocket(
        device, kDevToolsRemoteSocketName, kDevToolsRemoteBrowserTarget,
        adb_message_loop_, this);
}

void PortForwardingController::Connection::Shutdown() {
  registry_ = NULL;
  // This will have no effect if the socket is not connected yet.
  web_socket_->Disconnect();
}

TetheringAdbFilter::PortStatusMap
PortForwardingController::Connection::port_status() {
  if (tethering_adb_filter_)
    return tethering_adb_filter_->GetPortStatusMap();
  else
    return TetheringAdbFilter::PortStatusMap();
}

PortForwardingController::Connection::~Connection() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (registry_) {
    DCHECK(registry_->find(device_->serial()) != registry_->end());
    registry_->erase(device_->serial());
  }
}

void PortForwardingController::Connection::OnSocketOpened() {
  if (!registry_) {
    // Socket was created after Shutdown was called. Disconnect immediately.
    web_socket_->Disconnect();
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  tethering_adb_filter_ = new TetheringAdbFilter(
      device_, adb_message_loop_, pref_service_, web_socket_);
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
    Registry::iterator rit = registry_.find((*it)->serial());
    if (rit == registry_.end()) {
      // Will delete itself when disconnected.
      new Connection(
          &registry_, (*it)->device(), adb_message_loop_, pref_service_);
    } else {
      (*it)->set_port_status((*rit).second->port_status());
    }
  }
}
