// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/sensors/data_fetcher_shared_memory.h"

#include <GuidDef.h>
#include <InitGuid.h>
#include <PortableDeviceTypes.h>
#include <Sensors.h>
#include <objbase.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/win/iunknown_impl.h"

namespace {

const double kMeanGravity = 9.80665;

}  // namespace

namespace device {

class DataFetcherSharedMemory::SensorEventSink
    : public ISensorEvents,
      public base::win::IUnknownImpl {
 public:
  SensorEventSink() {}
  ~SensorEventSink() override {}

  // IUnknown interface
  ULONG STDMETHODCALLTYPE AddRef() override { return IUnknownImpl::AddRef(); }

  ULONG STDMETHODCALLTYPE Release() override { return IUnknownImpl::Release(); }

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (riid == __uuidof(ISensorEvents)) {
      *ppv = static_cast<ISensorEvents*>(this);
      AddRef();
      return S_OK;
    }
    return IUnknownImpl::QueryInterface(riid, ppv);
  }

  // ISensorEvents interface
  STDMETHODIMP OnEvent(ISensor* sensor,
                       REFGUID event_id,
                       IPortableDeviceValues* event_data) override {
    return S_OK;
  }

  STDMETHODIMP OnLeave(REFSENSOR_ID sensor_id) override { return S_OK; }

  STDMETHODIMP OnStateChanged(ISensor* sensor, SensorState state) override {
    return S_OK;
  }

  STDMETHODIMP OnDataUpdated(ISensor* sensor,
                             ISensorDataReport* new_data) override {
    if (nullptr == new_data || nullptr == sensor)
      return E_INVALIDARG;
    return UpdateSharedMemoryBuffer(sensor, new_data) ? S_OK : E_FAIL;
  }

 protected:
  virtual bool UpdateSharedMemoryBuffer(ISensor* sensor,
                                        ISensorDataReport* new_data) = 0;

