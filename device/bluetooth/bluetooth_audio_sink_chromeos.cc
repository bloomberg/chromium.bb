// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_audio_sink_chromeos.h"

#include <sstream>

#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "device/bluetooth/bluetooth_adapter_chromeos.h"

namespace {

// TODO(mcchou): Add the constant to dbus/service_constants.h.
const char kBluetoothAudioSinkServicePath[] = "/org/chromium/AudioSink";

dbus::ObjectPath GenerateEndpointPath() {
  static unsigned int sequence_number = 0;
  ++sequence_number;
  std::stringstream path;
  path << kBluetoothAudioSinkServicePath << "/endpoint" << sequence_number;
  return dbus::ObjectPath(path.str());
}

}  // namespace

namespace chromeos {

BluetoothAudioSinkChromeOS::BluetoothAudioSinkChromeOS(
    scoped_refptr<device::BluetoothAdapter> adapter)
    : state_(device::BluetoothAudioSink::STATE_INVALID),
      volume_(0),
      read_mtu_(0),
      write_mtu_(0),
      adapter_(adapter),
      weak_ptr_factory_(this) {
  DCHECK(adapter_.get());
  DCHECK(adapter_->IsPresent());

  StateChanged(device::BluetoothAudioSink::STATE_DISCONNECTED);
  adapter_->AddObserver(this);
}

BluetoothAudioSinkChromeOS::~BluetoothAudioSinkChromeOS() {
  DCHECK(adapter_.get());
  adapter_->RemoveObserver(this);
  // TODO(mcchou): BUG=441581
  // Unregister() should be called while media and media endpoint are still
  // valid.
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
  VLOG(1) << "Bluetooth audio sink: Bluetooth adapter present changed: "
          << present;

  if (adapter->IsPresent()) {
    StateChanged(device::BluetoothAudioSink::STATE_DISCONNECTED);
  } else {
    StateChanged(device::BluetoothAudioSink::STATE_INVALID);
  }
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
  // Whenever powered of |adapter_| turns false while present stays true, media
  // transport object should be removed accordingly, and the state should be
  // changed to STATE_DISCONNECTED.
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
  VLOG(1) << "Bluetooth audio sink: SelectConfiguration called";
  callback.Run(options_.capabilities);
}

void BluetoothAudioSinkChromeOS::ClearConfiguration(
    const dbus::ObjectPath& transport_path) {
  // TODO(mcchou): BUG=441581
  // Reset the configuration to the default one and close IOBuffer.
}

void BluetoothAudioSinkChromeOS::Release() {
  VLOG(1) << "Bluetooth audio sink: Release called";
  StateChanged(device::BluetoothAudioSink::STATE_INVALID);
}

void BluetoothAudioSinkChromeOS::Register(
    const device::BluetoothAudioSink::Options& options,
    const base::Closure& callback,
    const device::BluetoothAudioSink::ErrorCallback& error_callback) {
  // TODO(mcchou): BUG=441581
  // Get Media object, initiate an Media Endpoint with options, and return the
  // audio sink via callback. Add the audio sink as observer of both Media and
  // Media Transport.
  DCHECK(adapter_.get());
  DCHECK_EQ(state_, device::BluetoothAudioSink::STATE_DISCONNECTED);

  // Gets system bus.
  dbus::Bus* system_bus = DBusThreadManager::Get()->GetSystemBus();

  // Creates a Media Endpoint with newly-generated path.
  endpoint_path_ = GenerateEndpointPath();
  media_endpoint_.reset(
      BluetoothMediaEndpointServiceProvider::Create(
          system_bus, endpoint_path_, this));

  DCHECK(media_endpoint_.get());

  // Creates endpoint properties with |options|.
  options_ = options;
  chromeos::BluetoothMediaClient::EndpointProperties endpoint_properties;
  endpoint_properties.uuid = BluetoothMediaClient::kBluetoothAudioSinkUUID;
  endpoint_properties.codec = options_.codec;
  endpoint_properties.capabilities = options_.capabilities;

  // Gets Media object exported by D-Bus and registers the endpoint.
  chromeos::BluetoothMediaClient* media =
      DBusThreadManager::Get()->GetBluetoothMediaClient();
  BluetoothAdapterChromeOS* adapter_chromeos =
      static_cast<BluetoothAdapterChromeOS*>(adapter_.get());
  media->AddObserver(this);
  media_path_ = adapter_chromeos->object_path();
  media->RegisterEndpoint(
      media_path_,
      endpoint_path_,
      endpoint_properties,
      base::Bind(&BluetoothAudioSinkChromeOS::OnRegisterSucceeded,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&BluetoothAudioSinkChromeOS::OnRegisterFailed,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

void BluetoothAudioSinkChromeOS::Unregister(
    const base::Closure& callback,
    const device::BluetoothAudioSink::ErrorCallback& error_callback) {
  // TODO(mcchou): BUG=441581
  // Call UnregisterEndpoint on the media object with |media_path_| and clean
  // |observers_| and transport_path_ and reset state_ and volume.
  // Check whether the media endpoint is registered before invoking
  // UnregisterEndpoint.
}

void BluetoothAudioSinkChromeOS::StateChanged(
    device::BluetoothAudioSink::State state) {
  if (state == state_)
    return;

  VLOG(1) << "Bluetooth audio sink state changed: " << state;

  switch (state) {
    case device::BluetoothAudioSink::STATE_INVALID: {
      // TODO(mcchou): BUG=441581
      // Clean media, media transport and media endpoint.
      break;
    }
    case device::BluetoothAudioSink::STATE_DISCONNECTED: {
      // TODO(mcchou): BUG=441581
      // Clean media transport.
      break;
    }
    case device::BluetoothAudioSink::STATE_IDLE: {
      // TODO(mcchou): BUG=441581
      // Triggered by MediaTransportPropertyChanged. Stop watching on file
      // descriptor if there is one.
      break;
    }
    case device::BluetoothAudioSink::STATE_PENDING: {
      // TODO(mcchou): BUG=441581
      // Call BluetoothMediaTransportClient::Acquire() to get fd and mtus.
      break;
    }
    case device::BluetoothAudioSink::STATE_ACTIVE: {
      // TODO(mcchou): BUG=441581
      // Read from fd and call DataAvailable.
      ReadFromFD();
      break;
    }
    default:
      break;
  }

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

void BluetoothAudioSinkChromeOS::OnRegisterSucceeded(
    const base::Closure& callback) {
  VLOG(1) << "Bluetooth audio sink registerd";

  StateChanged(device::BluetoothAudioSink::STATE_DISCONNECTED);
  callback.Run();
}

void BluetoothAudioSinkChromeOS::OnRegisterFailed(
    const BluetoothAudioSink::ErrorCallback& error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  DCHECK(media_endpoint_.get());
  VLOG(1) << "Bluetooth audio sink: " << error_name << ": " <<  error_message;

  endpoint_path_ = dbus::ObjectPath("");
  media_endpoint_ = nullptr;
  error_callback.Run(device::BluetoothAudioSink::ERROR_NOT_REGISTERED);
}

void BluetoothAudioSinkChromeOS::ReadFromFD() {
  DCHECK_GE(fd_.value(), 0);

  // TODO(mcchou): BUG=441581
  // Read from file descriptor using watcher and create a buffer to contain the
  // data. Notify |Observers_| while there is audio data available.
}

}  // namespace chromeos
