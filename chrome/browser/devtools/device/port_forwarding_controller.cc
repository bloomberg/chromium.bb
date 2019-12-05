// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/port_forwarding_controller.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/tcp_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/public_buildflags.h"

using content::BrowserThread;

namespace {

const int kBufferSize = 16 * 1024;

enum {
  kStatusError = -3,
  kStatusDisconnecting = -2,
  kStatusConnecting = -1,
  kStatusOK = 0,
};

const char kErrorCodePath[] = "error.code";
const char kIdParam[] = "id";
const char kMethodParam[] = "method";
const char kParamsParam[] = "params";

const char kBindMethod[] = "Tethering.bind";
const char kUnbindMethod[] = "Tethering.unbind";
const char kAcceptedEvent[] = "Tethering.accepted";
const char kPortParam[] = "port";
const char kConnectionIdParam[] = "connectionId";

static bool ParseNotification(const std::string& json,
                              std::string* method,
                              base::Optional<base::Value>* params) {
  base::Optional<base::Value> value = base::JSONReader::Read(json);
  if (!value || !value->is_dict())
    return false;

  const std::string* method_value = value->FindStringKey(kMethodParam);
  if (!method_value)
    return false;
  *method = *method_value;

  auto extracted_param = value->ExtractKey(kParamsParam);
  if (extracted_param && extracted_param->is_dict())
    *params = std::move(extracted_param);
  return true;
}

static bool ParseResponse(const std::string& json,
                          int* command_id,
                          int* error_code) {
  base::Optional<base::Value> value = base::JSONReader::Read(json);
  if (!value || !value->is_dict())
    return false;
  base::Optional<int> command_id_opt = value->FindIntKey(kIdParam);
  if (!command_id_opt)
    return false;
  *command_id = *command_id_opt;

  base::Optional<int> error_value = value->FindIntPath(kErrorCodePath);
  if (error_value)
    *error_code = *error_value;

  return true;
}

static std::string SerializeCommand(int command_id,
                                    const std::string& method,
                                    base::Value params) {
  base::Value command(base::Value::Type::DICTIONARY);
  command.SetIntKey(kIdParam, command_id);
  command.SetStringKey(kMethodParam, method);
  command.SetKey(kParamsParam, std::move(params));

  std::string json_command;
  base::JSONWriter::Write(command, &json_command);
  return json_command;
}

net::NetworkTrafficAnnotationTag kPortForwardingControllerTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("port_forwarding_controller_socket",
                                        R"(
        semantics {
          sender: "Port Forwarding Controller"
          description:
            "For remote debugging local Android device, one might need to "
            "enable reverse tethering for forwarding local ports from the "
            "device to some ports on the host. This socket pumps the traffic "
            "between the two."
          trigger:
            "A user connects to an Android device using remote debugging and "
            "enables port forwarding on chrome://inspect."
          data: "Any data requested from the local port on Android device."
          destination: OTHER
          destination_other:
            "Data is sent to the target that user selects in chrome://inspect."
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This request cannot be disabled in settings, however it would be "
            "sent only if user enables port fowarding in chrome://inspect and "
            "USB debugging in the Android device system settings."
          policy_exception_justification:
            "Not implemented, policies defined on Android device will apply "
            "here."
        })");

using ResolveHostCallback = base::OnceCallback<void(net::AddressList)>;

// This class is created and runs on BrowserThread::UI thread.
class PortForwardingHostResolver : public network::ResolveHostClientBase {
 public:
  PortForwardingHostResolver(Profile* profile,
                             const std::string& host,
                             int port,
                             ResolveHostCallback resolve_host_callback)
      : resolve_host_callback_(std::move(resolve_host_callback)) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(!receiver_.is_bound());

    net::HostPortPair host_port_pair(host, port);
    // Use a transient NetworkIsolationKey, as there's no need to share cached
    // DNS results from this request with anything else.
    content::BrowserContext::GetDefaultStoragePartition(profile)
        ->GetNetworkContext()
        ->ResolveHost(host_port_pair,
                      net::NetworkIsolationKey::CreateTransient(), nullptr,
                      receiver_.BindNewPipeAndPassRemote());
    receiver_.set_disconnect_handler(
        base::BindOnce(&PortForwardingHostResolver::OnComplete,
                       base::Unretained(this), net::ERR_NAME_NOT_RESOLVED,
                       net::ResolveErrorInfo(net::ERR_FAILED), base::nullopt));
  }

