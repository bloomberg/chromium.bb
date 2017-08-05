// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/port_forwarding_controller.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/devtools/devtools_protocol.h"
#include "chrome/browser/devtools/devtools_protocol_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/tcp_client_socket.h"
#include "third_party/WebKit/public/public_features.h"

using content::BrowserThread;

namespace {

const int kBufferSize = 16 * 1024;

enum {
  kStatusError = -3,
  kStatusDisconnecting = -2,
  kStatusConnecting = -1,
  kStatusOK = 0,
};

namespace tethering = ::chrome::devtools::Tethering;

class SocketTunnel {
 public:
  static void StartTunnel(const std::string& host,
                          int port,
                          int result,
                          std::unique_ptr<net::StreamSocket> socket) {
    if (result == net::OK)
      new SocketTunnel(std::move(socket), host, port);
  }

 private:
  SocketTunnel(std::unique_ptr<net::StreamSocket> socket,
               const std::string& host,
               int port)
      : remote_socket_(std::move(socket)),
        pending_writes_(0),
        pending_destruction_(false) {
    host_resolver_ = net::HostResolver::CreateDefaultResolver(nullptr);
    net::HostResolver::RequestInfo request_info(net::HostPortPair(host, port));
    int result = host_resolver_->Resolve(
        request_info, net::DEFAULT_PRIORITY, &address_list_,
        base::Bind(&SocketTunnel::OnResolved, base::Unretained(this)),
        &request_, net::NetLogWithSource());
    if (result != net::ERR_IO_PENDING)
      OnResolved(result);
  }

  void OnResolved(int result) {
    if (result < 0) {
      SelfDestruct();
      return;
    }

    host_socket_.reset(new net::TCPClientSocket(address_list_, nullptr, nullptr,
                                                net::NetLogSource()));
    result = host_socket_->Connect(base::Bind(&SocketTunnel::OnConnected,
                                              base::Unretained(this)));
    if (result != net::ERR_IO_PENDING)
      OnConnected(result);
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
    if (pending_writes_ > 0) {
      pending_destruction_ = true;
      return;
    }
    delete this;
  }

  std::unique_ptr<net::StreamSocket> remote_socket_;
  std::unique_ptr<net::StreamSocket> host_socket_;
  std::unique_ptr<net::HostResolver> host_resolver_;
  std::unique_ptr<net::HostResolver::Request> request_;
  net::AddressList address_list_;
  int pending_writes_;
  bool pending_destruction_;
};

}  // namespace

class PortForwardingController::Connection
    : public AndroidDeviceManager::AndroidWebSocket::Delegate {
 public:
  Connection(Registry* registry,
             scoped_refptr<AndroidDeviceManager::Device> device,
             scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser,
             const ForwardingMap& forwarding_map);
  ~Connection() override;

  const PortStatusMap& GetPortStatusMap();

  void UpdateForwardingMap(const ForwardingMap& new_forwarding_map);

  scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser() {
    return browser_;
  }

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<Connection>;

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

  // DevToolsAndroidBridge::AndroidWebSocket::Delegate implementation:
  void OnSocketOpened() override;
  void OnFrameRead(const std::string& message) override;
  void OnSocketClosed() override;

  PortForwardingController::Registry* registry_;
  scoped_refptr<AndroidDeviceManager::Device> device_;
  scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser_;
  std::unique_ptr<AndroidDeviceManager::AndroidWebSocket> web_socket_;
  int command_id_;
  bool connected_;
  ForwardingMap forwarding_map_;
  CommandCallbackMap pending_responses_;
  PortStatusMap port_status_;

  DISALLOW_COPY_AND_ASSIGN(Connection);
};

PortForwardingController::Connection::Connection(
    Registry* registry,
    scoped_refptr<AndroidDeviceManager::Device> device,
    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser,
    const ForwardingMap& forwarding_map)
    : registry_(registry),
      device_(device),
      browser_(browser),
      command_id_(0),
      connected_(false),
      forwarding_map_(forwarding_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  (*registry_)[device_->serial()] = this;
  web_socket_.reset(device_->CreateWebSocket(
      browser->socket(), browser->browser_target_id(), this));
}

PortForwardingController::Connection::~Connection() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(registry_->find(device_->serial()) != registry_->end());
  registry_->erase(device_->serial());
}

void PortForwardingController::Connection::UpdateForwardingMap(
    const ForwardingMap& new_forwarding_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (connected_) {
    SerializeChanges(tethering::unbind::kName,
        new_forwarding_map, forwarding_map_);
    SerializeChanges(tethering::bind::kName,
        forwarding_map_, new_forwarding_map);
  }
  forwarding_map_ = new_forwarding_map;
}

