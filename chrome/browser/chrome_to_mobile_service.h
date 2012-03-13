// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_TO_MOBILE_SERVICE_H_
#define CHROME_BROWSER_CHROME_TO_MOBILE_SERVICE_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/string16.h"
#include "base/timer.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/common/net/gaia/oauth2_access_token_consumer.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"

class OAuth2AccessTokenFetcher;
class CloudPrintURL;
class Profile;

// ChromeToMobileService connects to the cloud print service to enumerate
// compatible mobiles owned by its profile and send URLs and MHTML snapshots.
// The mobile list updates regularly, and explicitly by RequestMobileListUpdate.
class ChromeToMobileService : public ProfileKeyedService,
                              public content::URLFetcherDelegate,
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

  explicit ChromeToMobileService(Profile* profile);
  virtual ~ChromeToMobileService();

  // content::URLFetcherDelegate methods.
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE;

  // OAuth2AccessTokenConsumer methods.
  virtual void OnGetTokenSuccess(const std::string& access_token) OVERRIDE;
  virtual void OnGetTokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

  // Get the list of mobile devices.
  const std::vector<base::DictionaryValue*>& mobiles() { return mobiles_; }

  // Request an updated mobile device list, request auth first if needed.
  void RequestMobileListUpdate();

  // Callback with an MHTML snapshot of the profile's selected WebContents.
  void GenerateSnapshot(base::WeakPtr<Observer> observer);

  // Send the profile's selected WebContents to the specified mobile device.
  void SendToMobile(const string16& mobile_id,
                    const FilePath& snapshot,
                    base::WeakPtr<Observer> observer);

 private:
  // Utility function to initialize the ScopedTempDir.
  void CreateUniqueTempDir();

  // Utility function to create URLFetcher requests.
  content::URLFetcher* CreateRequest(const RequestData& data);

  void RequestAuth();
  void RequestSearch();

  void HandleSearchResponse();
  void HandleSubmitResponse(const content::URLFetcher* source);

  Profile* profile_;

  // A utility class for accessing the cloud print service.
  scoped_ptr<CloudPrintURL> cloud_print_url_;

  // The list of mobile devices retrieved from the cloud print service.
  std::vector<base::DictionaryValue*> mobiles_;

  // The temporary directory for MHTML snapshot files.
  ScopedTempDir temp_dir_;

  // Map URLFetchers to observers for reporting OnSendComplete.
  typedef std::map<const content::URLFetcher*, base::WeakPtr<Observer> >
      RequestObserverMap;
  RequestObserverMap request_observer_map_;

  // The OAuth2 token and retry count.
  std::string oauth2_token_;
  size_t oauth2_retry_count_;

  // The pending URL requests.
  scoped_ptr<OAuth2AccessTokenFetcher> oauth2_request_;
  scoped_ptr<content::URLFetcher> search_request_;

  // A timer for authentication retries and mobile device list updates.
  base::OneShotTimer<ChromeToMobileService> request_timer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeToMobileService);
};

#endif  // CHROME_BROWSER_CHROME_TO_MOBILE_SERVICE_H_
