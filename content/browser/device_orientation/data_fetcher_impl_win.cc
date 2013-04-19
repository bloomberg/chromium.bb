// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/data_fetcher_impl_win.h"

#include <InitGuid.h>
#include <PortableDeviceTypes.h>
#include <Sensors.h>

#include "base/logging.h"
#include "base/win/iunknown_impl.h"
#include "base/win/windows_version.h"
#include "content/browser/device_orientation/orientation.h"

namespace {

// This should match ProviderImpl::kDesiredSamplingIntervalMs.
const int kPeriodInMilliseconds = 100;

}  // namespace

namespace content {

class DataFetcherImplWin::SensorEventSink : public ISensorEvents,
                                            public base::win::IUnknownImpl {
 public:
  explicit SensorEventSink(DataFetcherImplWin* const fetcher)
      : fetcher_(fetcher) {}

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
    scoped_refptr<Orientation> orientation = new Orientation();

    if (SUCCEEDED(new_data->GetSensorValue(
            SENSOR_DATA_TYPE_TILT_X_DEGREES, &value))) {
      orientation->set_beta(value.fltVal);
    }
    PropVariantClear(&value);

    if (SUCCEEDED(new_data->GetSensorValue(
            SENSOR_DATA_TYPE_TILT_Y_DEGREES, &value))) {
      orientation->set_gamma(value.fltVal);
    }
    PropVariantClear(&value);

    if (SUCCEEDED(new_data->GetSensorValue(
            SENSOR_DATA_TYPE_TILT_Z_DEGREES, &value))) {
      orientation->set_alpha(value.fltVal);
    }
    PropVariantClear(&value);

    orientation->set_absolute(true);
    fetcher_->OnOrientationData(orientation.get());

    return S_OK;
  }

  STDMETHODIMP OnLeave(REFSENSOR_ID sensor_id) OVERRIDE {
    return S_OK;
  }

  STDMETHODIMP OnStateChanged(ISensor* sensor, SensorState state) OVERRIDE {
    return S_OK;
  }

 private:
  DataFetcherImplWin* const fetcher_;

  DISALLOW_COPY_AND_ASSIGN(SensorEventSink);
};

// Create a DataFetcherImplWin object and return NULL if no valid sensor found.
// static
DataFetcher* DataFetcherImplWin::Create() {
  scoped_ptr<DataFetcherImplWin> fetcher(new DataFetcherImplWin);
  if (fetcher->Initialize())
    return fetcher.release();

  LOG(ERROR) << "DataFetcherImplWin::Initialize failed!";
  return NULL;
}

DataFetcherImplWin::~DataFetcherImplWin() {
  if (sensor_)
    sensor_->SetEventSink(NULL);
}

DataFetcherImplWin::DataFetcherImplWin() {
}

void DataFetcherImplWin::OnOrientationData(Orientation* orientation) {
  // This method is called on Windows sensor thread.
  base::AutoLock autolock(next_orientation_lock_);
  next_orientation_ = orientation;
}

const DeviceData* DataFetcherImplWin::GetDeviceData(DeviceData::Type type) {
  if (type != DeviceData::kTypeOrientation)
    return NULL;
  return GetOrientation();
}

const Orientation* DataFetcherImplWin::GetOrientation() {
  if (next_orientation_.get()) {
    base::AutoLock autolock(next_orientation_lock_);
    next_orientation_.swap(current_orientation_);
  }
  if (!current_orientation_.get())
    return new Orientation();
  return current_orientation_.get();
}

bool DataFetcherImplWin::Initialize() {
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return false;

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

  scoped_refptr<SensorEventSink> sensor_event_impl(new SensorEventSink(this));
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

}  // namespace content
