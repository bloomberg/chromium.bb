// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/port_forwarding_controller.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_change_registrar.h"
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

static const char kChromeProductName[] = "Chrome";
static const char kDevToolsRemoteBrowserTarget[] = "/devtools/browser";
const int kMinVersionPortForwarding = 28;

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
        net::HostPortPair(tokens[0], port));
    result = host_resolver_->Resolve(
        request_info,
        net::DEFAULT_PRIORITY,
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

typedef std::vector<int> ParsedVersion;

static ParsedVersion ParseVersion(const std::string& version) {
  ParsedVersion result;
  std::vector<std::string> parts;
  Tokenize(version, ".", &parts);
  for (size_t i = 0; i != parts.size(); ++i) {
    int value = 0;
    base::StringToInt(parts[i], &value);
    result.push_back(value);
  }
  return result;
}

static bool IsVersionLower(const ParsedVersion& left,
                           const ParsedVersion& right) {
  return std::lexicographical_compare(
    left.begin(), left.end(), right.begin(), right.end());
}

static bool IsPortForwardingSupported(const ParsedVersion& version) {
  return !version.empty() && version[0] >= kMinVersionPortForwarding;
}

static std::string FindBestSocketForTethering(
    const DevToolsAdbBridge::RemoteBrowsers browsers) {
  std::string socket;
  ParsedVersion newest_version;
  for (DevToolsAdbBridge::RemoteBrowsers::const_iterator it = browsers.begin();
       it != browsers.end(); ++it) {
    scoped_refptr<DevToolsAdbBridge::RemoteBrowser> browser = *it;
    ParsedVersion current_version = ParseVersion(browser->version());
    if (browser->product() == kChromeProductName &&
        IsPortForwardingSupported(current_version) &&
        IsVersionLower(newest_version, current_version)) {
      socket = browser->socket();
      newest_version = current_version;
    }
  }
  return socket;
}

}  // namespace

class PortForwardingController::Connection
    : public AdbWebSocket::Delegate,
      public base::RefCountedThreadSafe<
          Connection,
          content::BrowserThread::DeleteOnUIThread> {
 public:
  typedef DevToolsAdbBridge::RemoteDevice::PortStatus PortStatus;
  typedef DevToolsAdbBridge::RemoteDevice::PortStatusMap PortStatusMap;

  Connection(Registry* registry,
             scoped_refptr<DevToolsAdbBridge::AndroidDevice> device,
             const std::string& socket,
             scoped_refptr<DevToolsAdbBridge> bridge,
             PrefService* pref_service);

  const PortStatusMap& GetPortStatusMap();

  void Shutdown();

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<Connection>;

  virtual ~Connection();

  typedef std::map<int, std::string> ForwardingMap;

  typedef base::Callback<void(PortStatus)> CommandCallback;
  typedef std::map<int, CommandCallback> CommandCallbackMap;

  void OnPrefsChange();

  void ChangeForwardingMap(ForwardingMap map);

  void SerializeChanges(const std::string& method,
                        const ForwardingMap& old_map,
                        const ForwardingMap& new_map);

  void SendCommand(const std::string& method, int port);
  bool ProcessResponse(const std::string& json);

  void ProcessBindResponse(int port, PortStatus status);
  void ProcessUnbindResponse(int port, PortStatus status);
  void UpdateSocketCount(int port, int increment);
  void UpdatePortStatusMap();
  void UpdatePortStatusMapOnUIThread(const PortStatusMap& status_map);

  // AdbWebSocket::Delegate implementation:
  virtual void OnSocketOpened() OVERRIDE;
  virtual void OnFrameRead(const std::string& message) OVERRIDE;
  virtual void OnSocketClosed(bool closed_by_device) OVERRIDE;
  virtual bool ProcessIncomingMessage(const std::string& message) OVERRIDE;

  PortForwardingController::Registry* registry_;
  scoped_refptr<DevToolsAdbBridge::AndroidDevice> device_;
  scoped_refptr<DevToolsAdbBridge> bridge_;
  PrefChangeRegistrar pref_change_registrar_;
  scoped_refptr<AdbWebSocket> web_socket_;
  int command_id_;
  ForwardingMap forwarding_map_;
  CommandCallbackMap pending_responses_;
  PortStatusMap port_status_;
  PortStatusMap port_status_on_ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(Connection);
};

PortForwardingController::Connection::Connection(
    Registry* registry,
    scoped_refptr<DevToolsAdbBridge::AndroidDevice> device,
    const std::string& socket,
    scoped_refptr<DevToolsAdbBridge> bridge,
    PrefService* pref_service)
    : registry_(registry),
      device_(device),
      bridge_(bridge),
      command_id_(0) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  pref_change_registrar_.Init(pref_service);
  (*registry_)[device_->serial()] = this;
  web_socket_ = new AdbWebSocket(
      device, socket, kDevToolsRemoteBrowserTarget,
      bridge_->GetAdbMessageLoop(), this);
  AddRef();  // Balanced in OnSocketClosed();
}