  void GetSensorValue(REFPROPERTYKEY property,
                      ISensorDataReport* new_data,
                      double* value,
                      bool* has_value) {
    PROPVARIANT variant_value = {};
    if (SUCCEEDED(new_data->GetSensorValue(property, &variant_value))) {
      if (variant_value.vt == VT_R8)
        *value = variant_value.dblVal;
      else if (variant_value.vt == VT_R4)
        *value = variant_value.fltVal;
      *has_value = true;
    } else {
      *value = 0;
      *has_value = false;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SensorEventSink);
};

class DataFetcherSharedMemory::SensorEventSinkOrientation
    : public DataFetcherSharedMemory::SensorEventSink {
 public:
  explicit SensorEventSinkOrientation(
      DeviceOrientationHardwareBuffer* const buffer)
      : buffer_(buffer) {}
  ~SensorEventSinkOrientation() override {}

 protected:
  bool UpdateSharedMemoryBuffer(ISensor* sensor,
                                ISensorDataReport* new_data) override {
    double alpha, beta, gamma;
    bool has_alpha, has_beta, has_gamma;

    GetSensorValue(SENSOR_DATA_TYPE_TILT_X_DEGREES, new_data, &beta, &has_beta);
    GetSensorValue(SENSOR_DATA_TYPE_TILT_Y_DEGREES, new_data, &gamma,
                   &has_gamma);
    GetSensorValue(SENSOR_DATA_TYPE_TILT_Z_DEGREES, new_data, &alpha,
                   &has_alpha);

    if (buffer_) {
      buffer_->seqlock.WriteBegin();
      buffer_->data.alpha = alpha;
      buffer_->data.has_alpha = has_alpha;
      buffer_->data.beta = beta;
      buffer_->data.has_beta = has_beta;
      buffer_->data.gamma = gamma;
      buffer_->data.has_gamma = has_gamma;
      buffer_->data.absolute = has_alpha || has_beta || has_gamma;
      buffer_->data.all_available_sensors_are_active = true;
      buffer_->seqlock.WriteEnd();
    }

    return true;
  }

 private:
  DeviceOrientationHardwareBuffer* const buffer_;

  DISALLOW_COPY_AND_ASSIGN(SensorEventSinkOrientation);
};

class DataFetcherSharedMemory::SensorEventSinkMotion
    : public DataFetcherSharedMemory::SensorEventSink {
 public:
  explicit SensorEventSinkMotion(DeviceMotionHardwareBuffer* const buffer)
      : buffer_(buffer) {}
  ~SensorEventSinkMotion() override {}

 protected:
  bool UpdateSharedMemoryBuffer(ISensor* sensor,
                                ISensorDataReport* new_data) override {
    SENSOR_TYPE_ID sensor_type = GUID_NULL;
    if (!SUCCEEDED(sensor->GetType(&sensor_type)))
      return false;

    if (IsEqualIID(sensor_type, SENSOR_TYPE_ACCELEROMETER_3D)) {
      double acceleration_including_gravity_x;
      double acceleration_including_gravity_y;
      double acceleration_including_gravity_z;
      bool has_acceleration_including_gravity_x;
      bool has_acceleration_including_gravity_y;
      bool has_acceleration_including_gravity_z;

      GetSensorValue(SENSOR_DATA_TYPE_ACCELERATION_X_G, new_data,
                     &acceleration_including_gravity_x,
                     &has_acceleration_including_gravity_x);
      GetSensorValue(SENSOR_DATA_TYPE_ACCELERATION_Y_G, new_data,
                     &acceleration_including_gravity_y,
                     &has_acceleration_including_gravity_y);
      GetSensorValue(SENSOR_DATA_TYPE_ACCELERATION_Z_G, new_data,
                     &acceleration_including_gravity_z,
                     &has_acceleration_including_gravity_z);

      if (buffer_) {
        buffer_->seqlock.WriteBegin();
        buffer_->data.acceleration_including_gravity_x =
            -acceleration_including_gravity_x * kMeanGravity;
        buffer_->data.has_acceleration_including_gravity_x =
            has_acceleration_including_gravity_x;
        buffer_->data.acceleration_including_gravity_y =
            -acceleration_including_gravity_y * kMeanGravity;
        buffer_->data.has_acceleration_including_gravity_y =
            has_acceleration_including_gravity_y;
        buffer_->data.acceleration_including_gravity_z =
            -acceleration_including_gravity_z * kMeanGravity;
        buffer_->data.has_acceleration_including_gravity_z =
            has_acceleration_including_gravity_z;
        // TODO(timvolodine): consider setting this after all
        // sensors have fired.
        buffer_->data.all_available_sensors_are_active = true;
        buffer_->seqlock.WriteEnd();
      }

    } else if (IsEqualIID(sensor_type, SENSOR_TYPE_GYROMETER_3D)) {
      double alpha, beta, gamma;
      bool has_alpha, has_beta, has_gamma;

      GetSensorValue(SENSOR_DATA_TYPE_ANGULAR_VELOCITY_X_DEGREES_PER_SECOND,
                     new_data, &alpha, &has_alpha);
      GetSensorValue(SENSOR_DATA_TYPE_ANGULAR_VELOCITY_Y_DEGREES_PER_SECOND,
                     new_data, &beta, &has_beta);
      GetSensorValue(SENSOR_DATA_TYPE_ANGULAR_VELOCITY_Z_DEGREES_PER_SECOND,
                     new_data, &gamma, &has_gamma);

      if (buffer_) {
        buffer_->seqlock.WriteBegin();
        buffer_->data.rotation_rate_alpha = alpha;
        buffer_->data.has_rotation_rate_alpha = has_alpha;
        buffer_->data.rotation_rate_beta = beta;
        buffer_->data.has_rotation_rate_beta = has_beta;
        buffer_->data.rotation_rate_gamma = gamma;
        buffer_->data.has_rotation_rate_gamma = has_gamma;
        buffer_->data.all_available_sensors_are_active = true;
        buffer_->seqlock.WriteEnd();
      }
    }

    return true;
  }

 private:
  DeviceMotionHardwareBuffer* const buffer_;

  DISALLOW_COPY_AND_ASSIGN(SensorEventSinkMotion);
};

DataFetcherSharedMemory::DataFetcherSharedMemory() {}

DataFetcherSharedMemory::~DataFetcherSharedMemory() {}

DataFetcherSharedMemory::FetcherType DataFetcherSharedMemory::GetType() const {
  return FETCHER_TYPE_SEPARATE_THREAD;
}

bool DataFetcherSharedMemory::Start(ConsumerType consumer_type, void* buffer) {
  DCHECK(buffer);

  switch (consumer_type) {
    case CONSUMER_TYPE_ORIENTATION: {
      orientation_buffer_ =
          static_cast<DeviceOrientationHardwareBuffer*>(buffer);
      scoped_refptr<SensorEventSink> sink(
          new SensorEventSinkOrientation(orientation_buffer_));
      bool inclinometer_available =
          RegisterForSensor(SENSOR_TYPE_INCLINOMETER_3D,
                            sensor_inclinometer_.GetAddressOf(), sink);
      UMA_HISTOGRAM_BOOLEAN("InertialSensor.InclinometerWindowsAvailable",
                            inclinometer_available);
      if (inclinometer_available)
        return true;
      // if no sensors are available set buffer to ready, to fire null-events.
      SetBufferAvailableState(consumer_type, true);
    } break;
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE: {
      orientation_absolute_buffer_ =
          static_cast<DeviceOrientationHardwareBuffer*>(buffer);
      scoped_refptr<SensorEventSink> sink(
          new SensorEventSinkOrientation(orientation_absolute_buffer_));
      // Currently we use the same sensor as for orientation which provides
      // absolute angles.
      bool inclinometer_available =
          RegisterForSensor(SENSOR_TYPE_INCLINOMETER_3D,
                            sensor_inclinometer_absolute_.GetAddressOf(), sink);
      // TODO(timvolodine): consider adding UMA.
      if (inclinometer_available)
        return true;
      // if no sensors are available set buffer to ready, to fire null-events.
      SetBufferAvailableState(consumer_type, true);
    } break;
    case CONSUMER_TYPE_MOTION: {
      motion_buffer_ = static_cast<DeviceMotionHardwareBuffer*>(buffer);
      scoped_refptr<SensorEventSink> sink(
          new SensorEventSinkMotion(motion_buffer_));
      bool accelerometer_available =
          RegisterForSensor(SENSOR_TYPE_ACCELEROMETER_3D,
                            sensor_accelerometer_.GetAddressOf(), sink);
      bool gyrometer_available = RegisterForSensor(
          SENSOR_TYPE_GYROMETER_3D, sensor_gyrometer_.GetAddressOf(), sink);
      UMA_HISTOGRAM_BOOLEAN("InertialSensor.AccelerometerWindowsAvailable",
                            accelerometer_available);
      UMA_HISTOGRAM_BOOLEAN("InertialSensor.GyrometerWindowsAvailable",
                            gyrometer_available);
      if (accelerometer_available || gyrometer_available) {
        motion_buffer_->seqlock.WriteBegin();
        motion_buffer_->data.interval = GetInterval().InMilliseconds();
        motion_buffer_->seqlock.WriteEnd();
        return true;
      }
      // if no sensors are available set buffer to ready, to fire null-events.
      SetBufferAvailableState(consumer_type, true);
    } break;
    default:
      NOTREACHED();
  }
  return false;
}

bool DataFetcherSharedMemory::Stop(ConsumerType consumer_type) {
  DisableSensors(consumer_type);
  switch (consumer_type) {
    case CONSUMER_TYPE_ORIENTATION:
      SetBufferAvailableState(consumer_type, false);
      orientation_buffer_ = nullptr;
      return true;
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
      SetBufferAvailableState(consumer_type, false);
      orientation_absolute_buffer_ = nullptr;
      return true;
    case CONSUMER_TYPE_MOTION:
      SetBufferAvailableState(consumer_type, false);
      motion_buffer_ = nullptr;
      return true;
    default:
      NOTREACHED();
  }
  return false;
}

bool DataFetcherSharedMemory::RegisterForSensor(
    REFSENSOR_TYPE_ID sensor_type,
    ISensor** sensor,
    scoped_refptr<SensorEventSink> event_sink) {
  Microsoft::WRL::ComPtr<ISensorManager> sensor_manager;
  HRESULT hr = ::CoCreateInstance(CLSID_SensorManager, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&sensor_manager));
  if (FAILED(hr) || !sensor_manager.Get())
    return false;

