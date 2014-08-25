// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/port_forwarding_controller.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/devtools/devtools_protocol.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
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

static const char kDevToolsRemoteBrowserTarget[] = "/devtools/browser";
const int kMinVersionPortForwarding = 28;

class SocketTunnel : public base::NonThreadSafe {
 public:
  typedef base::Callback<void(int)> CounterCallback;

  static void StartTunnel(const std::string& host,
                          int port,
                          const CounterCallback& callback,
                          int result,
                          net::StreamSocket* socket) {
    if (result < 0)
      return;
    SocketTunnel* tunnel = new SocketTunnel(callback);
    tunnel->Start(socket, host, port);
  }

 private:
  explicit SocketTunnel(const CounterCallback& callback)
      : pending_writes_(0),
        pending_destruction_(false),
        callback_(callback),
        about_to_destroy_(false) {
    callback_.Run(1);
  }

  void Start(net::StreamSocket* socket, const std::string& host, int port) {
    remote_socket_.reset(socket);

    host_resolver_ = net::HostResolver::CreateDefaultResolver(NULL);
    net::HostResolver::RequestInfo request_info(net::HostPortPair(host, port));
    int result = host_resolver_->Resolve(
        request_info,
        net::DEFAULT_PRIORITY,
        &address_list_,
        base::Bind(&SocketTunnel::OnResolved, base::Unretained(this)),
        NULL,
        net::BoundNetLog());
    if (result != net::ERR_IO_PENDING)
      OnResolved(result);
  }

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

    ++pending_writes_; // avoid SelfDestruct in first Pump
    Pump(host_socket_.get(), remote_socket_.get());
    --pending_writes_;
    if (pending_destruction_) {
      SelfDestruct();
    } else {
      Pump(remote_socket_.get(), host_socket_.get());
    }
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

  scoped_ptr<net::StreamSocket> remote_socket_;
  scoped_ptr<net::StreamSocket> host_socket_;
  scoped_ptr<net::HostResolver> host_resolver_;
  net::AddressList address_list_;
  int pending_writes_;
  bool pending_destruction_;
  CounterCallback callback_;
  bool about_to_destroy_;
};

typedef DevToolsAndroidBridge::RemoteBrowser::ParsedVersion ParsedVersion;

static bool IsVersionLower(const ParsedVersion& left,
                           const ParsedVersion& right) {
  return std::lexicographical_compare(
    left.begin(), left.end(), right.begin(), right.end());
}

static bool IsPortForwardingSupported(const ParsedVersion& version) {
  return !version.empty() && version[0] >= kMinVersionPortForwarding;
}

static scoped_refptr<DevToolsAndroidBridge::RemoteBrowser>
FindBestBrowserForTethering(
    const DevToolsAndroidBridge::RemoteBrowsers browsers) {
  scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> best_browser;
  ParsedVersion newest_version;
  for (DevToolsAndroidBridge::RemoteBrowsers::const_iterator it =
      browsers.begin(); it != browsers.end(); ++it) {
    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser = *it;
    ParsedVersion current_version = browser->GetParsedVersion();
    if (IsPortForwardingSupported(current_version) &&
        IsVersionLower(newest_version, current_version)) {
      best_browser = browser;
      newest_version = current_version;
    }
  }
  return best_browser;
}

}  // namespace

class PortForwardingController::Connection
    : public DevToolsAndroidBridge::AndroidWebSocket::Delegate,
      public base::RefCountedThreadSafe<
          Connection,
          content::BrowserThread::DeleteOnUIThread> {
 public:
  Connection(Registry* registry,
             scoped_refptr<DevToolsAndroidBridge::RemoteDevice> device,
             scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser,
             const ForwardingMap& forwarding_map);

  const PortStatusMap& GetPortStatusMap();

  void UpdateForwardingMap(const ForwardingMap& new_forwarding_map);

  void Shutdown();

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<Connection>;

  virtual ~Connection();

  typedef std::map<int, std::string> ForwardingMap;

  typedef base::Callback<void(PortStatus)> CommandCallback;
  typedef std::map<int, CommandCallback> CommandCallbackMap;

  void SerializeChanges(const std::string& method,
                        const ForwardingMap& old_map,
                        const ForwardingMap& new_map);

  void SendCommand(const std::string& method, int port);
  bool ProcessResponse(const std::string& json);

  void ProcessBindResponse(int port, PortStatus status);
  void ProcessUnbindResponse(int port, PortStatus status);

  void UpdateSocketCountOnHandlerThread(int port, int increment);
  void UpdateSocketCount(int port, int increment);

  // DevToolsAndroidBridge::AndroidWebSocket::Delegate implementation:
  virtual void OnSocketOpened() OVERRIDE;
  virtual void OnFrameRead(const std::string& message) OVERRIDE;
  virtual void OnSocketClosed(bool closed_by_device) OVERRIDE;

  PortForwardingController::Registry* registry_;
  scoped_refptr<DevToolsAndroidBridge::RemoteDevice> device_;
  scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser_;
  scoped_refptr<DevToolsAndroidBridge::AndroidWebSocket> web_socket_;
  int command_id_;
  bool connected_;
  ForwardingMap forwarding_map_;
  CommandCallbackMap pending_responses_;
  PortStatusMap port_status_;

  DISALLOW_COPY_AND_ASSIGN(Connection);
};

