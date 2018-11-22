// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/diagnosticsd/diagnosticsd_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/diagnosticsd_client.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/cros_system_api/dbus/diagnosticsd/dbus-constants.h"

namespace chromeos {

namespace {

// Interval used between successive connection attempts to the diagnosticsd.
// This is a safety measure for avoiding busy loops when the diagnosticsd is
// dysfunctional.
constexpr base::TimeDelta kConnectionAttemptInterval =
    base::TimeDelta::FromSeconds(1);
// The maximum number of consecutive connection attempts to the diagnosticsd
// before giving up. This is to prevent wasting system resources on hopeless
// attempts to connect in cases when the diagnosticsd is dysfunctional.
constexpr int kMaxConnectionAttemptCount = 10;

DiagnosticsdBridge* g_diagnosticsd_bridge_instance = nullptr;

// Real implementation of the DiagnosticsdBridge delegate.
class DiagnosticsdBridgeDelegateImpl final
    : public DiagnosticsdBridge::Delegate {
 public:
  DiagnosticsdBridgeDelegateImpl();
  ~DiagnosticsdBridgeDelegateImpl() override;

  // Delegate overrides:
  void CreateDiagnosticsdServiceFactoryMojoInvitation(
      diagnosticsd::mojom::DiagnosticsdServiceFactoryPtr*
          diagnosticsd_service_factory_mojo_ptr,
      base::ScopedFD* remote_endpoint_fd) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdBridgeDelegateImpl);
};

DiagnosticsdBridgeDelegateImpl::DiagnosticsdBridgeDelegateImpl() = default;

DiagnosticsdBridgeDelegateImpl::~DiagnosticsdBridgeDelegateImpl() = default;

void DiagnosticsdBridgeDelegateImpl::
    CreateDiagnosticsdServiceFactoryMojoInvitation(
        diagnosticsd::mojom::DiagnosticsdServiceFactoryPtr*
            diagnosticsd_service_factory_mojo_ptr,
        base::ScopedFD* remote_endpoint_fd) {
  mojo::OutgoingInvitation invitation;
  mojo::PlatformChannel channel;
  mojo::ScopedMessagePipeHandle server_pipe = invitation.AttachMessagePipe(
      diagnostics::kDiagnosticsdMojoConnectionChannelToken);
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 channel.TakeLocalEndpoint());
  diagnosticsd_service_factory_mojo_ptr->Bind(
      mojo::InterfacePtrInfo<diagnosticsd::mojom::DiagnosticsdServiceFactory>(
          std::move(server_pipe), 0 /* version */));
  *remote_endpoint_fd =
      channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD();
}

}  // namespace

DiagnosticsdBridge::Delegate::~Delegate() = default;

// static
DiagnosticsdBridge* DiagnosticsdBridge::Get() {
  return g_diagnosticsd_bridge_instance;
}

// static
base::TimeDelta DiagnosticsdBridge::connection_attempt_interval_for_testing() {
  return kConnectionAttemptInterval;
}

// static
int DiagnosticsdBridge::max_connection_attempt_count_for_testing() {
  return kMaxConnectionAttemptCount;
}

DiagnosticsdBridge::DiagnosticsdBridge()
    : DiagnosticsdBridge(std::make_unique<DiagnosticsdBridgeDelegateImpl>()) {}

DiagnosticsdBridge::DiagnosticsdBridge(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK(delegate_);
  DCHECK(!g_diagnosticsd_bridge_instance);
  g_diagnosticsd_bridge_instance = this;
  WaitForDBusService();
}

DiagnosticsdBridge::~DiagnosticsdBridge() {
  DCHECK_EQ(g_diagnosticsd_bridge_instance, this);
  g_diagnosticsd_bridge_instance = nullptr;
}

void DiagnosticsdBridge::WaitForDBusService() {
  if (connection_attempt_ >= kMaxConnectionAttemptCount) {
    DLOG(WARNING) << "Stopping attempts to connect to diagnosticsd - too many "
                     "unsuccessful attempts in a row";
    return;
  }
  ++connection_attempt_;

  // Cancel any tasks previously created from WaitForDBusService() or
  // ScheduleWaitingForDBusService().
  dbus_waiting_weak_ptr_factory_.InvalidateWeakPtrs();

  DBusThreadManager::Get()
      ->GetDiagnosticsdClient()
      ->WaitForServiceToBeAvailable(
          base::BindOnce(&DiagnosticsdBridge::OnWaitedForDBusService,
                         dbus_waiting_weak_ptr_factory_.GetWeakPtr()));
}

