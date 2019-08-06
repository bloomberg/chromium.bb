// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_bridge.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/process/process_handle.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/mojo_utils.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_messaging.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_notification_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/wilco_dtc_supportd_client.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/cros_system_api/dbus/wilco_dtc_supportd/dbus-constants.h"

namespace chromeos {

namespace {

using wilco_dtc_supportd::mojom::WilcoDtcSupportdEvent;

// Interval used between successive connection attempts to the
// wilco_dtc_supportd. This is a safety measure for avoiding busy loops when the
// wilco_dtc_supportd is dysfunctional.
constexpr base::TimeDelta kConnectionAttemptInterval =
    base::TimeDelta::FromSeconds(1);
// The maximum number of consecutive connection attempts to the
// wilco_dtc_supportd before giving up. This is to prevent wasting system
// resources on hopeless attempts to connect in cases when the
// wilco_dtc_supportd is dysfunctional.
constexpr int kMaxConnectionAttemptCount = 10;

WilcoDtcSupportdBridge* g_wilco_dtc_supportd_bridge_instance = nullptr;

// Real implementation of the WilcoDtcSupportdBridge delegate.
class WilcoDtcSupportdBridgeDelegateImpl final
    : public WilcoDtcSupportdBridge::Delegate {
 public:
  WilcoDtcSupportdBridgeDelegateImpl();
  ~WilcoDtcSupportdBridgeDelegateImpl() override;

  // Delegate overrides:
  void CreateWilcoDtcSupportdServiceFactoryMojoInvitation(
      wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactoryPtr*
          wilco_dtc_supportd_service_factory_mojo_ptr,
      base::ScopedFD* remote_endpoint_fd) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WilcoDtcSupportdBridgeDelegateImpl);
};

WilcoDtcSupportdBridgeDelegateImpl::WilcoDtcSupportdBridgeDelegateImpl() =
    default;

WilcoDtcSupportdBridgeDelegateImpl::~WilcoDtcSupportdBridgeDelegateImpl() =
    default;

void WilcoDtcSupportdBridgeDelegateImpl::
    CreateWilcoDtcSupportdServiceFactoryMojoInvitation(
        wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactoryPtr*
            wilco_dtc_supportd_service_factory_mojo_ptr,
        base::ScopedFD* remote_endpoint_fd) {
  mojo::OutgoingInvitation invitation;
  mojo::PlatformChannel channel;
  mojo::ScopedMessagePipeHandle server_pipe = invitation.AttachMessagePipe(
      diagnostics::kWilcoDtcSupportdMojoConnectionChannelToken);
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 channel.TakeLocalEndpoint());
  wilco_dtc_supportd_service_factory_mojo_ptr->Bind(
      mojo::InterfacePtrInfo<
          wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactory>(
          std::move(server_pipe), 0 /* version */));
  *remote_endpoint_fd =
      channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD();
}

}  // namespace

WilcoDtcSupportdBridge::Delegate::~Delegate() = default;

// static
WilcoDtcSupportdBridge* WilcoDtcSupportdBridge::Get() {
  return g_wilco_dtc_supportd_bridge_instance;
}

// static
base::TimeDelta
WilcoDtcSupportdBridge::connection_attempt_interval_for_testing() {
  return kConnectionAttemptInterval;
}

// static
int WilcoDtcSupportdBridge::max_connection_attempt_count_for_testing() {
  return kMaxConnectionAttemptCount;
}

WilcoDtcSupportdBridge::WilcoDtcSupportdBridge(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : WilcoDtcSupportdBridge(
          std::make_unique<WilcoDtcSupportdBridgeDelegateImpl>(),
          std::move(url_loader_factory),
          std::make_unique<WilcoDtcSupportdNotificationController>()) {}

WilcoDtcSupportdBridge::WilcoDtcSupportdBridge(
    std::unique_ptr<Delegate> delegate,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<WilcoDtcSupportdNotificationController>
        notification_controller)
    : delegate_(std::move(delegate)),
      web_request_service_(std::move(url_loader_factory)),
      notification_controller_(std::move(notification_controller)) {
  DCHECK(delegate_);
  DCHECK(notification_controller_);
  DCHECK(!g_wilco_dtc_supportd_bridge_instance);
  g_wilco_dtc_supportd_bridge_instance = this;
  WaitForDBusService();
}

WilcoDtcSupportdBridge::~WilcoDtcSupportdBridge() {
  DCHECK_EQ(g_wilco_dtc_supportd_bridge_instance, this);
  g_wilco_dtc_supportd_bridge_instance = nullptr;
}

void WilcoDtcSupportdBridge::SetConfigurationData(const std::string* data) {
  configuration_data_ = data;
}

const std::string& WilcoDtcSupportdBridge::GetConfigurationDataForTesting() {
  return configuration_data_ ? *configuration_data_ : base::EmptyString();
}

void WilcoDtcSupportdBridge::WaitForDBusService() {
  if (connection_attempt_ >= kMaxConnectionAttemptCount) {
    DLOG(WARNING)
        << "Stopping attempts to connect to wilco_dtc_supportd - too many "
           "unsuccessful attempts in a row";
    return;
  }
  ++connection_attempt_;

  // Cancel any tasks previously created from WaitForDBusService() or
  // ScheduleWaitingForDBusService().
  dbus_waiting_weak_ptr_factory_.InvalidateWeakPtrs();

  DBusThreadManager::Get()
      ->GetWilcoDtcSupportdClient()
      ->WaitForServiceToBeAvailable(
          base::BindOnce(&WilcoDtcSupportdBridge::OnWaitedForDBusService,
                         dbus_waiting_weak_ptr_factory_.GetWeakPtr()));
}

void WilcoDtcSupportdBridge::ScheduleWaitingForDBusService() {
  // Cancel any tasks previously created from WaitForDBusService() or
  // ScheduleWaitingForDBusService().
  dbus_waiting_weak_ptr_factory_.InvalidateWeakPtrs();

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WilcoDtcSupportdBridge::WaitForDBusService,
                     dbus_waiting_weak_ptr_factory_.GetWeakPtr()),
      kConnectionAttemptInterval);
}