PortForwardingController::Connection::Connection(
    Registry* registry,
    scoped_refptr<DevToolsAndroidBridge::RemoteDevice> device,
    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser,
    const ForwardingMap& forwarding_map)
    : registry_(registry),
      device_(device),
      browser_(browser),
      command_id_(0),
      connected_(false),
      forwarding_map_(forwarding_map) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  (*registry_)[device_->serial()] = this;
  web_socket_ = browser->CreateWebSocket(kDevToolsRemoteBrowserTarget, this);
  web_socket_->Connect();
  AddRef();  // Balanced in OnSocketClosed();
}

void PortForwardingController::Connection::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  registry_ = NULL;
  // This will have no effect if the socket is not connected yet.
  web_socket_->Disconnect();
}

PortForwardingController::Connection::~Connection() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (registry_) {
    DCHECK(registry_->find(device_->serial()) != registry_->end());
    registry_->erase(device_->serial());
  }
}

void PortForwardingController::Connection::UpdateForwardingMap(
    const ForwardingMap& new_forwarding_map) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (connected_) {
    SerializeChanges(kTetheringUnbind, new_forwarding_map, forwarding_map_);
    SerializeChanges(kTetheringBind, forwarding_map_, new_forwarding_map);
  }
  forwarding_map_ = new_forwarding_map;
}

void PortForwardingController::Connection::SerializeChanges(
    const std::string& method,
    const ForwardingMap& old_map,
    const ForwardingMap& new_map) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::DictionaryValue params;
  params.SetInteger(kPortAttribute, port);
  DevToolsProtocol::Command command(++command_id_, method, &params);

  if (method == kTetheringBind) {
    pending_responses_[command.id()] =
        base::Bind(&Connection::ProcessBindResponse,
                   base::Unretained(this), port);
#if defined(DEBUG_DEVTOOLS)
    port_status_[port] = kStatusConnecting;
#endif  // defined(DEBUG_DEVTOOLS)
  } else {
    DCHECK_EQ(kTetheringUnbind, method);

    PortStatusMap::iterator it = port_status_.find(port);
    if (it != port_status_.end() && it->second == kStatusError) {
      // The bind command failed on this port, do not attempt unbind.
      port_status_.erase(it);
      return;
    }

    pending_responses_[command.id()] =
        base::Bind(&Connection::ProcessUnbindResponse,
                   base::Unretained(this), port);
#if defined(DEBUG_DEVTOOLS)
    port_status_[port] = kStatusDisconnecting;
#endif  // defined(DEBUG_DEVTOOLS)
  }

  web_socket_->SendFrame(command.Serialize());
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
}

void PortForwardingController::Connection::UpdateSocketCountOnHandlerThread(
    int port, int increment) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
     base::Bind(&Connection::UpdateSocketCount, this, port, increment));
}

void PortForwardingController::Connection::UpdateSocketCount(
    int port, int increment) {
#if defined(DEBUG_DEVTOOLS)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PortStatusMap::iterator it = port_status_.find(port);
  if (it == port_status_.end())
    return;
  if (it->second < 0 || (it->second == 0 && increment < 0))
    return;
  it->second += increment;
#endif  // defined(DEBUG_DEVTOOLS)
}

const PortForwardingController::PortStatusMap&
PortForwardingController::Connection::GetPortStatusMap() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return port_status_;
}

void PortForwardingController::Connection::OnSocketOpened() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!registry_) {
    // Socket was created after Shutdown was called. Disconnect immediately.
    web_socket_->Disconnect();
    return;
  }
  connected_ = true;
  SerializeChanges(kTetheringBind, ForwardingMap(), forwarding_map_);
}

void PortForwardingController::Connection::OnSocketClosed(
    bool closed_by_device) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Release();  // Balanced in the constructor.
}

void PortForwardingController::Connection::OnFrameRead(
    const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (ProcessResponse(message))
    return;

  scoped_ptr<DevToolsProtocol::Notification> notification(
      DevToolsProtocol::ParseNotification(message));
  if (!notification)
    return;

  if (notification->method() != kTetheringAccepted)
    return;

  base::DictionaryValue* params = notification->params();
  if (!params)
    return;

  int port;
  std::string connection_id;
  if (!params->GetInteger(kPortAttribute, &port) ||
      !params->GetString(kConnectionIdAttribute, &connection_id))
    return;

  std::map<int, std::string>::iterator it = forwarding_map_.find(port);
  if (it == forwarding_map_.end())
    return;

  std::string location = it->second;
  std::vector<std::string> tokens;
  Tokenize(location, ":", &tokens);
  int destination_port = 0;
  if (tokens.size() != 2 || !base::StringToInt(tokens[1], &destination_port))
    return;
  std::string destination_host = tokens[0];

  SocketTunnel::CounterCallback callback =
      base::Bind(&Connection::UpdateSocketCountOnHandlerThread, this, port);

  device_->OpenSocket(
      connection_id.c_str(),
      base::Bind(&SocketTunnel::StartTunnel,
                 destination_host,
                 destination_port,
                 callback));
}

