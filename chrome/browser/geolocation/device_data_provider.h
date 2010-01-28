// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A device data provider provides data from the device that is used by a
// NetworkLocationProvider to obtain a position fix. This data may be either
// cell radio data or wifi data. For a given type of data, we use a singleton
// instance of the device data provider, which is used by multiple
// NetworkLocationProvider objects.
//
// This file providers DeviceDataProvider, which provides static methods to
// access the singleton instance. The singleton instance uses a private
// implementation to abstract across platforms and also to allow mock providers
// to be used for testing.
//
// This file also provides DeviceDataProviderImplBase, a base class which
// provides commom functionality for the private implementations.
//
// This file also declares the data structures used to represent cell radio data
// and wifi data.

#ifndef CHROME_BROWSER_GEOLOCATION_DEVICE_DATA_PROVIDER_H_
#define CHROME_BROWSER_GEOLOCATION_DEVICE_DATA_PROVIDER_H_

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/scoped_ptr.h"

// The following data structures are used to store cell radio data and wifi
// data. See the Geolocation API design document at
// http://code.google.com/p/google-gears/wiki/LocationAPI for a more complete
// description.
//
// For all integer fields, we use kint32min to represent unknown values.

// Cell radio data relating to a single cell tower.
struct CellData {
  CellData()
      : cell_id(kint32min),
        location_area_code(kint32min),
        mobile_network_code(kint32min),
        mobile_country_code(kint32min),
        age(kint32min),
        radio_signal_strength(kint32min),
        timing_advance(kint32min) {}
  bool Matches(const CellData &other) const {
    // Ignore age and radio_signal_strength when matching.
    return cell_id == other.cell_id &&
           location_area_code == other.location_area_code &&
           mobile_network_code == other.mobile_network_code &&
           mobile_country_code == other.mobile_country_code &&
           timing_advance == other.timing_advance;
  }

  int cell_id;                // Unique identifier of the cell
  int location_area_code;     // For current location area
  int mobile_network_code;    // For current cell
  int mobile_country_code;    // For current cell
  int age;                    // Milliseconds since this cell was primary
  int radio_signal_strength;  // Measured in dBm.
  int timing_advance;         // Timing advance representing the distance from
                              // the cell tower. Each unit is roughly 550
                              // meters.
};

static bool CellDataMatches(const CellData &data1, const CellData &data2) {
  return data1.Matches(data2);
}

enum RadioType {
  RADIO_TYPE_UNKNOWN,
  RADIO_TYPE_GSM,
  RADIO_TYPE_CDMA,
  RADIO_TYPE_WCDMA,
};

// All data for the cell radio.
struct RadioData {
  RadioData()
      : home_mobile_network_code(kint32min),
        home_mobile_country_code(kint32min),
        radio_type(RADIO_TYPE_UNKNOWN) {}
  bool Matches(const RadioData &other) const {
    if (cell_data.size() != other.cell_data.size()) {
      return false;
    }
    if (!std::equal(cell_data.begin(), cell_data.end(), other.cell_data.begin(),
                    CellDataMatches)) {
      return false;
    }
    return device_id == other.device_id &&
           home_mobile_network_code == other.home_mobile_network_code &&
           home_mobile_country_code == other.home_mobile_country_code &&
           radio_type == other.radio_type &&
           carrier == other.carrier;
  }
  // Determines whether a new set of radio data differs significantly from this.
  bool DiffersSignificantly(const RadioData &other) const {
    // This is required by MockDeviceDataProviderImpl.
    // TODO(steveblock): Implement properly.
    return !Matches(other);
  }

  string16 device_id;
  std::vector<CellData> cell_data;
  int home_mobile_network_code;  // For the device's home network.
  int home_mobile_country_code;  // For the device's home network.
  RadioType radio_type;          // Mobile radio type.
  string16 carrier;         // Carrier name.
};

// Wifi data relating to a single access point.
struct AccessPointData {
  AccessPointData()
      : radio_signal_strength(kint32min),
        age(kint32min),
        channel(kint32min),
        signal_to_noise(kint32min) {}

  string16 mac_address;
  int radio_signal_strength;  // Measured in dBm
  int age;              // Milliseconds since this access point was detected
  int channel;
  int signal_to_noise;  // Ratio in dB
  string16 ssid;   // Network identifier
};

