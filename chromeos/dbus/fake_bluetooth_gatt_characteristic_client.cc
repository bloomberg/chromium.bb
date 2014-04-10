// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_bluetooth_gatt_characteristic_client.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const int kHeartRateMeasurementNotificationIntervalMs = 2000;

}  // namespace

// static
const char FakeBluetoothGattCharacteristicClient::
    kHeartRateMeasurementPathComponent[] = "char0000";
const char FakeBluetoothGattCharacteristicClient::
    kBodySensorLocationPathComponent[] = "char0001";
const char FakeBluetoothGattCharacteristicClient::
    kHeartRateControlPointPathComponent[] = "char0002";

// static
const char FakeBluetoothGattCharacteristicClient::kHeartRateMeasurementUUID[] =
    "00002a37-0000-1000-8000-00805f9b34fb";
const char FakeBluetoothGattCharacteristicClient::kBodySensorLocationUUID[] =
    "00002a38-0000-1000-8000-00805f9b34fb";
const char FakeBluetoothGattCharacteristicClient::kHeartRateControlPointUUID[] =
    "00002a39-0000-1000-8000-00805f9b34fb";

FakeBluetoothGattCharacteristicClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : BluetoothGattCharacteristicClient::Properties(
          NULL,
          bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface,
          callback) {
}

FakeBluetoothGattCharacteristicClient::Properties::~Properties() {
}

void FakeBluetoothGattCharacteristicClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();

  // TODO(armansito): Return success or failure here based on characteristic
  // read permission.
  callback.Run(true);
}

void FakeBluetoothGattCharacteristicClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeBluetoothGattCharacteristicClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  if (property->name() != value.name()) {
    callback.Run(false);
    return;
  }
  // Allow writing to only certain characteristics that are defined with the
  // write permission.
  // TODO(armansito): Actually check against the permissions property instead of
  // UUID, once that property is fully defined in the API.
  if (uuid.value() != kHeartRateControlPointUUID) {
    callback.Run(false);
    return;
  }
  callback.Run(true);
  property->ReplaceValueWithSetValue();
}

FakeBluetoothGattCharacteristicClient::FakeBluetoothGattCharacteristicClient()
    : heart_rate_visible_(false),
      calories_burned_(0),
      weak_ptr_factory_(this) {
}

FakeBluetoothGattCharacteristicClient::
    ~FakeBluetoothGattCharacteristicClient() {
}

void FakeBluetoothGattCharacteristicClient::Init(dbus::Bus* bus) {
}

void FakeBluetoothGattCharacteristicClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBluetoothGattCharacteristicClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<dbus::ObjectPath>
FakeBluetoothGattCharacteristicClient::GetCharacteristics() {
  std::vector<dbus::ObjectPath> paths;
  if (IsHeartRateVisible()) {
    paths.push_back(dbus::ObjectPath(heart_rate_measurement_path_));
    paths.push_back(dbus::ObjectPath(body_sensor_location_path_));
    paths.push_back(dbus::ObjectPath(heart_rate_control_point_path_));
  }
  return paths;
}

FakeBluetoothGattCharacteristicClient::Properties*
FakeBluetoothGattCharacteristicClient::GetProperties(
    const dbus::ObjectPath& object_path) {
  if (object_path.value() == heart_rate_measurement_path_) {
    DCHECK(heart_rate_measurement_properties_.get());
    return heart_rate_measurement_properties_.get();
  }
  if (object_path.value() == body_sensor_location_path_) {
    DCHECK(body_sensor_location_properties_.get());
    return body_sensor_location_properties_.get();
  }
  if (object_path.value() == heart_rate_control_point_path_) {
    DCHECK(heart_rate_control_point_properties_.get());
    return heart_rate_control_point_properties_.get();
  }
  return NULL;
}