void DiagnosticsdBridge::ScheduleWaitingForDBusService() {
  // Cancel any tasks previously created from WaitForDBusService() or
  // ScheduleWaitingForDBusService().
  dbus_waiting_weak_ptr_factory_.InvalidateWeakPtrs();

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DiagnosticsdBridge::WaitForDBusService,
                     dbus_waiting_weak_ptr_factory_.GetWeakPtr()),
      kConnectionAttemptInterval);
}

void DiagnosticsdBridge::OnWaitedForDBusService(bool service_is_available) {
  if (!service_is_available) {
    DLOG(WARNING) << "The diagnosticsd D-Bus service is unavailable";
    return;
  }

  // Cancel any tasks previously created from WaitForDBusService() or
  // ScheduleWaitingForDBusService().
  dbus_waiting_weak_ptr_factory_.InvalidateWeakPtrs();

  BootstrapMojoConnection();
}

void DiagnosticsdBridge::BootstrapMojoConnection() {
  DCHECK(!diagnosticsd_service_factory_mojo_ptr_);

  // Create a Mojo message pipe and attach
  // |diagnosticsd_service_factory_mojo_ptr_| to its local endpoint.
  base::ScopedFD remote_endpoint_fd;
  delegate_->CreateDiagnosticsdServiceFactoryMojoInvitation(
      &diagnosticsd_service_factory_mojo_ptr_, &remote_endpoint_fd);
  DCHECK(diagnosticsd_service_factory_mojo_ptr_);
  DCHECK(remote_endpoint_fd.is_valid());
  diagnosticsd_service_factory_mojo_ptr_.set_connection_error_handler(
      base::BindOnce(&DiagnosticsdBridge::OnMojoConnectionError,
                     weak_ptr_factory_.GetWeakPtr()));

  // Queue a call that would establish full-duplex Mojo communication with the
  // diagnosticsd daemon by sending an interface pointer to the self instance.
  mojo_self_binding_.Close();
  diagnosticsd::mojom::DiagnosticsdClientPtr self_proxy;
  mojo_self_binding_.Bind(mojo::MakeRequest(&self_proxy));
  diagnosticsd_service_factory_mojo_ptr_->GetService(
      mojo::MakeRequest(&diagnosticsd_service_mojo_ptr_), std::move(self_proxy),
      base::BindOnce(&DiagnosticsdBridge::OnMojoGetServiceCompleted,
                     weak_ptr_factory_.GetWeakPtr()));

  // Send the file descriptor with the Mojo message pipe's remote endpoint to
  // the diagnosticsd daemon via the D-Bus.
  DBusThreadManager::Get()->GetDiagnosticsdClient()->BootstrapMojoConnection(
      std::move(remote_endpoint_fd),
      base::BindOnce(&DiagnosticsdBridge::OnBootstrappedMojoConnection,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DiagnosticsdBridge::OnBootstrappedMojoConnection(bool success) {
  if (success)
    return;
  DLOG(ERROR) << "Failed to establish Mojo connection to diagnosticsd";
  diagnosticsd_service_factory_mojo_ptr_.reset();
  diagnosticsd_service_mojo_ptr_.reset();
  ScheduleWaitingForDBusService();
}

void DiagnosticsdBridge::OnMojoGetServiceCompleted() {
  DCHECK(diagnosticsd_service_mojo_ptr_);
  DVLOG(0) << "Established Mojo communication with diagnosticsd";
  // Reset the current connection attempt counter, since a successful
  // initialization of Mojo communication has completed.
  connection_attempt_ = 0;
}

void DiagnosticsdBridge::OnMojoConnectionError() {
  DLOG(WARNING) << "Mojo connection to the diagnosticsd daemon got shut down";
  diagnosticsd_service_factory_mojo_ptr_.reset();
  diagnosticsd_service_mojo_ptr_.reset();
  ScheduleWaitingForDBusService();
}

}  // namespace chromeos
