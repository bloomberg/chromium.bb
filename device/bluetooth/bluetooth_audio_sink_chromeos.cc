// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_audio_sink_chromeos.h"

#include <sstream>
#include <vector>

#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "dbus/message.h"
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

  adapter_->AddObserver(this);

  chromeos::BluetoothMediaClient* media =
      DBusThreadManager::Get()->GetBluetoothMediaClient();
  DCHECK(media);
  media->AddObserver(this);

  chromeos::BluetoothMediaTransportClient *transport =
      chromeos::DBusThreadManager::Get()->GetBluetoothMediaTransportClient();
  DCHECK(transport);
  transport->AddObserver(this);

  StateChanged(device::BluetoothAudioSink::STATE_DISCONNECTED);
}

BluetoothAudioSinkChromeOS::~BluetoothAudioSinkChromeOS() {
  DCHECK(adapter_.get());
  adapter_->RemoveObserver(this);

  chromeos::BluetoothMediaClient* media =
      DBusThreadManager::Get()->GetBluetoothMediaClient();
  DCHECK(media);
  media->RemoveObserver(this);

  chromeos::BluetoothMediaTransportClient *transport =
      chromeos::DBusThreadManager::Get()->GetBluetoothMediaTransportClient();
  DCHECK(transport);
  transport->RemoveObserver(this);

  // TODO(mcchou): BUG=441581
  // Unregister() should be called while media and media endpoint are still
  // valid.
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
  // Changes |state_| to STATE_INVALID or STATE_DISCONNECTED?
  if (object_path == media_path_) {
    StateChanged(device::BluetoothAudioSink::STATE_INVALID);
  }
}

void BluetoothAudioSinkChromeOS::MediaTransportRemoved(
    const dbus::ObjectPath& object_path) {
  // Whenever powered of |adapter_| turns false while present stays true, media
  // transport object should be removed accordingly, and the state should be
  // changed to STATE_DISCONNECTED.
  if (object_path == transport_path_) {
    StateChanged(device::BluetoothAudioSink::STATE_DISCONNECTED);
  }
}

void BluetoothAudioSinkChromeOS::MediaTransportPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  if (object_path != transport_path_)
    return;

  // Retrieves the property set of the transport object with |object_path|.
  chromeos::BluetoothMediaTransportClient::Properties* properties =
      DBusThreadManager::Get()
          ->GetBluetoothMediaTransportClient()
          ->GetProperties(object_path);

  // Dispatches a property changed event to the corresponding handler.
  if (property_name == properties->state.name()) {
    if (properties->state.value() ==
        BluetoothMediaTransportClient::kStateIdle) {
      StateChanged(device::BluetoothAudioSink::STATE_IDLE);
    } else if (properties->state.value() ==
               BluetoothMediaTransportClient::kStatePending) {
      StateChanged(device::BluetoothAudioSink::STATE_PENDING);
    } else if (properties->state.value() ==
               BluetoothMediaTransportClient::kStateActive) {
      StateChanged(device::BluetoothAudioSink::STATE_ACTIVE);
    }
  } else if (property_name == properties->volume.name()) {
    VolumeChanged(properties->volume.value());
  } else {
    VLOG(1) << "Bluetooth audio sink: transport property " << property_name
            << " changed";
  }
}

void BluetoothAudioSinkChromeOS::SetConfiguration(
    const dbus::ObjectPath& transport_path,
    const TransportProperties& properties) {
  VLOG(1) << "Bluetooth audio sink: SetConfiguration called";
  transport_path_ = transport_path;

  // The initial state for a connection should be "idle".
  if (properties.state != BluetoothMediaTransportClient::kStateIdle) {
    VLOG(1) << "Bluetooth Audio Sink: unexpected state " << properties.state;
    return;
  }

  // Updates |volume_| if the volume level is provided in |properties|.
  if (properties.volume.get() != nullptr)
    VolumeChanged(*properties.volume);

  StateChanged(device::BluetoothAudioSink::STATE_IDLE);
}

void BluetoothAudioSinkChromeOS::SelectConfiguration(
    const std::vector<uint8_t>& capabilities,
    const SelectConfigurationCallback& callback) {
  VLOG(1) << "Bluetooth audio sink: SelectConfiguration called";
  callback.Run(options_.capabilities);
}

void BluetoothAudioSinkChromeOS::ClearConfiguration(
    const dbus::ObjectPath& transport_path) {
  VLOG(1) << "Bluetooth audio sink: ClearConfiguration called";
  StateChanged(device::BluetoothAudioSink::STATE_DISCONNECTED);
}

void BluetoothAudioSinkChromeOS::Released() {
  VLOG(1) << "Bluetooth audio sink: Released called";
  StateChanged(device::BluetoothAudioSink::STATE_INVALID);
}

void BluetoothAudioSinkChromeOS::Register(
    const device::BluetoothAudioSink::Options& options,
    const base::Closure& callback,
    const device::BluetoothAudioSink::ErrorCallback& error_callback) {
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

  media_path_ = static_cast<BluetoothAdapterChromeOS*>(
      adapter_.get())->object_path();

  chromeos::BluetoothMediaClient* media =
      DBusThreadManager::Get()->GetBluetoothMediaClient();
  DCHECK(media);
  media->RegisterEndpoint(
      media_path_,
      endpoint_path_,
      endpoint_properties,
      base::Bind(&BluetoothAudioSinkChromeOS::OnRegisterSucceeded,
                 weak_ptr_factory_.GetWeakPtr(), callback),
      base::Bind(&BluetoothAudioSinkChromeOS::OnRegisterFailed,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));
}

BluetoothMediaEndpointServiceProvider*
    BluetoothAudioSinkChromeOS::GetEndpointServiceProvider() {
  return media_endpoint_.get();
}

void BluetoothAudioSinkChromeOS::StateChanged(
    device::BluetoothAudioSink::State state) {
  if (state == state_)
    return;

  VLOG(1) << "Bluetooth audio sink state changed: " << state;

  switch (state) {
    case device::BluetoothAudioSink::STATE_INVALID: {
      // TODO(mcchou): BUG=441581
      ResetMedia();
      ResetTransport();
      ResetEndpoint();
      break;
    }
    case device::BluetoothAudioSink::STATE_DISCONNECTED: {
      // TODO(mcchou): BUG=441581
      // Clean media transport and remove the audio sink from its observer list.
      ResetTransport();
      break;
    }
    case device::BluetoothAudioSink::STATE_IDLE: {
      // TODO(mcchou): BUG=441581
      // Triggered by MediaTransportPropertyChanged and SetConfiguration.
      // Stop watching on file descriptor if there is one.
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

  ResetEndpoint();
  error_callback.Run(device::BluetoothAudioSink::ERROR_NOT_REGISTERED);
}

void BluetoothAudioSinkChromeOS::ReadFromFD() {
  DCHECK_GE(fd_.value(), 0);

  // TODO(mcchou): BUG=441581
  // Read from file descriptor using watcher and create a buffer to contain the
  // data. Notify |Observers_| while there is audio data available.
}

void BluetoothAudioSinkChromeOS::ResetMedia() {
  media_path_ = dbus::ObjectPath("");
}

void BluetoothAudioSinkChromeOS::ResetTransport() {
  transport_path_ = dbus::ObjectPath("");
  volume_ = 0;
  read_mtu_ = 0;
  write_mtu_ = 0;
  fd_.PutValue(-1);
}

void BluetoothAudioSinkChromeOS::ResetEndpoint() {
  endpoint_path_ = dbus::ObjectPath("");
  media_endpoint_ = nullptr;
}

}  // namespace chromeos
