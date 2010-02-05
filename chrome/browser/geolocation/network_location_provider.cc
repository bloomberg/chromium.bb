// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/network_location_provider.h"

#include "base/time.h"

namespace {
// The maximum period of time we'll wait for a complete set of device data
// before sending the request.
const int kDataCompleteWaitPeriod = 1000 * 2;  // 2 seconds

// The maximum size of the cache of positions for previously requested device
// data.
const size_t kMaximumCacheSize = 10;

// TODO(joth): Share, or remove usage by porting callers to Time et al.
int64 GetCurrentTimeMillis() {
  return static_cast<int64>(base::Time::Now().ToDoubleT() * 1000);
}
}  // namespace

// The PositionCache handles caching and retrieving a position returned by a
// network location provider. It is not thread safe. It's methods are called on
// multiple threads by NetworkLocationProvider, but the timing is such that
// thread safety is not required.
class NetworkLocationProvider::PositionCache {
 public:
  // Caches the current position response for the current set of cell ID and
  // WiFi data. Returns true on success, false otherwise.
  bool CachePosition(const RadioData& radio_data,
                     const WifiData& wifi_data,
                     const Position& position) {
    // Check that we can generate a valid key for the device data.
    string16 key;
    if (!MakeKey(radio_data, wifi_data, &key)) {
      return false;
    }
    // If the cache is full, remove the oldest entry.
    if (cache_.size() == kMaximumCacheSize) {
      DCHECK(cache_times_.size() == kMaximumCacheSize);
      CacheTimesMap::iterator oldest_entry = cache_times_.begin();
      DCHECK(oldest_entry != cache_times_.end());
      cache_.erase(oldest_entry->second);
      cache_times_.erase(oldest_entry);
    }
    // Insert the position into the cache.
    std::pair<CacheMap::iterator, bool> result =
        cache_.insert(std::make_pair(key, position));
    DCHECK(result.second);
    cache_times_[position.timestamp] = result.first;
    DCHECK(cache_.size() == cache_times_.size());
    return true;
  }

  // Searches for a cached position response for the current set of cell ID and
  // WiFi data. Returns the cached position if available, NULL otherwise.
  const Position *FindPosition(const RadioData &radio_data,
                               const WifiData &wifi_data) {
    string16 key;
    if (!MakeKey(radio_data, wifi_data, &key)) {
      return NULL;
    }
    CacheMap::const_iterator iter = cache_.find(key);
    return iter == cache_.end() ? NULL : &iter->second;
  }

  // Makes the key for the map of cached positions, using a set of
  // device data. Returns true if a good key was generated, false otherwise.
  static bool MakeKey(const RadioData& /*radio_data*/,
                      const WifiData& wifi_data,
                      string16* key) {
    // Currently we use only the WiFi data, and base the key only on the MAC
    // addresses.
    // TODO(steveblock): Make use of radio_data.
    DCHECK(key);
    key->clear();
    key->reserve(wifi_data.access_point_data.size() * 19);
    const string16 separator(ASCIIToUTF16("|"));
    for (WifiData::AccessPointDataSet::const_iterator iter =
         wifi_data.access_point_data.begin();
         iter != wifi_data.access_point_data.begin();
         iter++) {
      *key += separator;
      *key += iter->mac_address;
      *key += separator;
    }
    // If the key is the empty string, return false, as we don't want to cache a
    // position for such a set of device data.
    return !key->empty();
  }

 private:
  // The cache of positions. This is stored using two maps. One map is keyed on
  // a string that represents a set of device data, the other is keyed on the
  // timestamp of the position.
  typedef std::map<string16, Position> CacheMap;
  CacheMap cache_;
  typedef std::map<int64, CacheMap::iterator> CacheTimesMap;
  CacheTimesMap cache_times_;
};

// NetworkLocationProvider factory function
LocationProviderBase* NewNetworkLocationProvider(
    LocationProviderBase::AccessTokenStore* access_token_store,
    URLRequestContextGetter* context,
    const GURL& url,
    const string16& host_name) {
  return new NetworkLocationProvider(access_token_store, context,
                                     url, host_name);
}

// NetworkLocationProvider
NetworkLocationProvider::NetworkLocationProvider(
    AccessTokenStore* access_token_store,
    URLRequestContextGetter* url_context_getter,
    const GURL& url,
    const string16& host_name)
    : access_token_store_(access_token_store),
      radio_data_provider_(NULL),
      wifi_data_provider_(NULL),
      is_radio_data_complete_(false),
      is_wifi_data_complete_(false),
      device_data_updated_timestamp_(kint64min),
      is_new_data_available_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(delayed_start_task_(this)) {
  // Create the position cache.
  position_cache_.reset(new PositionCache());

  request_.reset(new NetworkLocationRequest(url_context_getter, url,
                                            host_name, this));
}

NetworkLocationProvider::~NetworkLocationProvider() {
  if (radio_data_provider_)
    radio_data_provider_->Unregister(this);
  if (wifi_data_provider_)
    wifi_data_provider_->Unregister(this);
}

