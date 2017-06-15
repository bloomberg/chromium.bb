// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/night_light/night_light_client.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

// Delay to wait for a response to our geolocation request, if we get a response
// after which, we will consider the request a failure.
constexpr base::TimeDelta kGeolocationRequestTimeout =
    base::TimeDelta::FromSeconds(60);

// Minimum delay to wait to fire a new request after a previous one failing.
constexpr base::TimeDelta kMinimumDelayAfterFailure =
    base::TimeDelta::FromSeconds(60);

// Delay to wait to fire a new request after a previous one succeeding.
constexpr base::TimeDelta kNextRequestDelayAfterSuccess =
    base::TimeDelta::FromDays(1);

}  // namespace

NightLightClient::NightLightClient(
    net::URLRequestContextGetter* url_context_getter)
    : provider_(
          url_context_getter,
          chromeos::SimpleGeolocationProvider::DefaultGeolocationProviderURL()),
      binding_(this),
      backoff_delay_(kMinimumDelayAfterFailure) {}

NightLightClient::~NightLightClient() {}

void NightLightClient::Start() {
  if (!night_light_controller_) {
    service_manager::Connector* connector =
        content::ServiceManagerConnection::GetForProcess()->GetConnector();
    connector->BindInterface(ash::mojom::kServiceName,
                             &night_light_controller_);
  }
  ash::mojom::NightLightClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  night_light_controller_->SetClient(std::move(client));
}

void NightLightClient::OnScheduleTypeChanged(
    ash::mojom::NightLightController::ScheduleType new_type) {
  if (new_type ==
      ash::mojom::NightLightController::ScheduleType::kSunsetToSunrise) {
    // Schedule an immediate request.
    using_geoposition_ = true;
    ScheduleNextRequest(base::TimeDelta::FromSeconds(0));
  } else {
    using_geoposition_ = false;
    timer_.Stop();
  }
}

void NightLightClient::SetNightLightControllerPtrForTesting(
    ash::mojom::NightLightControllerPtr controller) {
  night_light_controller_ = std::move(controller);
}

void NightLightClient::FlushNightLightControllerForTesting() {
  night_light_controller_.FlushForTesting();
}

void NightLightClient::OnGeoposition(const chromeos::Geoposition& position,
                                     bool server_error,
                                     const base::TimeDelta elapsed) {
  if (!using_geoposition_) {
    // A response might arrive after the schedule type is no longer "sunset to
    // sunrise", which means we should not push any positions to the
    // NightLightController.
    return;
  }

  if (server_error || !position.Valid() ||
      elapsed > kGeolocationRequestTimeout) {
    // Don't send invalid positions to ash.
    // On failure, we schedule another request after the current backoff delay.
    ScheduleNextRequest(backoff_delay_);

    // If another failure occurs next, our backoff delay should double.
    backoff_delay_ *= 2;
    return;
  }

  night_light_controller_->SetCurrentGeoposition(
      ash::mojom::SimpleGeoposition::New(position.latitude,
                                         position.longitude));

  // On success, reset the backoff delay to its minimum value, and schedule
  // another request.
  backoff_delay_ = kMinimumDelayAfterFailure;
  ScheduleNextRequest(kNextRequestDelayAfterSuccess);
}

void NightLightClient::ScheduleNextRequest(base::TimeDelta delay) {
  timer_.Start(FROM_HERE, delay, this, &NightLightClient::RequestGeoposition);
}

void NightLightClient::RequestGeoposition() {
  provider_.RequestGeolocation(
      kGeolocationRequestTimeout, false /* send_wifi_access_points */,
      false /* send_cell_towers */,
      base::Bind(&NightLightClient::OnGeoposition, base::Unretained(this)));
}
