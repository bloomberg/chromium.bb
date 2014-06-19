// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/location/location_api.h"

#include "chrome/browser/extensions/api/location/location_manager.h"
#include "chrome/common/extensions/api/location.h"
#include "extensions/common/error_utils.h"

// TODO(vadimt): add tests.

namespace location = extensions::api::location;
namespace WatchLocation = location::WatchLocation;
namespace ClearWatch = location::ClearWatch;

namespace extensions {

const char kMustBePositive[] = "'*' must be 0 or greater.";
const char kMinDistanceInMeters[] = "minDistanceInMeters";
const char kMinTimeInMilliseconds[] = "minTimeInMilliseconds";

bool IsNegative(double* value) {
  return value && *value < 0.0;
}

bool LocationWatchLocationFunction::RunSync() {
  scoped_ptr<WatchLocation::Params> params(
      WatchLocation::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  double* min_distance_in_meters =
      params->request_info.min_distance_in_meters.get();
  if (IsNegative(min_distance_in_meters)) {
    error_ = ErrorUtils::FormatErrorMessage(
        kMustBePositive,
        kMinDistanceInMeters);
    return false;
  }

  double* min_time_in_milliseconds =
      params->request_info.min_time_in_milliseconds.get();
  if (IsNegative(min_time_in_milliseconds)) {
    error_ = ErrorUtils::FormatErrorMessage(
        kMustBePositive,
        kMinTimeInMilliseconds);
    return false;
  }

  // TODO(vadimt): validate and use params->request_info.maximumAge
  LocationManager::Get(browser_context())
      ->AddLocationRequest(extension_id(),
                           params->name,
                           min_distance_in_meters,
                           min_time_in_milliseconds);

  return true;
}

bool LocationClearWatchFunction::RunSync() {
  scoped_ptr<ClearWatch::Params> params(
      ClearWatch::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  LocationManager::Get(browser_context())
      ->RemoveLocationRequest(extension_id(), params->name);

  return true;
}

}  // namespace extensions