void PortForwardingController::Connection::Shutdown() {
  registry_ = NULL;
  // This will have no effect if the socket is not connected yet.
  web_socket_->Disconnect();
}

PortForwardingController::Connection::~Connection() {
  if (registry_) {
    DCHECK(registry_->find(device_->serial()) != registry_->end());
    registry_->erase(device_->serial());
  }
}

void PortForwardingController::Connection::OnPrefsChange() {
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

  bridge_->GetAdbMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(&Connection::ChangeForwardingMap,
                 this, new_forwarding_map));
}

void PortForwardingController::Connection::ChangeForwardingMap(
    ForwardingMap new_forwarding_map) {
  DCHECK_EQ(base::MessageLoop::current(), bridge_->GetAdbMessageLoop());

  SerializeChanges(kTetheringUnbind, new_forwarding_map, forwarding_map_);
  SerializeChanges(kTetheringBind, forwarding_map_, new_forwarding_map);
  forwarding_map_ = new_forwarding_map;
}

void PortForwardingController::Connection::SerializeChanges(
    const std::string& method,
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

void PortForwardingController::Connection::SendCommand(
    const std::string& method, int port) {
  base::DictionaryValue params;
  params.SetInteger(kPortAttribute, port);
  DevToolsProtocol::Command command(++command_id_, method, &params);

  if (method == kTetheringBind) {
    pending_responses_[command.id()] =
        base::Bind(&Connection::ProcessBindResponse,
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
        base::Bind(&Connection::ProcessUnbindResponse,
                   base::Unretained(this), port);
#if defined(DEBUG_DEVTOOLS)
    port_status_[port] = kStatusDisconnecting;
    UpdatePortStatusMap();
#endif  // defined(DEBUG_DEVTOOLS)
  }

  web_socket_->SendFrameOnHandlerThread(command.Serialize());
}

bool PortForwardingController::Connection::ProcessResponse(
    const std::string& message) {
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

void PortForwardingController::Connection::ProcessBindResponse(
    int port, PortStatus status) {
  port_status_[port] = status;
  UpdatePortStatusMap();
}

void PortForwardingController::Connection::ProcessUnbindResponse(
    int port, PortStatus status) {
  PortStatusMap::iterator it = port_status_.find(port);
  if (it == port_status_.end())
    return;
  if (status == kStatusError)
    it->second = status;
  else
    port_status_.erase(it);
  UpdatePortStatusMap();
}

void PortForwardingController::Connection::UpdateSocketCount(
    int port, int increment) {
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

void PortForwardingController::Connection::UpdatePortStatusMap() {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
     base::Bind(&Connection::UpdatePortStatusMapOnUIThread,
        this, port_status_));
}

void PortForwardingController::Connection::UpdatePortStatusMapOnUIThread(
    const PortStatusMap& status_map) {
  port_status_on_ui_thread_ = status_map;
}

const PortForwardingController::Connection::PortStatusMap&
PortForwardingController::Connection::GetPortStatusMap() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return port_status_on_ui_thread_;
}

void PortForwardingController::Connection::OnSocketOpened() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!registry_) {
    // Socket was created after Shutdown was called. Disconnect immediately.
    web_socket_->Disconnect();
    return;
  }
  OnPrefsChange();
  base::Closure pref_callback = base::Bind(
      &Connection::OnPrefsChange, base::Unretained(this));
  pref_change_registrar_.Add(
      prefs::kDevToolsPortForwardingEnabled, pref_callback);
  pref_change_registrar_.Add(
      prefs::kDevToolsPortForwardingConfig, pref_callback);
}

void PortForwardingController::Connection::OnFrameRead(
    const std::string& message) {
}

void PortForwardingController::Connection::OnSocketClosed(
    bool closed_by_device) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Release();  // Balanced in the constructor.
}

bool PortForwardingController::Connection::ProcessIncomingMessage(
    const std::string& message) {
  DCHECK_EQ(base::MessageLoop::current(), bridge_->GetAdbMessageLoop());
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
      base::Bind(&Connection::UpdateSocketCount, this, port));

  device_->OpenSocket(connection_id.c_str(),
      base::Bind(&SocketTunnel::Start, base::Unretained(tunnel)));
  return true;
}

PortForwardingController::PortForwardingController(
    scoped_refptr<DevToolsAdbBridge> bridge,
    PrefService* pref_service)
    : bridge_(bridge),
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
      std::string socket = FindBestSocketForTethering((*it)->browsers());
      if (!socket.empty() || (*it)->serial().empty()) {
        // Will delete itself when disconnected.
        new Connection(
          &registry_, (*it)->device(), socket, bridge_, pref_service_);
      }
    } else {
      (*it)->set_port_status((*rit).second->GetPortStatusMap());
    }
  }
}
