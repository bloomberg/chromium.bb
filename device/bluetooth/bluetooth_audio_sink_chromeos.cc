// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_audio_sink_chromeos.h"

#include <sstream>

#include "base/logging.h"

namespace chromeos {

BluetoothAudioSinkChromeOS::BluetoothAudioSinkChromeOS(
    BluetoothAdapterChromeOS* adapter)
    : state_(device::BluetoothAudioSink::STATE_INVALID),
      present_(false),
      powered_(false),
      volume_(0),
      read_mtu_(0),
      write_mtu_(0),
      adapter_(adapter),
      weak_ptr_factory_(this) {
  DCHECK(adapter_);

  present_ = adapter_->IsPresent();
  powered_ = adapter_->IsPowered();
  if (present_ && powered_)
    state_ = device::BluetoothAudioSink::STATE_DISCONNECTED;
  adapter_->AddObserver(this);
}

BluetoothAudioSinkChromeOS::~BluetoothAudioSinkChromeOS() {
  DCHECK(adapter_);
  adapter_->RemoveObserver(this);
}

void BluetoothAudioSinkChromeOS::AddObserver(
    device::BluetoothAudioSink::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BluetoothAudioSinkChromeOS::RemoveObserver(
    device::BluetoothAudioSink::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

device::BluetoothAudioSink::State BluetoothAudioSinkChromeOS::GetState() const {
  return state_;
}

uint16_t BluetoothAudioSinkChromeOS::GetVolume() const {
  return volume_;
}

void BluetoothAudioSinkChromeOS::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  // TODO(mcchou): BUG=441581
  // If |persent| is true, change state to |STATE_DISCONNECTED| and call
  // StateChanged(). Otherwise, change state to |STATE_INVALID| and call
  // StateChanged.
}

void BluetoothAudioSinkChromeOS::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  // TODO(mcchou): BUG=441581
  // If |powered| is true, change state to |STATE_DISCONNECTED| and call
  // StateChanged(). Otherwise, change state to |STATE_INVALID| and call
  // StateChanged.
}

void BluetoothAudioSinkChromeOS::MediaRemoved(
    const dbus::ObjectPath& object_path) {
  // TODO(mcchou): BUG=441581
  // Check if |object_path| equals to |media_path_|. If true, change the state
  // of the audio sink, call StateChanged and reset the audio sink.
}

void BluetoothAudioSinkChromeOS::MediaTransportRemoved(
    const dbus::ObjectPath& object_path) {
  // TODO(mcchou): BUG=441581
  // Check if |object_path| equals to |transport_path_|. If true, change the
  // state of the audio sink, call StateChanged and reset the audio sink.
}

void BluetoothAudioSinkChromeOS::MediaTransportPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  // TODO(mcchou): BUG=441581
  // Call StateChanged and VolumeChanged accordingly if there is any change on
  // state/volume.
}

void BluetoothAudioSinkChromeOS::SetConfiguration(
    const dbus::ObjectPath& transport_path,
    const dbus::MessageReader& properties) {
  // TODO(mcchou): BUG=441581
  // Update |transport_path_| and store properties if needed.
}

void BluetoothAudioSinkChromeOS::SelectConfiguration(
    const std::vector<uint8_t>& capabilities,
    const SelectConfigurationCallback& callback) {
  // TODO(mcchou): BUG=441581
  // Use SelectConfigurationCallback to return the agreed capabilities.
}

void BluetoothAudioSinkChromeOS::ClearConfiguration(
    const dbus::ObjectPath& transport_path) {
  // TODO(mcchou): BUG=441581
  // Reset the configuration to the default one and close IOBuffer.
}

void BluetoothAudioSinkChromeOS::Release() {
  // TODO(mcchou): BUG=441581
  // Let the audio sink does the clean-up and do nothing here.
}

void BluetoothAudioSinkChromeOS::Register(
    const device::BluetoothAudioSink::Options& options,
    const base::Closure& callback,
    const device::BluetoothAudioSink::ErrorCallback& error_callback) {
  // TODO(mcchou): BUG=441581
  // Get Media object, initiate an Media Endpoint with options, and return the
  // audio sink via callback. Add the audio sink as observer of both Media and
  // Media Transport.
}

void BluetoothAudioSinkChromeOS::Unregister(
    const base::Closure& callback,
    const device::BluetoothAudioSink::ErrorCallback& error_callback) {
  // TODO(mcchou): BUG=441581
  // Clean |observers_| and |transport_path_| and reset |state_| and |volume_|.
}

void BluetoothAudioSinkChromeOS::StateChanged(
    device::BluetoothAudioSink::State state) {
  DCHECK_NE(state, state_);
  VLOG(1) << "Bluetooth audio sink state changed: " << state;
  state_ = state;
  FOR_EACH_OBSERVER(device::BluetoothAudioSink::Observer, observers_,
                    BluetoothAudioSinkStateChanged(this, state_));
}

void BluetoothAudioSinkChromeOS::VolumeChanged(uint16_t volume) {
  DCHECK_NE(volume, volume_);
  VLOG(1) << "Bluetooth audio sink volume changed: " << volume;
  volume_ = volume;
  FOR_EACH_OBSERVER(device::BluetoothAudioSink::Observer, observers_,
                    BluetoothAudioSinkVolumeChanged(this, volume_));
}

void BluetoothAudioSinkChromeOS::ReadFromFD() {
  DCHECK_GE(fd_.value(), 0);

  // TODO(mcchou): BUG=441581
  // Read from file descriptor using watcher and create a buffer to contain the
  // data. Notify |Observers_| while there is audio data available.
}

}  // namespace chromeos
