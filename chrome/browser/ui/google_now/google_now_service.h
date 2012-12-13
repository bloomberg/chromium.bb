// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GOOGLE_NOW_GOOGLE_NOW_SERVICE_H_
#define CHROME_BROWSER_UI_GOOGLE_NOW_GOOGLE_NOW_SERVICE_H_

#include "base/timer.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "net/url_request/url_fetcher_delegate.h"

class Profile;

namespace base {
class TimeDelta;
}

namespace content {
struct Geoposition;
}

// The Google Now service gets Google Now cards from the server and shows them
// as Chrome notifications.
// The service performs periodic updating of Google Now cards.
// Each updating of the cards consists of 3 steps:
// 1. Obtaining the location of the machine (asynchronous);
// 2. Making a server request (asynchronous);
// 3. Showing the cards as notifications.
class GoogleNowService : public ProfileKeyedService,
                         public net::URLFetcherDelegate {
 public:
  // Must call Init after construction.
  explicit GoogleNowService(Profile* profile);
  virtual ~GoogleNowService();
  void Init();

 private:
  // Parsed response from the server.
  struct ServerResponse;

  // Returns true if Google Now integration is enabled for the profile.
  bool IsGoogleNowEnabled() const;
  // Gets new cards from the server and shows them as notifications.
  void UpdateCards();

  // Schedules next cards update after the specified delay.
  void StartWaitingForNextUpdate(base::TimeDelta delay);
  void OnWaitingForNextUpdateEnds();

  // Starts obtaining location of the machine.
  void StartObtainingGeolocation();
  void OnLocationObtained(const content::Geoposition& position);
  void OnLocationRequestTimeout();

  // Builds a URL for a server request.
  static std::string BuildRequestURL(const content::Geoposition& position);

  // Starts downloading cards from the server.
  void StartServerRequest(const content::Geoposition& position);

  // Parses server response. Returns true if the parsing was successful.
  static bool ParseServerResponse(const std::string& response_string,
                                  ServerResponse* server_response);

  // Shows Google Now cards as notifications.
  void ShowNotifications(const ServerResponse& server_response);

  // URLFetcherDelegate
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // The profile.
  Profile* const profile_;
  // Timer to schedule next cards update.
  base::OneShotTimer<GoogleNowService> next_update_timer_;
  // Timer to cancel geolocation requests that take too long.
  base::OneShotTimer<GoogleNowService> geolocation_request_timer_;
  // Weak factory for the geolocation request callback. Used to ensure
  // geolocation request callback is not run after this object is destroyed.
  base::WeakPtrFactory<GoogleNowService> geolocation_request_weak_factory_;
  // Fetcher for Google Now cards.
  scoped_ptr<net::URLFetcher> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(GoogleNowService);
};

#endif  // CHROME_BROWSER_UI_GOOGLE_NOW_GOOGLE_NOW_SERVICE_H_