// This is to allow AccessPointData to be used in std::set. We order
// lexicographically by MAC address.
struct AccessPointDataLess : public std::less<AccessPointData> {
  bool operator()(const AccessPointData& data1,
                  const AccessPointData& data2) const {
    return data1.mac_address < data2.mac_address;
  }
};

// All data for wifi.
struct WifiData {
  // Determines whether a new set of WiFi data differs significantly from this.
  bool DiffersSignificantly(const WifiData &other) const {
    // At least 5 or 50% of access points added or removed is significant.
    static const size_t kMinChangedAccessPoints = 5;

    // Compute size of interesction of old and new sets.
    size_t num_common = 0;
    for (AccessPointDataSet::const_iterator iter = access_point_data.begin();
         iter != access_point_data.end();
         iter++) {
      if (other.access_point_data.find(*iter) !=
          other.access_point_data.end()) {
        ++num_common;
      }
    }
    assert(num_common <= access_point_data.size());
    assert(num_common <= other.access_point_data.size());

    // Test how many have changed.
    size_t added_or_removed = std::max(
        other.access_point_data.size() - num_common,
        access_point_data.size() - num_common);
    return added_or_removed >=
        std::min(kMinChangedAccessPoints, access_point_data.size() / 2);
  }

  // Store access points as a set, sorted by MAC address. This allows quick
  // comparison of sets for detecting changes and for caching.
  typedef std::set<AccessPointData, AccessPointDataLess> AccessPointDataSet;
  AccessPointDataSet access_point_data;
};

template<typename DataType>
class DeviceDataProvider;

// DeviceDataProvider uses containment to hide platform-specific implementation
// details from common code. This class provides common functionality for these
// contained implementation classes.
template<typename DataType>
class DeviceDataProviderImplBase {
 public:
  DeviceDataProviderImplBase() : container_(NULL) {}
  virtual ~DeviceDataProviderImplBase() {}

  virtual bool StartDataProvider() = 0;

  virtual bool GetData(DataType* data) = 0;

  // Sets the container of this class, which is of type DeviceDataProvider.
  // This is required to pass as a parameter when making the callback to
  // listeners.
  void SetContainer(DeviceDataProvider<DataType>* container) {
    container_ = container;
  }

  typedef typename DeviceDataProvider<DataType>::ListenerInterface
          ListenerInterface;
  void AddListener(ListenerInterface* listener) {
    AutoLock mutex(listeners_mutex_);
    listeners_.insert(listener);
  }
  bool RemoveListener(ListenerInterface* listener) {
    AutoLock mutex(listeners_mutex_);
    typename ListenersSet::iterator iter = find(listeners_.begin(),
                                                listeners_.end(),
                                                listener);
    if (iter == listeners_.end()) {
      return false;
    }
    listeners_.erase(iter);
    return true;
  }

 protected:
  // Calls DeviceDataUpdateAvailable() on all registered listeners.
  typedef std::set<ListenerInterface*> ListenersSet;
  void NotifyListeners() {
    AutoLock lock(listeners_mutex_);
    for (typename ListenersSet::const_iterator iter = listeners_.begin();
         iter != listeners_.end();
         ++iter) {
      (*iter)->DeviceDataUpdateAvailable(container_);
    }
  }

 private:
  DeviceDataProvider<DataType>* container_;

  // The listeners to this class and their mutex.
  // TODO(joth): Once we've established the client is always single threaded,
  // remove mutex and instead capture client's MessageLoop to stage the
  // NotifyListeners callback via.
  ListenersSet listeners_;
  Lock listeners_mutex_;

  DISALLOW_COPY_AND_ASSIGN(DeviceDataProviderImplBase);
};

typedef DeviceDataProviderImplBase<RadioData> RadioDataProviderImplBase;
typedef DeviceDataProviderImplBase<WifiData> WifiDataProviderImplBase;