PortForwardingController::PortForwardingController(Profile* profile)
    : profile_(profile),
      pref_service_(profile->GetPrefs()),
      listening_(false) {
  pref_change_registrar_.Init(pref_service_);
  base::Closure callback = base::Bind(
      &PortForwardingController::OnPrefsChange, base::Unretained(this));
  pref_change_registrar_.Add(prefs::kDevToolsPortForwardingEnabled, callback);
  pref_change_registrar_.Add(prefs::kDevToolsPortForwardingConfig, callback);
  OnPrefsChange();
}


PortForwardingController::~PortForwardingController() {}

void PortForwardingController::Shutdown() {
  // Existing connection will not be shut down. This might be confusing for
  // some users, but the opposite is more confusing.
  StopListening();
}

void PortForwardingController::AddListener(Listener* listener) {
  listeners_.push_back(listener);
}

void PortForwardingController::RemoveListener(Listener* listener) {
  Listeners::iterator it =
      std::find(listeners_.begin(), listeners_.end(), listener);
  DCHECK(it != listeners_.end());
  listeners_.erase(it);
}

void PortForwardingController::DeviceListChanged(
    const DevToolsAndroidBridge::RemoteDevices& devices) {
  DevicesStatus status;

  for (DevToolsAndroidBridge::RemoteDevices::const_iterator it =
      devices.begin(); it != devices.end(); ++it) {
    scoped_refptr<DevToolsAndroidBridge::RemoteDevice> device = *it;
    if (!device->is_connected())
      continue;
    Registry::iterator rit = registry_.find(device->serial());
    if (rit == registry_.end()) {
      scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser =
          FindBestBrowserForTethering(device->browsers());
      if (browser) {
        new Connection(&registry_, device, browser, forwarding_map_);
      }
    } else {
      status[device->serial()] = (*rit).second->GetPortStatusMap();
    }
  }

  NotifyListeners(status);
}

void PortForwardingController::OnPrefsChange() {
  forwarding_map_.clear();

  if (pref_service_->GetBoolean(prefs::kDevToolsPortForwardingEnabled)) {
    const base::DictionaryValue* dict =
        pref_service_->GetDictionary(prefs::kDevToolsPortForwardingConfig);
    for (base::DictionaryValue::Iterator it(*dict);
         !it.IsAtEnd(); it.Advance()) {
      int port_num;
      std::string location;
      if (base::StringToInt(it.key(), &port_num) &&
          dict->GetString(it.key(), &location))
        forwarding_map_[port_num] = location;
    }
  }

  if (!forwarding_map_.empty()) {
    StartListening();
    UpdateConnections();
  } else {
    StopListening();
    ShutdownConnections();
    NotifyListeners(DevicesStatus());
  }
}

void PortForwardingController::StartListening() {
  if (listening_)
    return;
  listening_ = true;
  DevToolsAndroidBridge* android_bridge =
      DevToolsAndroidBridge::Factory::GetForProfile(profile_);
  if (android_bridge)
    android_bridge->AddDeviceListListener(this);

}

void PortForwardingController::StopListening() {
  if (!listening_)
    return;
  listening_ = false;
  DevToolsAndroidBridge* android_bridge =
      DevToolsAndroidBridge::Factory::GetForProfile(profile_);
  if (android_bridge)
    android_bridge->RemoveDeviceListListener(this);
}

void PortForwardingController::UpdateConnections() {
  for (Registry::iterator it = registry_.begin(); it != registry_.end(); ++it)
    it->second->UpdateForwardingMap(forwarding_map_);
}

void PortForwardingController::ShutdownConnections() {
  for (Registry::iterator it = registry_.begin(); it != registry_.end(); ++it)
    it->second->Shutdown();
  registry_.clear();
}

void PortForwardingController::NotifyListeners(
    const DevicesStatus& status) const {
  Listeners copy(listeners_);  // Iterate over copy.
  for (Listeners::const_iterator it = copy.begin(); it != copy.end(); ++it)
    (*it)->PortStatusChanged(status);
}

// static
PortForwardingController::Factory*
PortForwardingController::Factory::GetInstance() {
  return Singleton<PortForwardingController::Factory>::get();
}

// static
PortForwardingController* PortForwardingController::Factory::GetForProfile(
    Profile* profile) {
  return static_cast<PortForwardingController*>(GetInstance()->
          GetServiceForBrowserContext(profile, true));
}

PortForwardingController::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "PortForwardingController",
          BrowserContextDependencyManager::GetInstance()) {}

PortForwardingController::Factory::~Factory() {}

KeyedService* PortForwardingController::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new PortForwardingController(profile);
}
