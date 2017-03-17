// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/tray_bluetooth_helper.h"

#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm_shell.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_pairing_dialog.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"

using chromeos::BluetoothPairingDialog;

namespace {

void BluetoothSetDiscoveringError() {
  LOG(ERROR) << "BluetoothSetDiscovering failed.";
}

void BluetoothDeviceConnectError(
    device::BluetoothDevice::ConnectErrorCode error_code) {}

ash::SystemTrayNotifier* GetSystemTrayNotifier() {
  return ash::WmShell::Get()->system_tray_notifier();
}

}  // namespace

TrayBluetoothHelper::TrayBluetoothHelper() : weak_ptr_factory_(this) {}

TrayBluetoothHelper::~TrayBluetoothHelper() {
  if (adapter_)
    adapter_->RemoveObserver(this);
}

void TrayBluetoothHelper::Initialize() {
  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&TrayBluetoothHelper::InitializeOnAdapterReady,
                 weak_ptr_factory_.GetWeakPtr()));
}

void TrayBluetoothHelper::InitializeOnAdapterReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  CHECK(adapter_);
  adapter_->AddObserver(this);
}

void TrayBluetoothHelper::GetAvailableDevices(
    std::vector<ash::BluetoothDeviceInfo>* list) {
  device::BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  for (device::BluetoothDevice* device : devices) {
    ash::BluetoothDeviceInfo info;
    info.address = device->GetAddress();
    info.display_name = device->GetNameForDisplay();
    info.connected = device->IsConnected();
    info.connecting = device->IsConnecting();
    info.paired = device->IsPaired();
    info.device_type = device->GetDeviceType();
    list->push_back(info);
  }
}

void TrayBluetoothHelper::StartDiscovering() {
  if (HasDiscoverySession()) {
    LOG(WARNING) << "Already have active Bluetooth device discovery session.";
    return;
  }
  VLOG(1) << "Requesting new Bluetooth device discovery session.";
  should_run_discovery_ = true;
  adapter_->StartDiscoverySession(
      base::Bind(&TrayBluetoothHelper::OnStartDiscoverySession,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&BluetoothSetDiscoveringError));
}

void TrayBluetoothHelper::StopDiscovering() {
  should_run_discovery_ = false;
  if (!HasDiscoverySession()) {
    LOG(WARNING) << "No active Bluetooth device discovery session.";
    return;
  }
  VLOG(1) << "Stopping Bluetooth device discovery session.";
  discovery_session_->Stop(base::Bind(&base::DoNothing),
                           base::Bind(&BluetoothSetDiscoveringError));
}

void TrayBluetoothHelper::ConnectToDevice(const std::string& address) {
  device::BluetoothDevice* device = adapter_->GetDevice(address);
  if (!device || device->IsConnecting() ||
      (device->IsConnected() && device->IsPaired())) {
    return;
  }
  if (device->IsPaired() && !device->IsConnectable())
    return;
  if (device->IsPaired() || !device->IsPairable()) {
    base::RecordAction(
        base::UserMetricsAction("StatusArea_Bluetooth_Connect_Known"));
    device->Connect(NULL, base::Bind(&base::DoNothing),
                    base::Bind(&BluetoothDeviceConnectError));
    return;
  }
  // Show pairing dialog for the unpaired device.
  // TODO(jamescook): Move into SystemTrayClient and wire up with mojo.
  base::RecordAction(
      base::UserMetricsAction("StatusArea_Bluetooth_Connect_Unknown"));
  BluetoothPairingDialog* dialog = new BluetoothPairingDialog(device);
  // The dialog deletes itself on close.
  dialog->ShowInContainer(SystemTrayClient::GetDialogParentContainerId());
}

bool TrayBluetoothHelper::IsDiscovering() const {
  return adapter_ && adapter_->IsDiscovering();
}

void TrayBluetoothHelper::ToggleEnabled() {
  adapter_->SetPowered(!adapter_->IsPowered(), base::Bind(&base::DoNothing),
                       base::Bind(&base::DoNothing));
}

bool TrayBluetoothHelper::GetAvailable() {
  return adapter_ && adapter_->IsPresent();
}

bool TrayBluetoothHelper::GetEnabled() {
  return adapter_ && adapter_->IsPowered();
}

bool TrayBluetoothHelper::HasDiscoverySession() {
  return discovery_session_ && discovery_session_->IsActive();
}

////////////////////////////////////////////////////////////////////////////////
// BluetoothAdapter::Observer:

void TrayBluetoothHelper::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void TrayBluetoothHelper::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void TrayBluetoothHelper::AdapterDiscoveringChanged(
    device::BluetoothAdapter* adapter,
    bool discovering) {
  GetSystemTrayNotifier()->NotifyBluetoothDiscoveringChanged();
}

void TrayBluetoothHelper::DeviceAdded(device::BluetoothAdapter* adapter,
                                      device::BluetoothDevice* device) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void TrayBluetoothHelper::DeviceChanged(device::BluetoothAdapter* adapter,
                                        device::BluetoothDevice* device) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void TrayBluetoothHelper::DeviceRemoved(device::BluetoothAdapter* adapter,
                                        device::BluetoothDevice* device) {
  GetSystemTrayNotifier()->NotifyRefreshBluetooth();
}

void TrayBluetoothHelper::OnStartDiscoverySession(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  // If the discovery session was returned after a request to stop discovery
  // (e.g. the user dismissed the Bluetooth detailed view before the call
  // returned), don't claim the discovery session and let it clean up.
  if (!should_run_discovery_)
    return;
  VLOG(1) << "Claiming new Bluetooth device discovery session.";
  discovery_session_ = std::move(discovery_session);
  GetSystemTrayNotifier()->NotifyBluetoothDiscoveringChanged();
}