 private:
  ~PortForwardingHostResolver() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  // network::mojom::ResolveHostClient:
  void OnComplete(
      int result,
      const net::ResolveErrorInfo& resolve_error_info,
      const base::Optional<net::AddressList>& resolved_addresses) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (result < 0) {
      std::move(resolve_host_callback_).Run(net::AddressList());
    } else {
      DCHECK(resolved_addresses && !resolved_addresses->empty());
      std::move(resolve_host_callback_).Run(resolved_addresses.value());
    }

    delete this;
  }

  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
  ResolveHostCallback resolve_host_callback_;

  DISALLOW_COPY_AND_ASSIGN(PortForwardingHostResolver);
};

static void ResolveHost(Profile* profile,
                        const std::string& host,
                        int port,
                        ResolveHostCallback resolve_host_callback) {
  new PortForwardingHostResolver(profile, host, port,
                                 std::move(resolve_host_callback));
}

// This class is created and runs on the devtools ADB thread (except for the
// OnResolveHostComplete(), which runs on the BrowserThread::UI thread since it
// is called as a callback from PortForwardingHostResolver).
class SocketTunnel {
 public:
  static void StartTunnel(Profile* profile,
                          const std::string& host,
                          int port,
                          int result,
                          std::unique_ptr<net::StreamSocket> socket) {
    if (result == net::OK)
      new SocketTunnel(profile, std::move(socket), host, port);
  }

  ~SocketTunnel() { DCHECK_CALLED_ON_VALID_THREAD(thread_checker_); }

 private:
  SocketTunnel(Profile* profile,
               std::unique_ptr<net::StreamSocket> socket,
               const std::string& host,
               int port)
      : remote_socket_(std::move(socket)),
        pending_writes_(0),
        pending_destruction_(false),
        adb_thread_runner_(base::ThreadTaskRunnerHandle::Get()) {
    ResolveHostCallback resolve_host_callback = base::BindOnce(
        &SocketTunnel::OnResolveHostComplete, base::Unretained(this));
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&ResolveHost, profile, host, port,
                                  std::move(resolve_host_callback)));
  }

  void OnResolveHostComplete(net::AddressList resolved_addresses) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (resolved_addresses.empty()) {
      adb_thread_runner_->DeleteSoon(FROM_HERE, this);
    } else {
      adb_thread_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&SocketTunnel::OnResolved, base::Unretained(this),
                         resolved_addresses));
    }
  }

  void OnResolved(net::AddressList resolved_addresses) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    host_socket_.reset(new net::TCPClientSocket(resolved_addresses, nullptr,
                                                nullptr, net::NetLogSource()));
    int result = host_socket_->Connect(
        base::Bind(&SocketTunnel::OnConnected, base::Unretained(this)));
    if (result != net::ERR_IO_PENDING)
      OnConnected(result);
  }

  void OnConnected(int result) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

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
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    scoped_refptr<net::IOBuffer> buffer =
        base::MakeRefCounted<net::IOBuffer>(kBufferSize);
    int result = from->Read(
        buffer.get(),
        kBufferSize,
        base::Bind(
            &SocketTunnel::OnRead, base::Unretained(this), from, to, buffer));
    if (result != net::ERR_IO_PENDING)
      OnRead(from, to, std::move(buffer), result);
  }

  void OnRead(net::StreamSocket* from,
              net::StreamSocket* to,
              scoped_refptr<net::IOBuffer> buffer,
              int result) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (result <= 0) {
      SelfDestruct();
      return;
    }

    int total = result;
    scoped_refptr<net::DrainableIOBuffer> drainable =
        base::MakeRefCounted<net::DrainableIOBuffer>(std::move(buffer), total);

    ++pending_writes_;
    result = to->Write(drainable.get(), total,
                       base::Bind(&SocketTunnel::OnWritten,
                                  base::Unretained(this), drainable, from, to),
                       kPortForwardingControllerTrafficAnnotation);
    if (result != net::ERR_IO_PENDING)
      OnWritten(drainable, from, to, result);
  }

  void OnWritten(scoped_refptr<net::DrainableIOBuffer> drainable,
                 net::StreamSocket* from,
                 net::StreamSocket* to,
                 int result) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    --pending_writes_;
    if (result < 0) {
      SelfDestruct();
      return;
    }

    drainable->DidConsume(result);
    if (drainable->BytesRemaining() > 0) {
      ++pending_writes_;
      result =
          to->Write(drainable.get(), drainable->BytesRemaining(),
                    base::Bind(&SocketTunnel::OnWritten, base::Unretained(this),
                               drainable, from, to),
                    kPortForwardingControllerTrafficAnnotation);
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
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (pending_writes_ > 0) {
      pending_destruction_ = true;
      return;
    }
    delete this;
  }

  std::unique_ptr<net::StreamSocket> remote_socket_;
  std::unique_ptr<net::StreamSocket> host_socket_;
  int pending_writes_;
  bool pending_destruction_;
  scoped_refptr<base::SingleThreadTaskRunner> adb_thread_runner_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(SocketTunnel);
};

}  // namespace

