// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/geolocation/network_location_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "net/url_request/url_request_context_getter.h"

namespace device {
namespace {
// The maximum period of time we'll wait for a complete set of wifi data
// before sending the request.
const int kDataCompleteWaitSeconds = 2;
}  // namespace

// static
const size_t NetworkLocationProvider::PositionCache::kMaximumSize = 10;

NetworkLocationProvider::PositionCache::PositionCache() {}

NetworkLocationProvider::PositionCache::~PositionCache() {}

bool NetworkLocationProvider::PositionCache::CachePosition(
    const WifiData& wifi_data,
    const Geoposition& position) {
  // Check that we can generate a valid key for the wifi data.
  base::string16 key;
  if (!MakeKey(wifi_data, &key)) {
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

// Searches for a cached position response for the current WiFi data. Returns
// the cached position if available, nullptr otherwise.
const Geoposition* NetworkLocationProvider::PositionCache::FindPosition(
    const WifiData& wifi_data) {
  base::string16 key;
  if (!MakeKey(wifi_data, &key)) {
    return nullptr;
  }
  CacheMap::const_iterator iter = cache_.find(key);
  return iter == cache_.end() ? nullptr : &iter->second;
}

// Makes the key for the map of cached positions, using the available data.
// Returns true if a good key was generated, false otherwise.
//
// static
bool NetworkLocationProvider::PositionCache::MakeKey(const WifiData& wifi_data,
                                                     base::string16* key) {
  // Currently we use only WiFi data and base the key only on the MAC addresses.
  DCHECK(key);
  key->clear();
  const size_t kCharsPerMacAddress = 6 * 3 + 1;  // e.g. "11:22:33:44:55:66|"
  key->reserve(wifi_data.access_point_data.size() * kCharsPerMacAddress);
  const base::string16 separator(base::ASCIIToUTF16("|"));
  for (const auto& access_point_data : wifi_data.access_point_data) {
    *key += separator;
    *key += access_point_data.mac_address;
    *key += separator;
  }
  // If the key is the empty string, return false, as we don't want to cache a
  // position for such data.
  return !key->empty();
}

// NetworkLocationProvider
NetworkLocationProvider::NetworkLocationProvider(
    scoped_refptr<net::URLRequestContextGetter> url_context_getter,
    const std::string& api_key)
    : wifi_data_provider_manager_(nullptr),
      wifi_data_update_callback_(
          base::Bind(&NetworkLocationProvider::OnWifiDataUpdate,
                     base::Unretained(this))),
      is_wifi_data_complete_(false),
      is_permission_granted_(false),
      is_new_data_available_(false),
      request_(new NetworkLocationRequest(
          std::move(url_context_getter),
          api_key,
          base::Bind(&NetworkLocationProvider::OnLocationResponse,
                     base::Unretained(this)))),
      position_cache_(new PositionCache),
      weak_factory_(this) {}

NetworkLocationProvider::~NetworkLocationProvider() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (IsStarted())
    StopProvider();
}

void NetworkLocationProvider::SetUpdateCallback(
    const LocationProvider::LocationProviderUpdateCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  location_provider_update_callback_ = callback;
}

void NetworkLocationProvider::OnPermissionGranted() {
  const bool was_permission_granted = is_permission_granted_;
  is_permission_granted_ = true;
  if (!was_permission_granted && IsStarted())
    RequestPosition();
}

void NetworkLocationProvider::OnWifiDataUpdate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsStarted());
  is_wifi_data_complete_ = wifi_data_provider_manager_->GetData(&wifi_data_);
  if (!is_wifi_data_complete_)
    return;

  wifi_timestamp_ = base::Time::Now();
  is_new_data_available_ = true;
  RequestPosition();
}

void NetworkLocationProvider::OnLocationResponse(
    const Geoposition& position,
    bool server_error,
    const WifiData& wifi_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Record the position and update our cache.
  position_ = position;
  if (position.Validate())
    position_cache_->CachePosition(wifi_data, position);

  // Let listeners know that we now have a position available.
  if (!location_provider_update_callback_.is_null())
    location_provider_update_callback_.Run(this, position_);
}

bool NetworkLocationProvider::StartProvider(bool high_accuracy) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (IsStarted())
    return true;

  // Registers a callback with the data provider. The first call to Register()
  // will create a singleton data provider that will be deleted on Unregister().
  wifi_data_provider_manager_ =
      WifiDataProviderManager::Register(&wifi_data_update_callback_);

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&NetworkLocationProvider::RequestPosition,
                            weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kDataCompleteWaitSeconds));

  OnWifiDataUpdate();
  return true;
}

void NetworkLocationProvider::StopProvider() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsStarted());
  wifi_data_provider_manager_->Unregister(&wifi_data_update_callback_);
  wifi_data_provider_manager_ = nullptr;
  weak_factory_.InvalidateWeakPtrs();
}

const Geoposition& NetworkLocationProvider::GetPosition() {
  return position_;
}

void NetworkLocationProvider::RequestPosition() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!is_new_data_available_ || !is_wifi_data_complete_)
    return;
  DCHECK(!wifi_timestamp_.is_null())
      << "|wifi_timestamp_| must be set before looking up position";

  const Geoposition* cached_position =
      position_cache_->FindPosition(wifi_data_);
  if (cached_position) {
    DCHECK(cached_position->Validate());
    // Record the position and update its timestamp.
    position_ = *cached_position;

    // The timestamp of a position fix is determined by the timestamp
    // of the source data update. (The value of position_.timestamp from
    // the cache could be from weeks ago!)
    position_.timestamp = wifi_timestamp_;
    is_new_data_available_ = false;

    // Let listeners know that we now have a position available.
    if (!location_provider_update_callback_.is_null())
      location_provider_update_callback_.Run(this, position_);
    return;
  }
  // Don't send network requests until authorized. http://crbug.com/39171
  if (!is_permission_granted_)
    return;

  is_new_data_available_ = false;

  // TODO(joth): Rather than cancel pending requests, we should create a new
  // NetworkLocationRequest for each and hold a set of pending requests.
  DLOG_IF(WARNING, request_->is_request_pending())
      << "NetworkLocationProvider - pre-empting pending network request "
         "with new data. Wifi APs: "
      << wifi_data_.access_point_data.size();

  request_->MakeRequest(wifi_data_, wifi_timestamp_);
}

bool NetworkLocationProvider::IsStarted() const {
  return wifi_data_provider_manager_ != nullptr;
}

}  // namespace device