  Microsoft::WRL::ComPtr<ISensorCollection> sensor_collection;
  hr = sensor_manager->GetSensorsByType(sensor_type,
                                        sensor_collection.GetAddressOf());

  if (FAILED(hr) || !sensor_collection.Get())
    return false;

  ULONG count = 0;
  hr = sensor_collection->GetCount(&count);
  if (FAILED(hr) || !count)
    return false;

  hr = sensor_collection->GetAt(0, sensor);
  if (FAILED(hr) || !(*sensor))
    return false;

  Microsoft::WRL::ComPtr<IPortableDeviceValues> device_values;
  if (SUCCEEDED(::CoCreateInstance(CLSID_PortableDeviceValues, nullptr,
                                   CLSCTX_ALL, IID_PPV_ARGS(&device_values)))) {
    if (SUCCEEDED(device_values->SetUnsignedIntegerValue(
            SENSOR_PROPERTY_CURRENT_REPORT_INTERVAL,
            GetInterval().InMilliseconds()))) {
      Microsoft::WRL::ComPtr<IPortableDeviceValues> return_values;
      (*sensor)->SetProperties(device_values.Get(),
                               return_values.GetAddressOf());
    }
  }

  Microsoft::WRL::ComPtr<ISensorEvents> sensor_events;
  hr = event_sink->QueryInterface(IID_PPV_ARGS(&sensor_events));
  if (FAILED(hr) || !sensor_events.Get())
    return false;

