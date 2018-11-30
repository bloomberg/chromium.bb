// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_PROMOS_PROMO_SERVICE_H_
#define CHROME_BROWSER_SEARCH_PROMOS_PROMO_SERVICE_H_

#include <memory>

#include "base/observer_list.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/search/promos/promo_data.h"
#include "chrome/browser/search/promos/promo_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class GoogleURLTracker;
class GURL;

enum class Status {
  // Received a valid response.
  OK,
  // Some transient error occurred, e.g. the network request failed because
  // there is no network connectivity. A previously cached response may still
  // be used.
  TRANSIENT_ERROR,
  // A fatal error occurred, such as the server responding with an error code
  // or with invalid data. Any previously cached response should be cleared.
  FATAL_ERROR
};

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

// A service that downloads, caches, and hands out PromoData for middle-slot
// promos. It never initiates a download automatically, only when Refresh is
// called.
class PromoService : public KeyedService {
 public:
  PromoService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GoogleURLTracker* google_url_tracker);
  ~PromoService() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Returns the currently cached middle-slot PromoData, if any.
  const base::Optional<PromoData>& promo_data() const { return promo_data_; }
  Status promo_status() const { return promo_status_; }

  // Requests an asynchronous refresh from the network. After the update
  // completes, OnPromoDataUpdated will be called on the observers.
  void Refresh();

  // Add/remove observers. All observers must unregister themselves before the
  // PromoService is destroyed.
  void AddObserver(PromoServiceObserver* observer);
  void RemoveObserver(PromoServiceObserver* observer);

  GURL GetLoadURLForTesting() const;

 private:
  void PromoDataLoaded(Status status, const base::Optional<PromoData>& data);
  void LoadDone(std::unique_ptr<std::string> response_body);
  void JsonParsed(std::unique_ptr<base::Value> value);
  void JsonParseFailed(const std::string& message);

  void NotifyObservers();

  GURL GetApiUrl() const;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;

  GoogleURLTracker* google_url_tracker_;

  base::ObserverList<PromoServiceObserver, true>::Unchecked observers_;

  base::Optional<PromoData> promo_data_;
  Status promo_status_;

  base::WeakPtrFactory<PromoService> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_SEARCH_PROMOS_PROMO_SERVICE_H_
