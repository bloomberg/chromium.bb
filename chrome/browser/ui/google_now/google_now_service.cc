// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/google_now/google_now_service.h"

#include "content/public/common/geoposition.h"

using base::TimeDelta;
using content::Geoposition;
using net::URLRequest;

namespace {
// Period for polling for Google Now cards to use when the period from the
// server is not available.
// TODO(vadimt): Figure out the value.
// TODO(vadimt): Figure out the consequences for LBS.
// TODO(vadimt): Consider triggers other than the timer for refreshing the
// position, such as waking from sleep.
const int kDefaultPollingPeriodMs = 300000; // 5 minutes
}  // namespace

struct GoogleNowService::ServerResponse {
  // TODO(vadimt): Populate this structure with real fields.
  TimeDelta next_request_delay;
};

GoogleNowService::GoogleNowService(Profile* profile)
    : profile_(profile) {
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

  UpdateCards();
}

void GoogleNowService::StartObtainingGeolocation() {
  // TODO(vadimt): Implement via making a geolocation request.
  OnLocationObtained(Geoposition());
}

void GoogleNowService::OnLocationObtained(const Geoposition& position) {
  DCHECK(IsGoogleNowEnabled());
  DCHECK(!next_update_timer_.IsRunning());

  StartServerRequest(position);
}

void GoogleNowService::StartServerRequest(
    const content::Geoposition& position) {
  // TODO(vadimt): Implement via making URLRequest to the server.
  OnServerRequestCompleted(NULL, 0);
}

void GoogleNowService::OnServerRequestCompleted(URLRequest* request,
                                                int num_bytes) {
  DCHECK(IsGoogleNowEnabled());
  DCHECK(!next_update_timer_.IsRunning());

  ServerResponse server_response;
  // TODO(vadimt): Check request's status.
  if (ParseServerResponse(request, num_bytes, &server_response)) {
    ShowNotifications(server_response);
    // Once the cards are shown, schedule next cards update after the delay
    // suggested by the server.
    StartWaitingForNextUpdate(server_response.next_request_delay);
  } else {
    // If the server response is bad, schedule next cards update after the
    // default delay.
    // TODO(vadimt): Consider exponential backoff with randomized jitter.
    StartWaitingForNextUpdate(
        TimeDelta::FromMilliseconds(kDefaultPollingPeriodMs));
  }
}

bool GoogleNowService::ParseServerResponse(const URLRequest* request,
                                           int num_bytes,
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