void WilcoDtcSupportdBridge::OnWaitedForDBusService(bool service_is_available) {
  if (!service_is_available) {
    DLOG(WARNING) << "The wilco_dtc_supportd D-Bus service is unavailable";
    return;
  }

  // Cancel any tasks previously created from WaitForDBusService() or
  // ScheduleWaitingForDBusService().
  dbus_waiting_weak_ptr_factory_.InvalidateWeakPtrs();

  BootstrapMojoConnection();
}

void WilcoDtcSupportdBridge::BootstrapMojoConnection() {
  DCHECK(!wilco_dtc_supportd_service_factory_mojo_ptr_);

  // Create a Mojo message pipe and attach
  // |wilco_dtc_supportd_service_factory_mojo_ptr_| to its local endpoint.
  base::ScopedFD remote_endpoint_fd;
  delegate_->CreateWilcoDtcSupportdServiceFactoryMojoInvitation(
      &wilco_dtc_supportd_service_factory_mojo_ptr_, &remote_endpoint_fd);
  DCHECK(wilco_dtc_supportd_service_factory_mojo_ptr_);
  DCHECK(remote_endpoint_fd.is_valid());
  wilco_dtc_supportd_service_factory_mojo_ptr_.set_connection_error_handler(
      base::BindOnce(&WilcoDtcSupportdBridge::OnMojoConnectionError,
                     weak_ptr_factory_.GetWeakPtr()));

  // Queue a call that would establish full-duplex Mojo communication with the
  // wilco_dtc_supportd daemon by sending an interface pointer to the self
  // instance.
  mojo_self_binding_.Close();
  wilco_dtc_supportd::mojom::WilcoDtcSupportdClientPtr self_proxy;
  mojo_self_binding_.Bind(mojo::MakeRequest(&self_proxy));
  wilco_dtc_supportd_service_factory_mojo_ptr_->GetService(
      mojo::MakeRequest(&wilco_dtc_supportd_service_mojo_ptr_),
      std::move(self_proxy),
      base::BindOnce(&WilcoDtcSupportdBridge::OnMojoGetServiceCompleted,
                     weak_ptr_factory_.GetWeakPtr()));

  // Send the file descriptor with the Mojo message pipe's remote endpoint to
  // the wilco_dtc_supportd daemon via the D-Bus.
  DBusThreadManager::Get()
      ->GetWilcoDtcSupportdClient()
      ->BootstrapMojoConnection(
          std::move(remote_endpoint_fd),
          base::BindOnce(&WilcoDtcSupportdBridge::OnBootstrappedMojoConnection,
                         weak_ptr_factory_.GetWeakPtr()));
}

void WilcoDtcSupportdBridge::OnBootstrappedMojoConnection(bool success) {
  if (success)
    return;
  DLOG(ERROR) << "Failed to establish Mojo connection to wilco_dtc_supportd";
  wilco_dtc_supportd_service_factory_mojo_ptr_.reset();
  wilco_dtc_supportd_service_mojo_ptr_.reset();
  ScheduleWaitingForDBusService();
}

void WilcoDtcSupportdBridge::OnMojoGetServiceCompleted() {
  DCHECK(wilco_dtc_supportd_service_mojo_ptr_);
  DVLOG(0) << "Established Mojo communication with wilco_dtc_supportd";
  // Reset the current connection attempt counter, since a successful
  // initialization of Mojo communication has completed.
  connection_attempt_ = 0;
}

void WilcoDtcSupportdBridge::OnMojoConnectionError() {
  DLOG(WARNING)
      << "Mojo connection to the wilco_dtc_supportd daemon got shut down";
  wilco_dtc_supportd_service_factory_mojo_ptr_.reset();
  wilco_dtc_supportd_service_mojo_ptr_.reset();
  ScheduleWaitingForDBusService();
}

