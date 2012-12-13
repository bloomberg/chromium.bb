// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/google_now/google_now_service.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/geolocation.h"
#include "content/public/common/geoposition.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"

using base::Bind;
using base::TimeDelta;
using content::Geoposition;
using net::HTTP_OK;
using net::URLFetcher;
using net::URLRequestStatus;

namespace {
// TODO(vadimt): Figure out the values of the constants.

// Period for polling for Google Now cards to use when the period from the
// server is not available.
// TODO(vadimt): Figure out the consequences for LBS and battery.
// TODO(vadimt): Consider triggers other than the timer for refreshing the
// position, such as waking from sleep.
const int kDefaultPollingPeriodMs = 300000; // 5 minutes
// Time allocated to a geolocation request.
const int kGeolocationRequestTimeoutMs = 60000; // 1 minute
}  // namespace

struct GoogleNowService::ServerResponse {
  // TODO(vadimt): Populate this structure with real fields.
  TimeDelta next_request_delay;
};

GoogleNowService::GoogleNowService(Profile* profile)
    : profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(geolocation_request_weak_factory_(this)) {
  DCHECK(profile_);
}

GoogleNowService::~GoogleNowService() {
}

void GoogleNowService::Init() {
  // If Google Now integration is enabled for the profile, start the first cards
  // update.
  if (IsGoogleNowEnabled())
    UpdateCards();
}

bool GoogleNowService::IsGoogleNowEnabled() const {
  // TODO(vadimt): Return a value indicating whether Google Now integration is
  // enabled for 'profile_'.
  // TODO(vadimt): Process enabling and disabling Google Now integration while
  // the service is running.
  return true;
}

void GoogleNowService::UpdateCards() {
  // Start obtaining geolocation for the server's request.
  StartObtainingGeolocation();
}

void GoogleNowService::StartWaitingForNextUpdate(TimeDelta delay) {
  DCHECK(!next_update_timer_.IsRunning());

  next_update_timer_.Start(FROM_HERE, delay,
                           this, &GoogleNowService::OnWaitingForNextUpdateEnds);
}

void GoogleNowService::OnWaitingForNextUpdateEnds() {
  DCHECK(IsGoogleNowEnabled());
  DCHECK(!next_update_timer_.IsRunning());
  DCHECK(!geolocation_request_timer_.IsRunning());
  DCHECK(!geolocation_request_weak_factory_.HasWeakPtrs());
  DCHECK(fetcher_.get() == NULL);

  UpdateCards();
}

void GoogleNowService::StartObtainingGeolocation() {
  DCHECK(!geolocation_request_weak_factory_.HasWeakPtrs());
  content::RequestLocationUpdate(Bind(
      &GoogleNowService::OnLocationObtained,
      geolocation_request_weak_factory_.GetWeakPtr()));

  DCHECK(!geolocation_request_timer_.IsRunning());
  geolocation_request_timer_.Start(FROM_HERE,
      TimeDelta::FromMilliseconds(kGeolocationRequestTimeoutMs),
      this, &GoogleNowService::OnLocationRequestTimeout);
}

void GoogleNowService::OnLocationObtained(const Geoposition& position) {
  DCHECK(IsGoogleNowEnabled());
  DCHECK(!next_update_timer_.IsRunning());
  DCHECK(geolocation_request_timer_.IsRunning());
  DCHECK(geolocation_request_weak_factory_.HasWeakPtrs());
  DCHECK(fetcher_.get() == NULL);

  geolocation_request_weak_factory_.InvalidateWeakPtrs();
  geolocation_request_timer_.Stop();
  StartServerRequest(position);
}

void GoogleNowService::OnLocationRequestTimeout() {
  DCHECK(IsGoogleNowEnabled());
  DCHECK(!next_update_timer_.IsRunning());
  DCHECK(!geolocation_request_timer_.IsRunning());
  DCHECK(geolocation_request_weak_factory_.HasWeakPtrs());
  DCHECK(fetcher_.get() == NULL);

  geolocation_request_weak_factory_.InvalidateWeakPtrs();
  StartServerRequest(Geoposition());
}

std::string GoogleNowService::BuildRequestURL(
    const content::Geoposition& position) {
  std::string server_path =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kEnableGoogleNowIntegration);
  DCHECK(!server_path.empty());

  if (position.Validate()) {
    // If position is available, append it to the URL.
    std::stringstream parameters;
    parameters << "?q="
               << position.latitude << ','
               << position.longitude << ','
               << position.accuracy;
    server_path += parameters.str();
  }

  return server_path;
}

void GoogleNowService::StartServerRequest(
    const content::Geoposition& position) {
  DCHECK(fetcher_.get() == NULL);
  fetcher_.reset(URLFetcher::Create(GURL(BuildRequestURL(position)),
                                    URLFetcher::GET,
                                    this));

  // TODO(vadimt): Figure out how to send user's identity to the server. Make
  // sure we never get responses like 'Select an account' page.
  fetcher_->SetRequestContext(profile_->GetRequestContext());
  fetcher_->SetLoadFlags(
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
      net::LOAD_DO_NOT_SAVE_COOKIES);

  fetcher_->Start();
}

bool GoogleNowService::ParseServerResponse(const std::string& response_string,
                                           ServerResponse* server_response) {
  // TODO(vadimt): Do real parsing.
  server_response->next_request_delay =
      TimeDelta::FromMilliseconds(kDefaultPollingPeriodMs);
  return true;
}

void GoogleNowService::ShowNotifications(
    const ServerResponse& server_response) {
  // TODO(vadimt): Implement using Chrome Notifications.
}

void GoogleNowService::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(source != NULL);
  DCHECK(IsGoogleNowEnabled());
  DCHECK(!next_update_timer_.IsRunning());
  DCHECK(!geolocation_request_timer_.IsRunning());
  DCHECK(!geolocation_request_weak_factory_.HasWeakPtrs());
  DCHECK(fetcher_.get() == source);

  // TODO(vadimt): Implement exponential backoff with randomized jitter.
  TimeDelta next_request_delay =
      TimeDelta::FromMilliseconds(kDefaultPollingPeriodMs);

  if (source->GetStatus().status() == URLRequestStatus::SUCCESS &&
      source->GetResponseCode() == HTTP_OK) {
    std::string response_string;

    if (source->GetResponseAsString(&response_string)) {
      ServerResponse server_response;

      if (ParseServerResponse(response_string, &server_response)) {
        ShowNotifications(server_response);
        next_request_delay = server_response.next_request_delay;
      }
    }
  }

  fetcher_.reset(NULL);
  StartWaitingForNextUpdate(next_request_delay);
}