// A device data provider
//
// We use a singleton instance of this class which is shared by multiple network
// location providers. These location providers access the instance through the
// Register and Unregister methods.
template<typename DataType>
class DeviceDataProvider {
 public:
  // Interface to be implemented by listeners to a device data provider.
  class ListenerInterface {
   public:
    // NOTE this may be called back in the context of the implementation private
    // worker thread. (TODO Is there a naming convention to use for this?)
    virtual void DeviceDataUpdateAvailable(
        DeviceDataProvider<DataType>* provider) = 0;
    virtual ~ListenerInterface() {}
  };

  // Sets the factory function which will be used by Register to create the
  // implementation used by the singleton instance. This factory approach is
  // used to abastract accross both platform-specific implementation and to
  // inject mock implementations for testing.
  typedef DeviceDataProviderImplBase<DataType>* (*ImplFactoryFunction)(void);
  static void SetFactory(ImplFactoryFunction factory_function_in) {
    factory_function_ = factory_function_in;
  }

  static void ResetFactory() {
    factory_function_ = DefaultFactoryFunction;
  }

  // Adds a listener, which will be called back with DeviceDataUpdateAvailable
  // whenever new data is available. Returns the singleton instance.
  static DeviceDataProvider* Register(ListenerInterface* listener) {
    // TODO(joth): The following comment applied when this was used in Gears;
    // revisit if this is still needed once usage is established in Chromium.
    // We protect against Register and Unregister being called asynchronously
    // from different threads. This is the case when a device data provider is
    // used by a NetworkLocationProvider object. Register is always called from
    // the JavaScript thread. Unregister is called when NetworkLocationProvider
    // objects are destructed, which happens asynchronously once the
    // NetworkLocationProvider HTTP request has completed.
    AutoLock mutex(instance_mutex_);
    if (!instance_) {
      instance_ = new DeviceDataProvider();
    }
    assert(instance_);
    instance_->Ref();
    instance_->AddListener(listener);
    return instance_;
  }

  // Removes a listener. If this is the last listener, deletes the singleton
  // instance. Return value indicates success.
  static bool Unregister(ListenerInterface* listener) {
    AutoLock mutex(instance_mutex_);
    if (!instance_->RemoveListener(listener)) {
      return false;
    }
    if (instance_->Unref()) {
      delete instance_;
      instance_ = NULL;
    }
    return true;
  }

  // Provides whatever data the provider has, which may be nothing. Return
  // value indicates whether this is all the data the provider could ever
  // obtain.
  bool GetData(DataType* data) {
    return impl_->GetData(data);
  }

 private:
  // Private constructor and destructor, callers access singleton through
  // Register and Unregister.
  DeviceDataProvider() : count_(0) {
    assert(factory_function_);
    impl_.reset((*factory_function_)());
    impl_->SetContainer(this);
    bool started = impl_->StartDataProvider();
    assert(started);
  }
  virtual ~DeviceDataProvider() {}

  void Ref() {
    ++count_;
  }
  // Returns true when the ref count transitions from 1 to 0.
  bool Unref() {
    --count_;
    return count_ == 0;
  }

  void AddListener(ListenerInterface* listener) {
    impl_->AddListener(listener);
  }

  bool RemoveListener(ListenerInterface* listener) {
    return impl_->RemoveListener(listener);
  }

  static DeviceDataProviderImplBase<DataType>* DefaultFactoryFunction();

  // The singleton instance of this class and its mutex.
  static DeviceDataProvider* instance_;
  static Lock instance_mutex_;

  // The factory function used to create the singleton instance.
  static ImplFactoryFunction factory_function_;

  // The internal implementation.
  scoped_ptr<DeviceDataProviderImplBase<DataType> > impl_;

  int count_;

  DISALLOW_EVIL_CONSTRUCTORS(DeviceDataProvider);
};

// static
template<typename DataType>
Lock DeviceDataProvider<DataType>::instance_mutex_;

// static
template<typename DataType>
DeviceDataProvider<DataType>* DeviceDataProvider<DataType>::instance_ = NULL;

// static
template<typename DataType>
typename DeviceDataProvider<DataType>::ImplFactoryFunction
    DeviceDataProvider<DataType>::factory_function_ = DefaultFactoryFunction;

typedef DeviceDataProvider<RadioData> RadioDataProvider;
typedef DeviceDataProvider<WifiData> WifiDataProvider;

#endif  // CHROME_BROWSER_GEOLOCATION_DEVICE_DATA_PROVIDER_H_