class PortForwardingController::Connection
    : public AndroidDeviceManager::AndroidWebSocket::Delegate {
 public:
  Connection(Profile* profile,
             Registry* registry,
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

  Profile* profile_;
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
    Profile* profile,
    Registry* registry,
    scoped_refptr<AndroidDeviceManager::Device> device,
    scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> browser,
    const ForwardingMap& forwarding_map)
    : profile_(profile),
      registry_(registry),
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
    SerializeChanges(kUnbindMethod, new_forwarding_map, forwarding_map_);
    SerializeChanges(kBindMethod, forwarding_map_, new_forwarding_map);
  }
  forwarding_map_ = new_forwarding_map;
}

void PortForwardingController::Connection::SerializeChanges(
    const std::string& method,
    const ForwardingMap& old_map,
    const ForwardingMap& new_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto new_it(new_map.begin()); new_it != new_map.end(); ++new_it) {
    int port = new_it->first;
    const std::string& location = new_it->second;
    auto old_it = old_map.find(port);
    if (old_it != old_map.end() && old_it->second == location)
      continue;  // The port points to the same location in both configs, skip.

    SendCommand(method, port);
  }
}

void PortForwardingController::Connection::SendCommand(
    const std::string& method, int port) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Value params(base::Value::Type::DICTIONARY);
  DCHECK(method == kBindMethod || kUnbindMethod == method);
  params.SetIntKey(kPortParam, port);
  int id = ++command_id_;

  if (method == kBindMethod) {
    pending_responses_[id] =
        base::Bind(&Connection::ProcessBindResponse,
                   base::Unretained(this), port);
#if BUILDFLAG(DEBUG_DEVTOOLS)
    port_status_[port] = kStatusConnecting;
#endif  // BUILDFLAG(DEBUG_DEVTOOLS)
  } else {
    auto it = port_status_.find(port);
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

  web_socket_->SendFrame(SerializeCommand(id, method, std::move(params)));
}

bool PortForwardingController::Connection::ProcessResponse(
    const std::string& message) {
  int id = 0;
  int error_code = 0;
  if (!ParseResponse(message, &id, &error_code))
    return false;

  auto it = pending_responses_.find(id);
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
  auto it = port_status_.find(port);
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
  SerializeChanges(kBindMethod, ForwardingMap(), forwarding_map_);
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
  base::Optional<base::Value> params;
  if (!ParseNotification(message, &method, &params))
    return;

  if (method != kAcceptedEvent || !params)
    return;

  base::Optional<int> port = params->FindIntKey(kPortParam);
  if (!port)
    return;
  const std::string* connection_id = params->FindStringKey(kConnectionIdParam);
  if (!connection_id)
    return;

  auto it = forwarding_map_.find(*port);
  if (it == forwarding_map_.end())
    return;

  std::string location = it->second;
  std::vector<std::string> tokens = base::SplitString(
      location, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int destination_port = 0;
  if (tokens.size() != 2 || !base::StringToInt(tokens[1], &destination_port))
    return;
  std::string destination_host = tokens[0];

  device_->OpenSocket(*connection_id,
                      base::Bind(&SocketTunnel::StartTunnel, profile_,
                                 destination_host, destination_port));
}

PortForwardingController::PortForwardingController(Profile* profile)
    : profile_(profile), pref_service_(profile->GetPrefs()) {
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
    auto rit = registry_.find(remote_device->serial());
    if (rit == registry_.end()) {
      if (!remote_device->browsers().empty()) {
        new Connection(profile_, &registry_, device,
                       remote_device->browsers()[0], forwarding_map_);
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
    const base::Value* value =
        pref_service_->GetDictionary(prefs::kDevToolsPortForwardingConfig);
    for (const auto& dict_element : value->DictItems()) {
      int port_num;
      if (base::StringToInt(dict_element.first, &port_num) &&
          dict_element.second.is_string()) {
        forwarding_map_[port_num] = dict_element.second.GetString();
      }
    }
  }

  if (!forwarding_map_.empty())
    UpdateConnections();
  else
    CloseAllConnections();
}

void PortForwardingController::UpdateConnections() {
  for (auto it = registry_.begin(); it != registry_.end(); ++it)
    it->second->UpdateForwardingMap(forwarding_map_);
}
