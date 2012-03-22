// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/bluetooth_event_router.h"

#include "base/json/json_writer.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_adapter.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"

namespace chromeos {

ExtensionBluetoothEventRouter::ExtensionBluetoothEventRouter(Profile* profile)
    : profile_(profile),
      adapter_(chromeos::BluetoothAdapter::CreateDefaultAdapter()) {
  DCHECK(profile_);
  DCHECK(adapter_.get());
  adapter_->AddObserver(this);
}

ExtensionBluetoothEventRouter::~ExtensionBluetoothEventRouter() {
  adapter_->RemoveObserver(this);
}

void ExtensionBluetoothEventRouter::AdapterPresentChanged(
    chromeos::BluetoothAdapter* adapter, bool present) {
  DCHECK(adapter == adapter_.get());
  DispatchEvent(extension_event_names::kBluetoothOnAvailabilityChanged,
      present);
}

void ExtensionBluetoothEventRouter::AdapterPoweredChanged(
    chromeos::BluetoothAdapter* adapter, bool has_power) {
  DCHECK(adapter == adapter_.get());
  DispatchEvent(extension_event_names::kBluetoothOnPowerChanged, has_power);
}

void ExtensionBluetoothEventRouter::DispatchEvent(
    const char* event_name, bool value) {
  ListValue args;
  args.Append(Value::CreateBooleanValue(value));
  std::string json_args;
  base::JSONWriter::Write(&args, &json_args);

  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      event_name, json_args, NULL, GURL());
}

}  // namespace chromeos
