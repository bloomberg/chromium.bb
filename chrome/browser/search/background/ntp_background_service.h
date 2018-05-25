// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/search/background/ntp_background_data.h"
#include "chrome/browser/search/background/ntp_background_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

// A service that connects to backends that provide background image
// information, including collection names, image urls and descriptions.
class NtpBackgroundService : public KeyedService {
 public:
  NtpBackgroundService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const base::Optional<GURL>& api_url_override);
  ~NtpBackgroundService() override;

  // Requests an asynchronous fetch from the network. After the update
  // completes, OnCollectionInfoUpdated will be called on the observers.
  void FetchCollectionInfo();

  // Returns the currently cached CollectionInfo, if any.
  const std::vector<CollectionInfo>& collection_info() const {
    return collection_info_;
  }

  // Add/remove observers. All observers must unregister themselves before the
  // NtpBackgroundService is destroyed.
  void AddObserver(NtpBackgroundServiceObserver* observer);
  void RemoveObserver(NtpBackgroundServiceObserver* observer);

  GURL GetLoadURLForTesting() const;

 private:
  GURL api_url_;

  // Used to download the proto from the Backdrop service.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;

  base::ObserverList<NtpBackgroundServiceObserver, true> observers_;

  // Callback that processes the response from the FetchCollectionInfo request,
  // refreshing the contents of collection_info_data_ with server-provided data.
  void OnCollectionInfoFetchComplete(
      const std::unique_ptr<std::string> response_body);

  void NotifyObservers();

  std::vector<CollectionInfo> collection_info_;

  DISALLOW_COPY_AND_ASSIGN(NtpBackgroundService);
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_H_
