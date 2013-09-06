// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "data_fetcher_shared_memory.h"

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
  explicit SensorEventSink(DeviceOrientationHardwareBuffer* buffer)
      : buffer_(buffer) {}

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

  STDMETHODIMP OnDataUpdated(ISensor* sensor,
                             ISensorDataReport* new_data) OVERRIDE {
    if (NULL == new_data || NULL == sensor)
      return E_INVALIDARG;

    PROPVARIANT value = {};
    double alpha = 0;
    bool has_alpha = false;
    double beta = 0;
    bool has_beta = false;
    double gamma = 0;
    bool has_gamma = false;

    if (SUCCEEDED(new_data->GetSensorValue(
        SENSOR_DATA_TYPE_TILT_X_DEGREES, &value))) {
      beta = value.fltVal;
      has_beta = true;
    }
    PropVariantClear(&value);

    if (SUCCEEDED(new_data->GetSensorValue(
        SENSOR_DATA_TYPE_TILT_Y_DEGREES, &value))) {
      gamma = value.fltVal;
      has_gamma = true;
    }
    PropVariantClear(&value);

    if (SUCCEEDED(new_data->GetSensorValue(
        SENSOR_DATA_TYPE_TILT_Z_DEGREES, &value))) {
      alpha = value.fltVal;
      has_alpha = true;
    }
    PropVariantClear(&value);

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

  STDMETHODIMP OnLeave(REFSENSOR_ID sensor_id) OVERRIDE {
    return S_OK;
  }

  STDMETHODIMP OnStateChanged(ISensor* sensor, SensorState state) OVERRIDE {
    return S_OK;
  }

 private:
  DeviceOrientationHardwareBuffer* buffer_;

  DISALLOW_COPY_AND_ASSIGN(SensorEventSink);
};


DataFetcherSharedMemory::DataFetcherSharedMemory()
    : motion_buffer_(NULL),
      orientation_buffer_(NULL) {
}

DataFetcherSharedMemory::~DataFetcherSharedMemory() {
}

bool DataFetcherSharedMemory::Start(ConsumerType consumer_type, void* buffer) {
  DCHECK(buffer);

  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return false;

  if (consumer_type != CONSUMER_TYPE_ORIENTATION)
    return false;

  orientation_buffer_ = static_cast<DeviceOrientationHardwareBuffer*>(buffer);
  DCHECK(orientation_buffer_);

  base::win::ScopedComPtr<ISensorManager> sensor_manager;
  HRESULT hr = sensor_manager.CreateInstance(CLSID_SensorManager);
  if (FAILED(hr) || !sensor_manager)
    return false;

  base::win::ScopedComPtr<ISensorCollection> sensor_collection;
  hr = sensor_manager->GetSensorsByType(
      SENSOR_TYPE_INCLINOMETER_3D, sensor_collection.Receive());

  if (FAILED(hr) || !sensor_collection)
    return false;

  ULONG count = 0;
  hr = sensor_collection->GetCount(&count);
  if (FAILED(hr) || !count)
    return false;

  hr = sensor_collection->GetAt(0, sensor_.Receive());
  if (FAILED(hr) || !sensor_)
    return false;

  base::win::ScopedComPtr<IPortableDeviceValues> device_values;
  if (SUCCEEDED(device_values.CreateInstance(CLSID_PortableDeviceValues))) {
    if (SUCCEEDED(device_values->SetUnsignedIntegerValue(
        SENSOR_PROPERTY_CURRENT_REPORT_INTERVAL, kPeriodInMilliseconds))) {
      base::win::ScopedComPtr<IPortableDeviceValues> return_values;
      sensor_->SetProperties(device_values.get(), return_values.Receive());
    }
  }

  scoped_refptr<SensorEventSink> sensor_event_impl(new SensorEventSink(
      orientation_buffer_));

  base::win::ScopedComPtr<ISensorEvents> sensor_events;
  hr = sensor_event_impl->QueryInterface(
      __uuidof(ISensorEvents), sensor_events.ReceiveVoid());
  if (FAILED(hr) || !sensor_events)
    return false;

  hr = sensor_->SetEventSink(sensor_events);
  if (FAILED(hr))
    return false;

  return true;
}

bool DataFetcherSharedMemory::Stop(ConsumerType consumer_type) {
  if (consumer_type != CONSUMER_TYPE_ORIENTATION)
    return false;

  if (sensor_)
    sensor_->SetEventSink(NULL);

  if (orientation_buffer_) {
    orientation_buffer_->seqlock.WriteBegin();
    orientation_buffer_->data.allAvailableSensorsAreActive = false;
    orientation_buffer_->seqlock.WriteEnd();
    orientation_buffer_ = NULL;
  }

  return true;
}


}  // namespace content