// LocationProviderBase implementation
void NetworkLocationProvider::GetPosition(Position *position) {
  DCHECK(position);
  AutoLock lock(position_mutex_);
  *position = position_;
}

void NetworkLocationProvider::UpdatePosition() {
  if (is_new_data_available_) {
    RequestPosition();
  }
}

// DeviceDataProviderInterface::ListenerInterface implementation.
void NetworkLocationProvider::DeviceDataUpdateAvailable(
    RadioDataProvider* provider) {
  {
    AutoLock lock(data_mutex_);
    DCHECK(provider == radio_data_provider_);
    is_radio_data_complete_ = radio_data_provider_->GetData(&radio_data_);
  }
  OnDeviceDataUpdated();
}

void NetworkLocationProvider::DeviceDataUpdateAvailable(
    WifiDataProvider* provider) {
  {
    AutoLock lock(data_mutex_);
    DCHECK(provider == wifi_data_provider_);
    is_wifi_data_complete_ = wifi_data_provider_->GetData(&wifi_data_);
  }
  OnDeviceDataUpdated();
}

// NetworkLocationRequest::ListenerInterface implementation.
void NetworkLocationProvider::LocationResponseAvailable(
    const Position& position,
    bool server_error,
    const string16& access_token) {
  CheckRunningInClientLoop();
  // Record the position and update our cache.
  {
    AutoLock position_lock(position_mutex_);
    position_ = position;
  }
  if (position.IsValidFix()) {
    AutoLock lock(data_mutex_);
    position_cache_->CachePosition(radio_data_, wifi_data_, position);
  }

  // Record access_token if it's set.
  if (!access_token.empty() && access_token_ != access_token) {
    access_token_ = access_token;
    access_token_store_->SetAccessToken(request_->url(), access_token);
  }

  // If new data arrived whilst request was pending reissue the request.
  UpdatePosition();
  // Let listeners know that we now have a position available.
  UpdateListeners();
}

bool NetworkLocationProvider::StartProvider() {
  CheckRunningInClientLoop();
  DCHECK(radio_data_provider_ == NULL);
  DCHECK(wifi_data_provider_ == NULL);
  if (!request_->url().is_valid()) {
    LOG(WARNING) << "StartProvider() : Failed, Bad URL: "
                 << request_->url().possibly_invalid_spec();
    return false;
  }

  // Get the device data providers. The first call to Register will create the
  // provider and it will be deleted by ref counting.
  radio_data_provider_ = RadioDataProvider::Register(this);
  wifi_data_provider_ = WifiDataProvider::Register(this);

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      delayed_start_task_.NewRunnableMethod(
          &NetworkLocationProvider::RequestPosition),
      kDataCompleteWaitPeriod);
  {
  // Get the device data.
  AutoLock lock(data_mutex_);
  is_radio_data_complete_ = radio_data_provider_->GetData(&radio_data_);
  is_wifi_data_complete_ = wifi_data_provider_->GetData(&wifi_data_);
  }
  if (is_radio_data_complete_ || is_wifi_data_complete_)
    OnDeviceDataUpdated();
  return true;
}

// Other methods
void NetworkLocationProvider::RequestPosition() {
  CheckRunningInClientLoop();

  delayed_start_task_.RevokeAll();
  const Position* cached_position;
  {
    AutoLock lock(data_mutex_);
    cached_position = position_cache_->FindPosition(radio_data_, wifi_data_);
  }
  DCHECK_NE(device_data_updated_timestamp_, kint64min) <<
    "Timestamp must be set before looking up position";
  if (cached_position) {
    DCHECK(cached_position->IsValidFix());
    // Record the position and update its timestamp.
    {
      AutoLock lock(position_mutex_);
      position_ = *cached_position;
      // The timestamp of a position fix is determined by the timestamp
      // of the source data update. (The value of position_.timestamp from
      // the cache could be from weeks ago!)
      position_.timestamp = device_data_updated_timestamp_;
    }
    is_new_data_available_ = false;
    // Let listeners know that we now have a position available.
    UpdateListeners();
    return;
  }

  DCHECK(request_ != NULL);

  // TODO(joth): Consider timing out any pending request.
  if (request_->is_request_pending())
    return;

  is_new_data_available_ = false;

  if (access_token_.empty())
    access_token_store_->GetAccessToken(request_->url(), &access_token_);

  AutoLock data_lock(data_mutex_);
  request_->MakeRequest(access_token_, radio_data_, wifi_data_,
                        device_data_updated_timestamp_);
}

void NetworkLocationProvider::OnDeviceDataUpdated() {
  if (MessageLoop::current() != client_loop()) {
    client_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &NetworkLocationProvider::OnDeviceDataUpdated));
    return;
  }
  device_data_updated_timestamp_ = GetCurrentTimeMillis();

  is_new_data_available_ = is_radio_data_complete_ || is_radio_data_complete_;
  if (delayed_start_task_.empty() ||
      (is_radio_data_complete_ && is_radio_data_complete_)) {
    UpdatePosition();
  }
}
