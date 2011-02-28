// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/network_location_provider.h"

#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "content/browser/geolocation/access_token_store.h"

namespace {
// The maximum period of time we'll wait for a complete set of device data
// before sending the request.
const int kDataCompleteWaitPeriod = 1000 * 2;  // 2 seconds
}  // namespace

// static
const size_t NetworkLocationProvider::PositionCache::kMaximumSize = 10;

NetworkLocationProvider::PositionCache::PositionCache() {}

NetworkLocationProvider::PositionCache::~PositionCache() {}

bool NetworkLocationProvider::PositionCache::CachePosition(
    const GatewayData& gateway_data,
    const WifiData& wifi_data,
    const Geoposition& position) {
  // Check that we can generate a valid key for the device data.
  string16 key;
  if (!MakeKey(gateway_data, wifi_data, &key)) {
    return false;
  }
  // If the cache is full, remove the oldest entry.
  if (cache_.size() == kMaximumSize) {
    DCHECK(cache_age_list_.size() == kMaximumSize);
    CacheAgeList::iterator oldest_entry = cache_age_list_.begin();
    DCHECK(oldest_entry != cache_age_list_.end());
    cache_.erase(*oldest_entry);
    cache_age_list_.erase(oldest_entry);
  }
  DCHECK_LT(cache_.size(), kMaximumSize);
  // Insert the position into the cache.
  std::pair<CacheMap::iterator, bool> result =
      cache_.insert(std::make_pair(key, position));
  if (!result.second) {
    NOTREACHED();  // We never try to add the same key twice.
    CHECK_EQ(cache_.size(), cache_age_list_.size());
    return false;
  }
  cache_age_list_.push_back(result.first);
  DCHECK_EQ(cache_.size(), cache_age_list_.size());
  return true;
}

// Searches for a cached position response for the current set of cell ID and
// WiFi data. Returns the cached position if available, NULL otherwise.
const Geoposition* NetworkLocationProvider::PositionCache::FindPosition(
    const GatewayData& gateway_data,
    const WifiData& wifi_data) {
  string16 key;
  if (!MakeKey(gateway_data, wifi_data, &key)) {
    return NULL;
  }
  CacheMap::const_iterator iter = cache_.find(key);
  return iter == cache_.end() ? NULL : &iter->second;
}

// Makes the key for the map of cached positions, using a set of
// device data. Returns true if a good key was generated, false otherwise.
//
// static
bool NetworkLocationProvider::PositionCache::MakeKey(
    const GatewayData& gateway_data,
    const WifiData& wifi_data,
    string16* key) {
  // Currently we use only the WiFi data and gateway data, and base the
  // key only on the MAC addresses.
  // TODO(joth): Make use of radio_data.
  DCHECK(key);
  key->clear();
  const size_t kCharsPerMacAddress = 6 * 3 + 1;  // e.g. "11:22:33:44:55:66|"
  key->reserve(wifi_data.access_point_data.size() * kCharsPerMacAddress
               + gateway_data.router_data.size() * kCharsPerMacAddress);
  const string16 separator(ASCIIToUTF16("|"));
  for (GatewayData::RouterDataSet::const_iterator iter =
       gateway_data.router_data.begin();
       iter != gateway_data.router_data.end();
       iter++) {
    *key += separator;
    *key += iter->mac_address;
    *key += separator;
  }
  for (WifiData::AccessPointDataSet::const_iterator iter =
       wifi_data.access_point_data.begin();
       iter != wifi_data.access_point_data.end();
       iter++) {
    *key += separator;
    *key += iter->mac_address;
    *key += separator;
  }
  // If the key is the empty string, return false, as we don't want to cache a
  // position for such a set of device data.
  return !key->empty();
}

// NetworkLocationProvider factory function
LocationProviderBase* NewNetworkLocationProvider(
    AccessTokenStore* access_token_store,
    URLRequestContextGetter* context,
    const GURL& url,
    const string16& access_token) {
  return new NetworkLocationProvider(
      access_token_store, context, url, access_token);
}