void FakeBluetoothGattCharacteristicClient::ExposeHeartRateCharacteristics(
    const dbus::ObjectPath& service_path) {
  if (IsHeartRateVisible()) {
    VLOG(2) << "Fake Heart Rate characteristics are already visible.";
    return;
  }

  VLOG(2) << "Exposing fake Heart Rate characteristics.";

  // ==== Heart Rate Measurement Characteristic ====
  heart_rate_measurement_path_ =
      service_path.value() + "/" + kHeartRateMeasurementPathComponent;
  heart_rate_measurement_properties_.reset(new Properties(base::Bind(
      &FakeBluetoothGattCharacteristicClient::OnPropertyChanged,
      weak_ptr_factory_.GetWeakPtr(),
      dbus::ObjectPath(heart_rate_measurement_path_))));
  heart_rate_measurement_properties_->uuid.ReplaceValue(
      kHeartRateMeasurementUUID);
  heart_rate_measurement_properties_->service.ReplaceValue(service_path);

  // TODO(armansito): Fill out the flags field once bindings for the values have
  // been added. For now, leave it empty.

  std::vector<uint8> measurement_value = GetHeartRateMeasurementValue();
  heart_rate_measurement_properties_->value.ReplaceValue(measurement_value);

  // ==== Body Sensor Location Characteristic ====
  body_sensor_location_path_ =
      service_path.value() + "/" + kBodySensorLocationPathComponent;
  body_sensor_location_properties_.reset(new Properties(base::Bind(
      &FakeBluetoothGattCharacteristicClient::OnPropertyChanged,
      weak_ptr_factory_.GetWeakPtr(),
      dbus::ObjectPath(body_sensor_location_path_))));
  body_sensor_location_properties_->uuid.ReplaceValue(kBodySensorLocationUUID);
  body_sensor_location_properties_->service.ReplaceValue(service_path);

  // TODO(armansito): Fill out the flags field once bindings for the values have
  // been added. For now, leave it empty.

  // The sensor is in the "Other" location.
  std::vector<uint8> body_sensor_location_value;
  body_sensor_location_value.push_back(0);
  body_sensor_location_properties_->value.ReplaceValue(
      body_sensor_location_value);

  // ==== Heart Rate Control Point Characteristic ====
  heart_rate_control_point_path_ =
      service_path.value() + "/" + kHeartRateControlPointPathComponent;
  heart_rate_control_point_properties_.reset(new Properties(base::Bind(
      &FakeBluetoothGattCharacteristicClient::OnPropertyChanged,
      weak_ptr_factory_.GetWeakPtr(),
      dbus::ObjectPath(heart_rate_control_point_path_))));
  heart_rate_control_point_properties_->uuid.ReplaceValue(
      kHeartRateControlPointUUID);
  heart_rate_control_point_properties_->service.ReplaceValue(service_path);

  // TODO(armansito): Fill out the flags field once bindings for the values have
  // been added. For now, leave it empty.

  // Set the initial value to 0. Whenever this gets set to 1, we will reset the
  // total calories burned and change the value back to 0.
  std::vector<uint8> heart_rate_control_point_value;
  heart_rate_control_point_value.push_back(0);
  heart_rate_control_point_properties_->value.ReplaceValue(
      heart_rate_control_point_value);

  heart_rate_visible_ = true;

  NotifyCharacteristicAdded(dbus::ObjectPath(heart_rate_measurement_path_));
  NotifyCharacteristicAdded(dbus::ObjectPath(body_sensor_location_path_));
  NotifyCharacteristicAdded(dbus::ObjectPath(heart_rate_control_point_path_));

  // Set up notifications for heart rate measurement.
  // TODO(armansito): Do this based on the value of the "client characteristic
  // configuration" descriptor. Since it's still unclear how descriptors will
  // be handled by BlueZ, automatically set up notifications for now.
  ScheduleHeartRateMeasurementValueChange();

  // TODO(armansito): Add descriptors.
}

void FakeBluetoothGattCharacteristicClient::HideHeartRateCharacteristics() {
  VLOG(2) << "Hiding fake Heart Rate characteristics.";

  // Notify the observers before deleting the properties structures so that they
  // can be accessed from the observer method.
  NotifyCharacteristicRemoved(dbus::ObjectPath(heart_rate_measurement_path_));
  NotifyCharacteristicRemoved(dbus::ObjectPath(body_sensor_location_path_));
  NotifyCharacteristicRemoved(dbus::ObjectPath(heart_rate_control_point_path_));

  heart_rate_measurement_properties_.reset();
  body_sensor_location_properties_.reset();
  heart_rate_control_point_properties_.reset();

  heart_rate_measurement_path_.clear();
  body_sensor_location_path_.clear();
  heart_rate_control_point_path_.clear();
  heart_rate_visible_ = false;
}

dbus::ObjectPath
FakeBluetoothGattCharacteristicClient::GetHeartRateMeasurementPath() const {
  return dbus::ObjectPath(heart_rate_measurement_path_);
}

dbus::ObjectPath
FakeBluetoothGattCharacteristicClient::GetBodySensorLocationPath() const {
  return dbus::ObjectPath(body_sensor_location_path_);
}

dbus::ObjectPath
FakeBluetoothGattCharacteristicClient::GetHeartRateControlPointPath() const {
  return dbus::ObjectPath(heart_rate_control_point_path_);
}