void WilcoDtcSupportdBridge::PerformWebRequest(
    wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod http_method,
    mojo::ScopedHandle url,
    std::vector<mojo::ScopedHandle> headers,
    mojo::ScopedHandle request_body,
    PerformWebRequestCallback callback) {
  // Extract a GURL value from a ScopedHandle.
  GURL gurl;
  if (url.is_valid()) {
    std::unique_ptr<base::SharedMemory> shared_memory;
    gurl = GURL(GetStringPieceFromMojoHandle(std::move(url), &shared_memory));
    if (!shared_memory) {
      LOG(ERROR) << "Failed to read data from mojo handle";
      std::move(callback).Run(
          wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::
              kNetworkError,
          0 /* http_status */, mojo::ScopedHandle() /* response_body */);
      return;
    }
  }

  // Extract headers from ScopedHandle's.
  std::vector<base::StringPiece> header_contents;
  std::vector<std::unique_ptr<base::SharedMemory>> shared_memories;
  for (auto& header : headers) {
    if (!header.is_valid()) {
      header_contents.push_back("");
      continue;
    }
    shared_memories.push_back(nullptr);
    header_contents.push_back(GetStringPieceFromMojoHandle(
        std::move(header), &shared_memories.back()));
    if (!shared_memories.back()) {
      LOG(ERROR) << "Failed to read data from mojo handle";
      std::move(callback).Run(
          wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::
              kNetworkError,
          0 /* http_status */, mojo::ScopedHandle() /* response_body */);
      return;
    }
  }

  // Extract a string value from a ScopedHandle.
  std::string request_body_content;
  if (request_body.is_valid()) {
    std::unique_ptr<base::SharedMemory> shared_memory;
    request_body_content = std::string(
        GetStringPieceFromMojoHandle(std::move(request_body), &shared_memory));
    if (!shared_memory) {
      LOG(ERROR) << "Failed to read data from mojo handle";
      std::move(callback).Run(
          wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus::
              kNetworkError,
          0 /* http_status */, mojo::ScopedHandle() /* response_body */);
      return;
    }
  }

  web_request_service_.PerformRequest(
      http_method, std::move(gurl), std::move(header_contents),
      std::move(request_body_content), std::move(callback));
}

void WilcoDtcSupportdBridge::GetConfigurationData(
    GetConfigurationDataCallback callback) {
  std::move(callback).Run(configuration_data_ ? *configuration_data_
                                              : std::string());
}

void WilcoDtcSupportdBridge::HandleEvent(WilcoDtcSupportdEvent event) {
  switch (event) {
    case WilcoDtcSupportdEvent::kBatteryAuth:
      notification_controller_->ShowBatteryAuthNotification();
      return;
    case WilcoDtcSupportdEvent::kNonWilcoCharger:
      notification_controller_->ShowNonWilcoChargerNotification();
      return;
    case WilcoDtcSupportdEvent::kIncompatibleDock:
      notification_controller_->ShowIncompatibleDockNotification();
      return;
    case WilcoDtcSupportdEvent::kDockError:
      notification_controller_->ShowDockErrorNotification();
      return;
    case WilcoDtcSupportdEvent::kDockDisplay:
      notification_controller_->ShowDockDisplayNotification();
      return;
    case WilcoDtcSupportdEvent::kDockThunderbolt:
      notification_controller_->ShowDockThunderboltNotification();
      return;
  }
  LOG(ERROR) << "Unrecognized event " << event << " event";
}

void WilcoDtcSupportdBridge::SendWilcoDtcMessageToUi(
    mojo::ScopedHandle json_message,
    SendWilcoDtcMessageToUiCallback callback) {
  // Extract the string value of the received message.
  DCHECK(json_message);
  std::unique_ptr<base::SharedMemory> json_message_shared_memory;
  base::StringPiece json_message_string = GetStringPieceFromMojoHandle(
      std::move(json_message), &json_message_shared_memory);
  if (json_message_string.empty()) {
    LOG(ERROR) << "Failed to read data from mojo handle";
    std::move(callback).Run(mojo::ScopedHandle() /* response_json_message */);
    return;
  }

  DeliverWilcoDtcSupportdUiMessageToExtensions(
      json_message_string.as_string(),
      base::BindOnce(
          [](SendWilcoDtcMessageToUiCallback callback,
             const std::string& response) {
            mojo::ScopedHandle response_mojo_handle;
            if (!response.empty()) {
              response_mojo_handle =
                  CreateReadOnlySharedMemoryMojoHandle(response);
              if (!response_mojo_handle)
                LOG(ERROR) << "Failed to create mojo handle for string";
            }
            std::move(callback).Run(std::move(response_mojo_handle));
          },
          std::move(callback)));
}

}  // namespace chromeos