// NetworkLocationProvider
NetworkLocationProvider::NetworkLocationProvider(
    AccessTokenStore* access_token_store,
    URLRequestContextGetter* url_context_getter,
    const GURL& url,
    const string16& access_token)
    : access_token_store_(access_token_store),
      gateway_data_provider_(NULL),
      radio_data_provider_(NULL),
      wifi_data_provider_(NULL),
      is_gateway_data_complete_(false),
      is_radio_data_complete_(false),
      is_wifi_data_complete_(false),
      access_token_(access_token),
      is_new_data_available_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(delayed_start_task_(this)) {
  // Create the position cache.
  position_cache_.reset(new PositionCache());

  request_.reset(new NetworkLocationRequest(url_context_getter, url, this));
}

NetworkLocationProvider::~NetworkLocationProvider() {
  StopProvider();
}

// LocationProviderBase implementation
void NetworkLocationProvider::GetPosition(Geoposition *position) {
  DCHECK(position);
  *position = position_;
}

void NetworkLocationProvider::UpdatePosition() {
  // TODO(joth): When called via the public (base class) interface, this should
  // poke each data provider to get them to expedite their next scan.
  // Whilst in the delayed start, only send request if all data is ready.
  if (delayed_start_task_.empty() ||
      (is_gateway_data_complete_ && is_radio_data_complete_ &&
          is_wifi_data_complete_)) {
    RequestPosition();
  }
}

void NetworkLocationProvider::OnPermissionGranted(
    const GURL& requesting_frame) {
  const bool host_was_empty = most_recent_authorized_host_.empty();
  most_recent_authorized_host_ = requesting_frame.host();
  if (host_was_empty && !most_recent_authorized_host_.empty()
      && IsStarted()) {
    UpdatePosition();
  }
}

// DeviceDataProviderInterface::ListenerInterface implementation.
void NetworkLocationProvider::DeviceDataUpdateAvailable(
    GatewayDataProvider* provider) {
  DCHECK(provider == gateway_data_provider_);
  is_gateway_data_complete_ = gateway_data_provider_->GetData(&gateway_data_);
  OnDeviceDataUpdated();
}

void NetworkLocationProvider::DeviceDataUpdateAvailable(
    RadioDataProvider* provider) {
  DCHECK(provider == radio_data_provider_);
  is_radio_data_complete_ = radio_data_provider_->GetData(&radio_data_);
  OnDeviceDataUpdated();
}

void NetworkLocationProvider::DeviceDataUpdateAvailable(
    WifiDataProvider* provider) {
  DCHECK(provider == wifi_data_provider_);
  is_wifi_data_complete_ = wifi_data_provider_->GetData(&wifi_data_);
  OnDeviceDataUpdated();
}

// NetworkLocationRequest::ListenerInterface implementation.
void NetworkLocationProvider::LocationResponseAvailable(
    const Geoposition& position,
    bool server_error,
    const string16& access_token,
    const GatewayData& gateway_data,
    const RadioData& radio_data,
    const WifiData& wifi_data) {
  DCHECK(CalledOnValidThread());
  // Record the position and update our cache.
  position_ = position;
  if (position.IsValidFix()) {
    position_cache_->CachePosition(gateway_data, wifi_data, position);
  }

  // Record access_token if it's set.
  if (!access_token.empty() && access_token_ != access_token) {
    access_token_ = access_token;
    access_token_store_->SaveAccessToken(request_->url(), access_token);
  }

  // Let listeners know that we now have a position available.
  UpdateListeners();
}