void FakeBluetoothGattCharacteristicClient::OnPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  VLOG(2) << "Characteristic property changed: " << object_path.value()
          << ": " << property_name;

  FOR_EACH_OBSERVER(BluetoothGattCharacteristicClient::Observer, observers_,
                    GattCharacteristicPropertyChanged(
                        object_path, property_name));

  // If the heart rate control point was set, reset the calories burned.
  if (object_path.value() != heart_rate_control_point_path_)
    return;
  DCHECK(heart_rate_control_point_properties_.get());
  dbus::Property<std::vector<uint8> >* value_prop =
      &heart_rate_control_point_properties_->value;
  if (property_name != value_prop->name())
    return;

  std::vector<uint8> value = value_prop->value();
  DCHECK(value.size() == 1);
  if (value[0] == 0)
    return;

  DCHECK(value[0] == 1);
  calories_burned_ = 0;
  value[0] = 0;
  value_prop->ReplaceValue(value);
}

void FakeBluetoothGattCharacteristicClient::NotifyCharacteristicAdded(
    const dbus::ObjectPath& object_path) {
  VLOG(2) << "GATT characteristic added: " << object_path.value();
  FOR_EACH_OBSERVER(BluetoothGattCharacteristicClient::Observer, observers_,
                    GattCharacteristicAdded(object_path));
}

void FakeBluetoothGattCharacteristicClient::NotifyCharacteristicRemoved(
    const dbus::ObjectPath& object_path) {
  VLOG(2) << "GATT characteristic removed: " << object_path.value();
  FOR_EACH_OBSERVER(BluetoothGattCharacteristicClient::Observer, observers_,
                    GattCharacteristicRemoved(object_path));
}

void FakeBluetoothGattCharacteristicClient::
    ScheduleHeartRateMeasurementValueChange() {
  if (!IsHeartRateVisible())
    return;
  VLOG(2) << "Updating heart rate value.";
  std::vector<uint8> measurement = GetHeartRateMeasurementValue();
  heart_rate_measurement_properties_->value.ReplaceValue(measurement);

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeBluetoothGattCharacteristicClient::
                     ScheduleHeartRateMeasurementValueChange,
                 weak_ptr_factory_.GetWeakPtr()),
                 base::TimeDelta::FromMilliseconds(
                     kHeartRateMeasurementNotificationIntervalMs));
}

std::vector<uint8>
FakeBluetoothGattCharacteristicClient::GetHeartRateMeasurementValue() {
  // TODO(armansito): We should make sure to properly pack this struct to ensure
  // correct byte alignment and endianness. It doesn't matter too much right now
  // as this is a fake and GCC on Linux seems to do the right thing.
  struct {
    uint8 flags;
    uint8 bpm;
    uint16 energy_expanded;
    uint16 rr_interval;
  } value;

  // Flags in LSB:     0       11   1 1 000
  //                   |       |    | | |
  // 8-bit bpm format --       |    | | |
  // Sensor contact supported --    | | |
  // Energy expanded field present -- | |
  // RR-Interval values present ------- |
  // Reserved for future use ------------
  value.flags = 0x0;
  value.flags |= (0x03 << 1);
  value.flags |= (0x01 << 3);
  value.flags |= (0x01 << 4);

  // Pick a value between 117 bpm and 153 bpm for heart rate.
  value.bpm = static_cast<uint8>(base::RandInt(117, 153));

  // Total calories burned in kJoules since the last reset. Increment this by 1
  // every time. It's fine if it overflows: it becomes 0 when the user resets
  // the heart rate monitor (or pretend that he had a lot of cheeseburgers).
  value.energy_expanded = calories_burned_++;

  // Include one RR-Interval value, in seconds.
  value.rr_interval = 60/value.bpm;

  // Return the bytes in an array.
  uint8* bytes = reinterpret_cast<uint8*>(&value);
  std::vector<uint8> return_value;
  return_value.assign(bytes, bytes + sizeof(value));
  return return_value;
}

bool FakeBluetoothGattCharacteristicClient::IsHeartRateVisible() const {
  DCHECK(heart_rate_visible_ != heart_rate_measurement_path_.empty());
  DCHECK(heart_rate_visible_ != body_sensor_location_path_.empty());
  DCHECK(heart_rate_visible_ != heart_rate_control_point_path_.empty());
  DCHECK(heart_rate_visible_ == !!heart_rate_measurement_properties_.get());
  DCHECK(heart_rate_visible_ == !!body_sensor_location_properties_.get());
  DCHECK(heart_rate_visible_ == !!heart_rate_control_point_properties_.get());
  return heart_rate_visible_;
}

}  // namespace chromeos
