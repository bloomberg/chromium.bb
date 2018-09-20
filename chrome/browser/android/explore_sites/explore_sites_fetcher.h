// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_FETCHER_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace explore_sites {

// A class that fetches data from the server.
class ExploreSitesFetcher {
 public:
  // Callback to pass back the catalog returned from the server.
  // Invoked with |nullptr| if there is an error.
  using Callback =
      base::OnceCallback<void(ExploreSitesRequestStatus status,
                              const std::unique_ptr<std::string> data)>;

  // Creates a fetcher for the GetCatalog RPC.
  static std::unique_ptr<ExploreSitesFetcher> CreateForGetCatalog(
      Callback callback,
      const int64_t catalog_version,
      const std::string country_code,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory);

  // Creates a fetcher for the GetCategories RPC.
  static std::unique_ptr<ExploreSitesFetcher> CreateForGetCategories(
      Callback callback,
      const int64_t catalog_version,
      const std::string country_code,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory);

  ~ExploreSitesFetcher();

 private:
  explicit ExploreSitesFetcher(
      Callback callback,
      const GURL& url,
      const int64_t catalog_version,
      const std::string country_code,
      scoped_refptr<network ::SharedURLLoaderFactory> loader_factory);

  // Invoked from SimpleURLLoader after download is complete.
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  ExploreSitesRequestStatus HandleResponseCode();

  Callback callback_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  base::WeakPtrFactory<ExploreSitesFetcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesFetcher);
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_FETCHER_H_