bool NetworkLocationProvider::StartProvider(bool high_accuracy) {
  DCHECK(CalledOnValidThread());
  if (IsStarted())
    return true;
  DCHECK(wifi_data_provider_ == NULL);
  if (!request_->url().is_valid()) {
    LOG(WARNING) << "StartProvider() : Failed, Bad URL: "
                 << request_->url().possibly_invalid_spec();
    return false;
  }

  // Get the device data providers. The first call to Register will create the
  // provider and it will be deleted by ref counting.
  gateway_data_provider_ = GatewayDataProvider::Register(this);
  radio_data_provider_ = RadioDataProvider::Register(this);
  wifi_data_provider_ = WifiDataProvider::Register(this);

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      delayed_start_task_.NewRunnableMethod(
          &NetworkLocationProvider::RequestPosition),
      kDataCompleteWaitPeriod);
  // Get the device data.
  is_gateway_data_complete_ = gateway_data_provider_->GetData(&gateway_data_);
  is_radio_data_complete_ = radio_data_provider_->GetData(&radio_data_);
  is_wifi_data_complete_ = wifi_data_provider_->GetData(&wifi_data_);
  if (is_gateway_data_complete_ || is_radio_data_complete_ ||
      is_wifi_data_complete_)
    OnDeviceDataUpdated();
  return true;
}

void NetworkLocationProvider::StopProvider() {
  DCHECK(CalledOnValidThread());
  if (IsStarted()) {
    gateway_data_provider_->Unregister(this);
    radio_data_provider_->Unregister(this);
    wifi_data_provider_->Unregister(this);
  }
  gateway_data_provider_ = NULL;
  radio_data_provider_ = NULL;
  wifi_data_provider_ = NULL;
  delayed_start_task_.RevokeAll();
}

// Other methods
void NetworkLocationProvider::RequestPosition() {
  DCHECK(CalledOnValidThread());
  if (!is_new_data_available_)
    return;

  const Geoposition* cached_position =
      position_cache_->FindPosition(gateway_data_, wifi_data_);
  DCHECK(!device_data_updated_timestamp_.is_null()) <<
      "Timestamp must be set before looking up position";
  if (cached_position) {
    DCHECK(cached_position->IsValidFix());
    // Record the position and update its timestamp.
    position_ = *cached_position;
    // The timestamp of a position fix is determined by the timestamp
    // of the source data update. (The value of position_.timestamp from
    // the cache could be from weeks ago!)
    position_.timestamp = device_data_updated_timestamp_;
    is_new_data_available_ = false;
    // Let listeners know that we now have a position available.
    UpdateListeners();
    return;
  }
  // Don't send network requests until authorized. http://crbug.com/39171
  if (most_recent_authorized_host_.empty())
    return;

  delayed_start_task_.RevokeAll();
  is_new_data_available_ = false;

  // TODO(joth): Rather than cancel pending requests, we should create a new
  // NetworkLocationRequest for each and hold a set of pending requests.
  if (request_->is_request_pending()) {
    DVLOG(1) << "NetworkLocationProvider - pre-empting pending network request "
                "with new data. Wifi APs: "
             << wifi_data_.access_point_data.size();
  }
  // The hostname sent in the request is just to give a first-order
  // approximation of usage. We do not need to guarantee that this network
  // request was triggered by an API call from this specific host.
  request_->MakeRequest(most_recent_authorized_host_, access_token_,
                        gateway_data_, radio_data_, wifi_data_,
                        device_data_updated_timestamp_);
}

void NetworkLocationProvider::OnDeviceDataUpdated() {
  DCHECK(CalledOnValidThread());
  device_data_updated_timestamp_ = base::Time::Now();

  is_new_data_available_ = is_gateway_data_complete_ ||
      is_radio_data_complete_ || is_wifi_data_complete_;
  UpdatePosition();
}

bool NetworkLocationProvider::IsStarted() const {
  DCHECK_EQ(!!gateway_data_provider_, !!wifi_data_provider_);
  DCHECK_EQ(!!radio_data_provider_, !!wifi_data_provider_);
  return wifi_data_provider_ != NULL;
}