void PortForwardingController::Connection::SerializeChanges(
    const std::string& method,
    const ForwardingMap& old_map,
    const ForwardingMap& new_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<base::DictionaryValue> params(new base::DictionaryValue);
  if (method == tethering::bind::kName) {
    params->SetInteger(tethering::bind::kParamPort, port);
  } else {
    DCHECK_EQ(tethering::unbind::kName, method);
    params->SetInteger(tethering::unbind::kParamPort, port);
  }
  int id = ++command_id_;

  if (method == tethering::bind::kName) {
    pending_responses_[id] =
        base::Bind(&Connection::ProcessBindResponse,
                   base::Unretained(this), port);
#if BUILDFLAG(DEBUG_DEVTOOLS)
    port_status_[port] = kStatusConnecting;
#endif  // BUILDFLAG(DEBUG_DEVTOOLS)
  } else {
    PortStatusMap::iterator it = port_status_.find(port);
    if (it != port_status_.end() && it->second == kStatusError) {
      // The bind command failed on this port, do not attempt unbind.
      port_status_.erase(it);
      return;
    }

    pending_responses_[id] =
        base::Bind(&Connection::ProcessUnbindResponse,
                   base::Unretained(this), port);
#if BUILDFLAG(DEBUG_DEVTOOLS)
    port_status_[port] = kStatusDisconnecting;
#endif  // BUILDFLAG(DEBUG_DEVTOOLS)
  }

  web_socket_->SendFrame(
      DevToolsProtocol::SerializeCommand(id, method, std::move(params)));
}

bool PortForwardingController::Connection::ProcessResponse(
    const std::string& message) {
  int id = 0;
  int error_code = 0;
  if (!DevToolsProtocol::ParseResponse(message, &id, &error_code))
    return false;

  CommandCallbackMap::iterator it = pending_responses_.find(id);
  if (it == pending_responses_.end())
    return false;

  it->second.Run(error_code ? kStatusError : kStatusOK);
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

const PortForwardingController::PortStatusMap&
PortForwardingController::Connection::GetPortStatusMap() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return port_status_;
}

void PortForwardingController::Connection::OnSocketOpened() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  connected_ = true;
  SerializeChanges(tethering::bind::kName, ForwardingMap(), forwarding_map_);
}

void PortForwardingController::Connection::OnSocketClosed() {
  delete this;
}

void PortForwardingController::Connection::OnFrameRead(
    const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (ProcessResponse(message))
    return;

  std::string method;
  std::unique_ptr<base::DictionaryValue> params;
  if (!DevToolsProtocol::ParseNotification(message, &method, &params))
    return;

  if (method != tethering::accepted::kName || !params)
    return;

  int port;
  std::string connection_id;
  if (!params->GetInteger(tethering::accepted::kParamPort, &port) ||
      !params->GetString(tethering::accepted::kParamConnectionId,
                         &connection_id))
    return;

  std::map<int, std::string>::iterator it = forwarding_map_.find(port);
  if (it == forwarding_map_.end())
    return;

  std::string location = it->second;
  std::vector<std::string> tokens = base::SplitString(
      location, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int destination_port = 0;
  if (tokens.size() != 2 || !base::StringToInt(tokens[1], &destination_port))
    return;
  std::string destination_host = tokens[0];

  device_->OpenSocket(
      connection_id.c_str(),
      base::Bind(&SocketTunnel::StartTunnel,
                 destination_host,
                 destination_port));
}

PortForwardingController::PortForwardingController(Profile* profile)
    : pref_service_(profile->GetPrefs()) {
  pref_change_registrar_.Init(pref_service_);
  base::Closure callback = base::Bind(
      &PortForwardingController::OnPrefsChange, base::Unretained(this));
  pref_change_registrar_.Add(prefs::kDevToolsPortForwardingEnabled, callback);
  pref_change_registrar_.Add(prefs::kDevToolsPortForwardingConfig, callback);
  OnPrefsChange();
}

PortForwardingController::~PortForwardingController() {}

PortForwardingController::ForwardingStatus
PortForwardingController::DeviceListChanged(
    const DevToolsAndroidBridge::CompleteDevices& complete_devices) {
  ForwardingStatus status;
  if (forwarding_map_.empty())
    return status;

  for (const auto& pair : complete_devices) {
    scoped_refptr<AndroidDeviceManager::Device> device(pair.first);
    scoped_refptr<DevToolsAndroidBridge::RemoteDevice> remote_device(
        pair.second);
    if (!remote_device->is_connected())
      continue;
    Registry::iterator rit = registry_.find(remote_device->serial());
    if (rit == registry_.end()) {
      if (remote_device->browsers().size() > 0) {
        new Connection(&registry_, device, remote_device->browsers()[0],
                       forwarding_map_);
      }
    } else {
      status.push_back(std::make_pair(rit->second->browser(),
                                      rit->second->GetPortStatusMap()));
    }
  }
  return status;
}

void PortForwardingController::CloseAllConnections() {
  Registry copy(registry_);
  for (auto& entry : copy)
    delete entry.second;
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

  if (!forwarding_map_.empty())
    UpdateConnections();
  else
    CloseAllConnections();
}

void PortForwardingController::UpdateConnections() {
  for (Registry::iterator it = registry_.begin(); it != registry_.end(); ++it)
    it->second->UpdateForwardingMap(forwarding_map_);
}
