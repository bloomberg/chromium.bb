// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/geolocation/location_arbitrator.h"

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "device/geolocation/access_token_store.h"
#include "device/geolocation/geolocation_delegate.h"
#include "device/geolocation/network_location_provider.h"

namespace device {

// To avoid oscillations, set this to twice the expected update interval of a
// a GPS-type location provider (in case it misses a beat) plus a little.
const int64_t LocationArbitrator::kFixStaleTimeoutMilliseconds =
    11 * base::Time::kMillisecondsPerSecond;

LocationArbitrator::LocationArbitrator(
    std::unique_ptr<GeolocationDelegate> delegate,
    GeolocationProvider::RequestContextProducer request_context_producer,
    const std::string& api_key)
    : delegate_(std::move(delegate)),
      request_context_producer_(request_context_producer),
      api_key_(api_key),
      position_provider_(nullptr),
      is_permission_granted_(false),
      is_running_(false) {}

LocationArbitrator::~LocationArbitrator() {}

bool LocationArbitrator::HasPermissionBeenGrantedForTest() const {
  return is_permission_granted_;
}

void LocationArbitrator::OnPermissionGranted() {
  is_permission_granted_ = true;
  for (const auto& provider : providers_)
    provider->OnPermissionGranted();
}

void LocationArbitrator::StartProvider(bool enable_high_accuracy) {
  is_running_ = true;
  enable_high_accuracy_ = enable_high_accuracy;

  if (providers_.empty()) {
    RegisterSystemProvider();

    // Create a network location provider if the embedder provided an
    // AccessTokenStore.
    // TODO(amoylan): Replace this usage of GetAccessTokenStore() (i.e., as a
    // flag controlling whether to use network location providers) with a check
    // on whether the return value of |request_context_producer_| is null.
    if (GetAccessTokenStore() && !request_context_producer_.is_null()) {
      // Note: .Reset() will cancel any previous callback.
      request_context_response_callback_.Reset(
          base::Bind(&LocationArbitrator::OnRequestContextResponse,
                     base::Unretained(this)));
      // Invoke callback to obtain a URL request context.
      request_context_producer_.Run(
          request_context_response_callback_.callback());
      return;
    }
  }
  DoStartProviders();
}

void LocationArbitrator::DoStartProviders() {
  if (providers_.empty()) {
    // If no providers are available, we report an error to avoid
    // callers waiting indefinitely for a reply.
    Geoposition position;
    position.error_code = Geoposition::ERROR_CODE_POSITION_UNAVAILABLE;
    arbitrator_update_callback_.Run(this, position);
    return;
  }
  for (const auto& provider : providers_) {
    provider->StartProvider(enable_high_accuracy_);
  }
}

void LocationArbitrator::StopProvider() {
  // Reset the reference location state (provider+position)
  // so that future starts use fresh locations from
  // the newly constructed providers.
  position_provider_ = nullptr;
  position_ = Geoposition();

  providers_.clear();
  is_running_ = false;
}

void LocationArbitrator::OnRequestContextResponse(
    scoped_refptr<net::URLRequestContextGetter> context_getter) {
  // Create a NetworkLocationProvider using the provided request context.
  RegisterProvider(
      NewNetworkLocationProvider(std::move(context_getter), api_key_));
  DoStartProviders();
}

void LocationArbitrator::RegisterProvider(
    std::unique_ptr<LocationProvider> provider) {
  if (!provider)
    return;
  provider->SetUpdateCallback(base::Bind(&LocationArbitrator::OnLocationUpdate,
                                         base::Unretained(this)));
  if (is_permission_granted_)
    provider->OnPermissionGranted();
  providers_.push_back(std::move(provider));
}

void LocationArbitrator::RegisterSystemProvider() {
  std::unique_ptr<LocationProvider> provider =
      delegate_->OverrideSystemLocationProvider();
  if (!provider)
    provider = NewSystemLocationProvider();
  RegisterProvider(std::move(provider));
}

void LocationArbitrator::OnLocationUpdate(const LocationProvider* provider,
                                          const Geoposition& new_position) {
  DCHECK(new_position.Validate() ||
         new_position.error_code != Geoposition::ERROR_CODE_NONE);
  if (!IsNewPositionBetter(position_, new_position,
                           provider == position_provider_))
    return;
  position_provider_ = provider;
  position_ = new_position;
  arbitrator_update_callback_.Run(this, position_);
}

const Geoposition& LocationArbitrator::GetPosition() {
  return position_;
}

void LocationArbitrator::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  DCHECK(!callback.is_null());
  arbitrator_update_callback_ = callback;
}

scoped_refptr<AccessTokenStore> LocationArbitrator::NewAccessTokenStore() {
  return delegate_->CreateAccessTokenStore();
}

scoped_refptr<AccessTokenStore> LocationArbitrator::GetAccessTokenStore() {
  if (!access_token_store_)
    access_token_store_ = NewAccessTokenStore();
  return access_token_store_;
}

std::unique_ptr<LocationProvider>
LocationArbitrator::NewNetworkLocationProvider(
    scoped_refptr<net::URLRequestContextGetter> context,
    const std::string& api_key) {
#if defined(OS_ANDROID)
  // Android uses its own SystemLocationProvider.
  return nullptr;
#else
  return std::make_unique<NetworkLocationProvider>(std::move(context), api_key);
#endif
}

std::unique_ptr<LocationProvider>
LocationArbitrator::NewSystemLocationProvider() {
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_FUCHSIA)
  return nullptr;
#else
  return device::NewSystemLocationProvider();
#endif
}

base::Time LocationArbitrator::GetTimeNow() const {
  return base::Time::Now();
}

bool LocationArbitrator::IsNewPositionBetter(const Geoposition& old_position,
                                             const Geoposition& new_position,
                                             bool from_same_provider) const {
  // Updates location_info if it's better than what we currently have,
  // or if it's a newer update from the same provider.
  if (!old_position.Validate()) {
    // Older location wasn't locked.
    return true;
  }
  if (new_position.Validate()) {
    // New location is locked, let's check if it's any better.
    if (old_position.accuracy >= new_position.accuracy) {
      // Accuracy is better.
      return true;
    } else if (from_same_provider) {
      // Same provider, fresher location.
      return true;
    } else if ((GetTimeNow() - old_position.timestamp).InMilliseconds() >
               kFixStaleTimeoutMilliseconds) {
      // Existing fix is stale.
      return true;
    }
  }
  return false;
}

}  // namespace device