  hr = (*sensor)->SetEventSink(sensor_events.Get());
  if (FAILED(hr))
    return false;

  return true;
}

void DataFetcherSharedMemory::DisableSensors(ConsumerType consumer_type) {
  switch (consumer_type) {
    case CONSUMER_TYPE_ORIENTATION:
      if (sensor_inclinometer_.Get()) {
        sensor_inclinometer_->SetEventSink(nullptr);
        sensor_inclinometer_.Reset();
      }
      break;
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
      if (sensor_inclinometer_absolute_.Get()) {
        sensor_inclinometer_absolute_->SetEventSink(nullptr);
        sensor_inclinometer_absolute_.Reset();
      }
      break;
    case CONSUMER_TYPE_MOTION:
      if (sensor_accelerometer_.Get()) {
        sensor_accelerometer_->SetEventSink(nullptr);
        sensor_accelerometer_.Reset();
      }
      if (sensor_gyrometer_.Get()) {
        sensor_gyrometer_->SetEventSink(nullptr);
        sensor_gyrometer_.Reset();
      }
      break;
    default:
      NOTREACHED();
  }
}

void DataFetcherSharedMemory::SetBufferAvailableState(
    ConsumerType consumer_type,
    bool enabled) {
  switch (consumer_type) {
    case CONSUMER_TYPE_ORIENTATION:
      if (orientation_buffer_) {
        orientation_buffer_->seqlock.WriteBegin();
        orientation_buffer_->data.all_available_sensors_are_active = enabled;
        orientation_buffer_->seqlock.WriteEnd();
      }
      break;
    case CONSUMER_TYPE_ORIENTATION_ABSOLUTE:
      if (orientation_absolute_buffer_) {
        orientation_absolute_buffer_->seqlock.WriteBegin();
        orientation_absolute_buffer_->data.all_available_sensors_are_active =
            enabled;
        orientation_absolute_buffer_->seqlock.WriteEnd();
      }
      break;
    case CONSUMER_TYPE_MOTION:
      if (motion_buffer_) {
        motion_buffer_->seqlock.WriteBegin();
        motion_buffer_->data.all_available_sensors_are_active = enabled;
        motion_buffer_->seqlock.WriteEnd();
      }
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace device
