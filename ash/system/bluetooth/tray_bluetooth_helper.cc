// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/tray_bluetooth_helper.h"

using device::mojom::BluetoothSystem;

namespace ash {

TrayBluetoothHelper::TrayBluetoothHelper() = default;

TrayBluetoothHelper::~TrayBluetoothHelper() = default;

void TrayBluetoothHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TrayBluetoothHelper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool TrayBluetoothHelper::IsBluetoothStateAvailable() {
  switch (GetBluetoothState()) {
    case BluetoothSystem::State::kUnsupported:
    case BluetoothSystem::State::kUnavailable:
      return false;
    case BluetoothSystem::State::kPoweredOff:
    case BluetoothSystem::State::kTransitioning:
    case BluetoothSystem::State::kPoweredOn:
      return true;
  }
}

void TrayBluetoothHelper::NotifyBluetoothSystemStateChanged() {
  for (auto& observer : observers_)
    observer.OnBluetoothSystemStateChanged();
}

void TrayBluetoothHelper::NotifyBluetoothScanStateChanged() {
  for (auto& observer : observers_)
    observer.OnBluetoothScanStateChanged();
}

void TrayBluetoothHelper::NotifyBluetoothDeviceListChanged() {
  for (auto& observer : observers_)
    observer.OnBluetoothDeviceListChanged();
}

}  // namespace ash
