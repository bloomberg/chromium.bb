// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "data_fetcher_shared_memory.h"

#include <GuidDef.h>
#include <InitGuid.h>
#include <PortableDeviceTypes.h>
#include <Sensors.h>

#include "base/logging.h"
#include "base/win/iunknown_impl.h"
#include "base/win/windows_version.h"

namespace {

const int kPeriodInMilliseconds = 100;

}  // namespace


namespace content {

class DataFetcherSharedMemory::SensorEventSink
    : public ISensorEvents, public base::win::IUnknownImpl {
 public:
  SensorEventSink() {}
  virtual ~SensorEventSink() {}

  // IUnknown interface
  virtual ULONG STDMETHODCALLTYPE AddRef() OVERRIDE {
    return IUnknownImpl::AddRef();
  }

  virtual ULONG STDMETHODCALLTYPE Release() OVERRIDE {
    return IUnknownImpl::Release();
  }

  virtual STDMETHODIMP QueryInterface(REFIID riid, void** ppv) OVERRIDE {
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
                       IPortableDeviceValues* event_data) OVERRIDE {
    return S_OK;
  }

  STDMETHODIMP OnLeave(REFSENSOR_ID sensor_id) OVERRIDE {
    return S_OK;
  }

  STDMETHODIMP OnStateChanged(ISensor* sensor, SensorState state) OVERRIDE {
    return S_OK;
  }

  void GetSensorValue(REFPROPERTYKEY property, ISensorDataReport* new_data,
      double* value, bool* has_value) {
    PROPVARIANT variant_value = {};
    if (SUCCEEDED(new_data->GetSensorValue(property, &variant_value))) {
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
  explicit SensorEventSinkOrientation(DeviceOrientationHardwareBuffer* buffer)
      : buffer_(buffer) {}
  virtual ~SensorEventSinkOrientation() {}

  STDMETHODIMP OnDataUpdated(ISensor* sensor,
                             ISensorDataReport* new_data) OVERRIDE {
    if (NULL == new_data || NULL == sensor)
      return E_INVALIDARG;

    double alpha, beta, gamma;
    bool has_alpha, has_beta, has_gamma;

    GetSensorValue(SENSOR_DATA_TYPE_TILT_X_DEGREES, new_data, &alpha,
        &has_alpha);
    GetSensorValue(SENSOR_DATA_TYPE_TILT_Y_DEGREES, new_data, &beta,
        &has_beta);
    GetSensorValue(SENSOR_DATA_TYPE_TILT_Z_DEGREES, new_data, &gamma,
        &has_gamma);

    if (buffer_) {
      buffer_->seqlock.WriteBegin();
      buffer_->data.alpha = alpha;
      buffer_->data.hasAlpha = has_alpha;
      buffer_->data.beta = beta;
      buffer_->data.hasBeta = has_beta;
      buffer_->data.gamma = gamma;
      buffer_->data.hasGamma = has_gamma;
      buffer_->data.absolute = true;
      buffer_->data.hasAbsolute = has_alpha || has_beta || has_gamma;
      buffer_->data.allAvailableSensorsAreActive = true;
      buffer_->seqlock.WriteEnd();
    }

    return S_OK;
  }

 private:
  DeviceOrientationHardwareBuffer* buffer_;

  DISALLOW_COPY_AND_ASSIGN(SensorEventSinkOrientation);
};

class DataFetcherSharedMemory::SensorEventSinkMotion
    : public DataFetcherSharedMemory::SensorEventSink {
 public:
  explicit SensorEventSinkMotion(DeviceMotionHardwareBuffer* buffer)
      : buffer_(buffer) {}
  virtual ~SensorEventSinkMotion() {}

  STDMETHODIMP OnDataUpdated(ISensor* sensor,
                             ISensorDataReport* new_data) OVERRIDE {
    if (NULL == new_data || NULL == sensor)
      return E_INVALIDARG;

    SENSOR_TYPE_ID sensor_type = GUID_NULL;
    if (!SUCCEEDED(sensor->GetType(&sensor_type)))
      return E_INVALIDARG;

    if (IsEqualIID(sensor_type, SENSOR_TYPE_ACCELEROMETER_3D)) {
      double accelerationX, accelerationY, accelerationZ;
      bool has_accelerationX, has_accelerationY, has_accelerationZ;

      GetSensorValue(SENSOR_DATA_TYPE_ACCELERATION_X_G, new_data,
          &accelerationX, &has_accelerationX);
      GetSensorValue(SENSOR_DATA_TYPE_ACCELERATION_Y_G, new_data,
          &accelerationY, &has_accelerationY);
      GetSensorValue(SENSOR_DATA_TYPE_ACCELERATION_Z_G, new_data,
          &accelerationZ, &has_accelerationZ);

      buffer_->seqlock.WriteBegin();
      buffer_->data.accelerationX = accelerationX;
      buffer_->data.hasAccelerationX = has_accelerationX;
      buffer_->data.accelerationY = accelerationY;
      buffer_->data.hasAccelerationY = has_accelerationY;
      buffer_->data.accelerationZ = accelerationZ;
      buffer_->data.hasAccelerationZ = has_accelerationZ;
      // TODO(timvolodine): consider setting this to true after
      // all sensors have fired.
      buffer_->data.allAvailableSensorsAreActive = true;
      buffer_->seqlock.WriteEnd();

    } else if (IsEqualIID(sensor_type, SENSOR_TYPE_GYROMETER_3D)) {
      double alpha, beta, gamma;
      bool has_alpha, has_beta, has_gamma;

      GetSensorValue(SENSOR_DATA_TYPE_ANGULAR_VELOCITY_X_DEGREES_PER_SECOND,
          new_data, &alpha, &has_alpha);
      GetSensorValue(SENSOR_DATA_TYPE_ANGULAR_VELOCITY_Y_DEGREES_PER_SECOND,
          new_data, &beta, &has_beta);
      GetSensorValue(SENSOR_DATA_TYPE_ANGULAR_VELOCITY_Z_DEGREES_PER_SECOND,
          new_data, &gamma, &has_gamma);

      buffer_->seqlock.WriteBegin();
      buffer_->data.rotationRateAlpha = alpha;
      buffer_->data.hasRotationRateAlpha = true;
      buffer_->data.rotationRateBeta = beta;
      buffer_->data.hasRotationRateBeta = true;
      buffer_->data.rotationRateGamma = gamma;
      buffer_->data.hasRotationRateGamma = true;
      buffer_->data.allAvailableSensorsAreActive = true;
      buffer_->seqlock.WriteEnd();
    }

    return S_OK;
  }

  private:
   DeviceMotionHardwareBuffer* buffer_;

   DISALLOW_COPY_AND_ASSIGN(SensorEventSinkMotion);
 };


DataFetcherSharedMemory::DataFetcherSharedMemory()
    : motion_buffer_(NULL),
      orientation_buffer_(NULL) {
}

DataFetcherSharedMemory::~DataFetcherSharedMemory() {
}

bool DataFetcherSharedMemory::Start(ConsumerType consumer_type, void* buffer) {
  DCHECK(buffer);

  switch (consumer_type) {
    case CONSUMER_TYPE_ORIENTATION:
      {
        orientation_buffer_ =
            static_cast<DeviceOrientationHardwareBuffer*>(buffer);
        scoped_refptr<SensorEventSink> sink(
            new SensorEventSinkOrientation(orientation_buffer_));
        if (RegisterForSensor(SENSOR_TYPE_INCLINOMETER_3D,
            sensor_inclinometer_.Receive(), sink))
          return true;
        // if no sensors are available set buffer to ready, to fire null-events.
        SetBufferAvailableState(consumer_type, true);
      }
      break;
    case CONSUMER_TYPE_MOTION:
      {
        motion_buffer_ = static_cast<DeviceMotionHardwareBuffer*>(buffer);
        scoped_refptr<SensorEventSink> sink(
            new SensorEventSinkMotion(motion_buffer_));
        bool accelerometer_available = RegisterForSensor(
            SENSOR_TYPE_ACCELEROMETER_3D, sensor_accelerometer_.Receive(),
            sink);
        bool gyrometer_available = RegisterForSensor(
            SENSOR_TYPE_GYROMETER_3D, sensor_gyrometer_.Receive(), sink);
        if (accelerometer_available || gyrometer_available)
          return true;
        // if no sensors are available set buffer to ready, to fire null-events.
        SetBufferAvailableState(consumer_type, true);
      }
      break;
    default:
      NOTREACHED();
  }
  return false;
}

bool DataFetcherSharedMemory::Stop(ConsumerType consumer_type) {
  switch (consumer_type) {
    case CONSUMER_TYPE_ORIENTATION:
      if (sensor_inclinometer_)
        sensor_inclinometer_->SetEventSink(NULL);
      SetBufferAvailableState(consumer_type, false);
      orientation_buffer_ = NULL;
      return true;
    case CONSUMER_TYPE_MOTION:
      if (sensor_accelerometer_)
        sensor_accelerometer_->SetEventSink(NULL);
      if (sensor_gyrometer_)
        sensor_gyrometer_->SetEventSink(NULL);
      SetBufferAvailableState(consumer_type, false);
      motion_buffer_ = NULL;
      return true;
    default:
      NOTREACHED();
  }
  return false;
}

void DataFetcherSharedMemory::SetBufferAvailableState(
    ConsumerType consumer_type, bool enabled) {
  switch(consumer_type) {
    case CONSUMER_TYPE_ORIENTATION:
      if (orientation_buffer_) {
        orientation_buffer_->seqlock.WriteBegin();
        orientation_buffer_->data.allAvailableSensorsAreActive = enabled;
        orientation_buffer_->seqlock.WriteEnd();
      }
    case CONSUMER_TYPE_MOTION:
      if (motion_buffer_) {
        motion_buffer_->seqlock.WriteBegin();
        motion_buffer_->data.allAvailableSensorsAreActive = enabled;
        motion_buffer_->seqlock.WriteEnd();
      }
    default:
      NOTREACHED();
  }
}

bool DataFetcherSharedMemory::RegisterForSensor(
    REFSENSOR_TYPE_ID sensor_type,
    ISensor** sensor,
    scoped_refptr<SensorEventSink> event_sink) {
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return false;

  base::win::ScopedComPtr<ISensorManager> sensor_manager;
  HRESULT hr = sensor_manager.CreateInstance(CLSID_SensorManager);
  if (FAILED(hr) || !sensor_manager)
    return false;

  base::win::ScopedComPtr<ISensorCollection> sensor_collection;
  hr = sensor_manager->GetSensorsByType(
      sensor_type, sensor_collection.Receive());

  if (FAILED(hr) || !sensor_collection)
    return false;

  ULONG count = 0;
  hr = sensor_collection->GetCount(&count);
  if (FAILED(hr) || !count)
    return false;

  hr = sensor_collection->GetAt(0, sensor);
  if (FAILED(hr) || !sensor)
    return false;

  base::win::ScopedComPtr<IPortableDeviceValues> device_values;
  if (SUCCEEDED(device_values.CreateInstance(CLSID_PortableDeviceValues))) {
    if (SUCCEEDED(device_values->SetUnsignedIntegerValue(
        SENSOR_PROPERTY_CURRENT_REPORT_INTERVAL, kPeriodInMilliseconds))) {
      base::win::ScopedComPtr<IPortableDeviceValues> return_values;
      (*sensor)->SetProperties(device_values.get(), return_values.Receive());
    }
  }

  base::win::ScopedComPtr<ISensorEvents> sensor_events;
  hr = event_sink->QueryInterface(
      __uuidof(ISensorEvents), sensor_events.ReceiveVoid());
  if (FAILED(hr) || !sensor_events)
    return false;

  hr = (*sensor)->SetEventSink(sensor_events);
  if (FAILED(hr))
    return false;

  return true;
}

}  // namespace content
