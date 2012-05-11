// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_TO_MOBILE_SERVICE_H_
#define CHROME_BROWSER_CHROME_TO_MOBILE_SERVICE_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "base/timer.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/common/net/gaia/oauth2_access_token_consumer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"

class OAuth2AccessTokenFetcher;
class CloudPrintURL;
class MockChromeToMobileService;
class Profile;

namespace base {
class DictionaryValue;
}

// ChromeToMobileService connects to the cloud print service to enumerate
// compatible mobiles owned by its profile and send URLs and MHTML snapshots.
class ChromeToMobileService : public ProfileKeyedService,
                              public content::URLFetcherDelegate,
                              public content::NotificationObserver,
                              public OAuth2AccessTokenConsumer {
 public:
  class Observer {
   public:
    virtual ~Observer();

    // Called on generation of the page's MHTML snapshot.
    virtual void SnapshotGenerated(const FilePath& path, int64 bytes) = 0;

    // Called after URLFetcher responses from sending the URL (and snapshot).
    virtual void OnSendComplete(bool success) = 0;
  };

  enum Metric {
    DEVICES_REQUESTED = 0,  // Cloud print was contacted to list devices.
    DEVICES_AVAILABLE,      // Cloud print returned 1+ compatible devices.
    BUBBLE_SHOWN,           // The page action bubble was shown.
    SNAPSHOT_GENERATED,     // A snapshot was successfully generated.
    SNAPSHOT_ERROR,         // An error occurred during snapshot generation.
    SENDING_URL,            // Send was invoked (with or without a snapshot).
    SENDING_SNAPSHOT,       // A snapshot was sent along with the page URL.
    SEND_SUCCESS,           // Cloud print responded with success on send.
    SEND_ERROR,             // Cloud print responded with failure on send.
    NUM_METRICS
  };

  // The URLFetcher request types.
  enum RequestType {
    SEARCH,
    URL,
    DELAYED_SNAPSHOT,
    SNAPSHOT,
  };

  // The aggregated URLFetcher submission data.
  struct RequestData {
    RequestData();
    ~RequestData();

    string16 mobile_id;
    GURL url;
    string16 title;
    FilePath snapshot_path;
    std::string snapshot_id;
    RequestType type;
  };

  // Returns whether Chrome To Mobile is enabled. Check for the 'disable' or
  // 'enable' command line switches, otherwise relay the default enabled state.
  static bool IsChromeToMobileEnabled();

  explicit ChromeToMobileService(Profile* profile);
  virtual ~ChromeToMobileService();

  // Returns true if the service has found any registered mobile devices.
  bool HasDevices();

  // Get the list of mobile devices.
  const std::vector<base::DictionaryValue*>& mobiles();

  // Request an updated mobile device list, request auth first if needed.
  // Virtual for unit test mocking.
  virtual void RequestMobileListUpdate();

  // Callback with an MHTML snapshot of the profile's selected WebContents.
  // Virtual for unit test mocking.
  virtual void GenerateSnapshot(base::WeakPtr<Observer> observer);

  // Send the profile's selected WebContents to the specified mobile device.
  // Virtual for unit test mocking.
  virtual void SendToMobile(const string16& mobile_id,
                            const FilePath& snapshot,
                            base::WeakPtr<Observer> observer);

  // Delete the snapshot file (should be called on observer destruction).
  // Virtual for unit test mocking.
  virtual void DeleteSnapshot(const FilePath& snapshot);

  // Log a metric for the "ChromeToMobile.Service" histogram.
  // Virtual for unit test mocking.
  virtual void LogMetric(Metric metric);

  // content::URLFetcherDelegate method.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // content::NotificationObserver method.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // OAuth2AccessTokenConsumer methods.
  virtual void OnGetTokenSuccess(const std::string& access_token) OVERRIDE;
  virtual void OnGetTokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

 private:
  friend class MockChromeToMobileService;

  // Handle the attempted creation of a temporary file for snapshot generation.
  // Alert the observer of failure or generate MHTML with an observer callback.
  void SnapshotFileCreated(base::WeakPtr<Observer> observer,
                           const FilePath& path,
                           bool success);

  // Utility function to create URLFetcher requests.
  content::URLFetcher* CreateRequest(const RequestData& data);

  // Send the OAuth2AccessTokenFetcher request.
  // Virtual for unit test mocking.
  virtual void RefreshAccessToken();

  // Request account information to limit cloud print access to existing users.
  void RequestAccountInfo();

  // Send the cloud print URLFetcher search request.
  void RequestSearch();

  void HandleAccountInfoResponse();
  void HandleSearchResponse();
  void HandleSubmitResponse(const net::URLFetcher* source);

  base::WeakPtrFactory<ChromeToMobileService> weak_ptr_factory_;

  Profile* profile_;

  // Used to recieve TokenService notifications for GaiaOAuth2LoginRefreshToken.
  content::NotificationRegistrar registrar_;

  // Cloud print helper class and auth token.
  scoped_ptr<CloudPrintURL> cloud_print_url_;
  std::string access_token_;

  // The list of mobile devices retrieved from the cloud print service.
  ScopedVector<base::DictionaryValue> mobiles_;

  // The set of snapshots currently available.
  std::set<FilePath> snapshots_;

  // Map URLFetchers to observers for reporting OnSendComplete.
  typedef std::map<const net::URLFetcher*, base::WeakPtr<Observer> >
      RequestObserverMap;
  RequestObserverMap request_observer_map_;

  // The pending OAuth access token request and a timer for retrying on failure.
  scoped_ptr<OAuth2AccessTokenFetcher> access_token_fetcher_;
  base::OneShotTimer<ChromeToMobileService> auth_retry_timer_;

  // The pending account information request and the cloud print access flag.
  scoped_ptr<content::URLFetcher> account_info_request_;
  bool cloud_print_accessible_;

  // The pending mobile device search request; and the time of the last request.
  scoped_ptr<content::URLFetcher> search_request_;
  base::TimeTicks previous_search_time_;

  DISALLOW_COPY_AND_ASSIGN(ChromeToMobileService);
};

#endif  // CHROME_BROWSER_CHROME_TO_MOBILE_SERVICE_H_
