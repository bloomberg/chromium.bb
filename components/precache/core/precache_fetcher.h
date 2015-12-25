// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRECACHE_CORE_PRECACHE_FETCHER_H_
#define COMPONENTS_PRECACHE_CORE_PRECACHE_FETCHER_H_

#include <stdint.h>

#include <list>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}

namespace precache {

// Visible for testing.
extern const int kNoTracking;

// Public interface to code that fetches resources that the user is likely to
// want to fetch in the future, putting them in the network stack disk cache.
// Precaching is intended to be done when Chrome is not actively in use, likely
// hours ahead of the time when the resources are actually needed.
//
// This class takes as input a prioritized list of URL domains that the user
// commonly visits, referred to as starting hosts. This class interacts with a
// server, sending it the list of starting hosts sequentially. For each starting
// host, the server returns a manifest of resource URLs that are good candidates
// for precaching. Every resource returned is fetched, and responses are cached
// as they are received. Destroying the PrecacheFetcher while it is precaching
// will cancel any fetch in progress and cancel precaching.
//
// The URLs of the server-side component must be specified in order for the
// PrecacheFetcher to work. This includes the URL that the precache
// configuration settings are fetched from and the prefix of URLs where precache
// manifests are fetched from. These can be set by using command line switches
// or by providing default values.
//
// Sample interaction:
//
// class MyPrecacheFetcherDelegate : public PrecacheFetcher::PrecacheDelegate {
//  public:
//   void PrecacheResourcesForTopURLs(
//       net::URLRequestContextGetter* request_context,
//       const std::list<GURL>& top_urls) {
//     fetcher_.reset(new PrecacheFetcher(request_context, top_urls, this));
//     fetcher_->Start();
//   }
//
//   virtual void OnDone() {
//     // Do something when precaching is done.
//   }
//
//  private:
//   scoped_ptr<PrecacheFetcher> fetcher_;
// };
class PrecacheFetcher {
 public:
  class PrecacheDelegate {
   public:
    // Called when the fetching of resources has finished, whether the resources
    // were fetched or not. If the PrecacheFetcher is destroyed before OnDone is
    // called, then precaching will be canceled and OnDone will not be called.
    virtual void OnDone() = 0;
  };

  // Visible for testing.
  class Fetcher;

  // Constructs a new PrecacheFetcher. The |starting_hosts| parameter is a
  // prioritized list of hosts that the user commonly visits. These hosts are
  // used by a server side component to construct a list of resource URLs that
  // the user is likely to fetch.
  PrecacheFetcher(const std::vector<std::string>& starting_hosts,
                  net::URLRequestContextGetter* request_context,
                  const GURL& config_url,
                  const std::string& manifest_url_prefix,
                  PrecacheDelegate* precache_delegate);

  virtual ~PrecacheFetcher();

  // Starts fetching resources to precache. URLs are fetched sequentially. Can
  // be called from any thread. Start should only be called once on a
  // PrecacheFetcher instance.
  void Start();

 private:
  // Fetches the next resource or manifest URL, if any remain. Fetching is done
  // sequentially and depth-first: all resources are fetched for a manifest
  // before the next manifest is fetched. This is done to limit the length of
  // the |resource_urls_to_fetch_| list, reducing the memory usage.
  void StartNextFetch();

  // Called when the precache configuration settings have been fetched.
  // Determines the list of manifest URLs to fetch according to the list of
  // |starting_hosts_| and information from the precache configuration settings.
  // If the fetch of the configuration settings fails, then precaching ends.
  void OnConfigFetchComplete(const net::URLFetcher& source);

  // Called when a precache manifest has been fetched. Builds the list of
  // resource URLs to fetch according to the URLs in the manifest. If the fetch
  // of a manifest fails, then it skips to the next manifest.
  void OnManifestFetchComplete(const net::URLFetcher& source);

  // Called when a resource has been fetched.
  void OnResourceFetchComplete(const net::URLFetcher& source);

  // The prioritized list of starting hosts that the server will pick resource
  // URLs to be precached for.
  const std::vector<std::string> starting_hosts_;

  // The request context used when fetching URLs.
  const scoped_refptr<net::URLRequestContextGetter> request_context_;

  // The custom URL to use when fetching the config. If not provided, the
  // default flag-specified URL will be used.
  const GURL config_url_;

  // The custom URL prefix to use when fetching manifests. If not provided, the
  // default flag-specified prefix will be used.
  const std::string manifest_url_prefix_;

  // Non-owning pointer. Should not be NULL.
  PrecacheDelegate* precache_delegate_;

  // Tally of the total number of bytes contained in URL fetches, including
  // config, manifests, and resources. This the number of bytes as they would be
  // compressed over the network.
  int total_response_bytes_;

  // Tally of the total number of bytes received over the network from URL
  // fetches (the same ones as in total_response_bytes_). This includes response
  // headers and intermediate responses such as 30xs.
  int network_response_bytes_;

  scoped_ptr<Fetcher> fetcher_;

  // Time when the prefetch was started.
  base::TimeTicks start_time_;

  int num_manifest_urls_to_fetch_;
  std::list<GURL> manifest_urls_to_fetch_;
  std::list<GURL> resource_urls_to_fetch_;

  DISALLOW_COPY_AND_ASSIGN(PrecacheFetcher);
};

// Class that fetches a URL, and runs the specified callback when the fetch is
// complete. This class exists so that a different method can be run in
// response to different kinds of fetches, e.g. OnConfigFetchComplete when
// configuration settings are fetched, OnManifestFetchComplete when a manifest
// is fetched, etc.
class PrecacheFetcher::Fetcher : public net::URLFetcherDelegate {
 public:
  // Construct a new Fetcher. This will create and start a new URLFetcher for
  // the specified URL using the specified request context.
  Fetcher(net::URLRequestContextGetter* request_context,
          const GURL& url,
          const base::Callback<void(const net::URLFetcher&)>& callback,
          bool is_resource_request);
  ~Fetcher() override;
  void OnURLFetchComplete(const net::URLFetcher* source) override;
  int64_t response_bytes() const { return response_bytes_; }
  int64_t network_response_bytes() const { return network_response_bytes_; }

 private:
  enum class FetchStage { CACHE, NETWORK };

  void LoadFromCache();
  void LoadFromNetwork();

  net::URLRequestContextGetter* const request_context_;
  const GURL url_;
  const base::Callback<void(const net::URLFetcher&)> callback_;
  const bool is_resource_request_;

  FetchStage fetch_stage_;
  // The url_fetcher_cache_ is kept alive until Fetcher destruction for testing.
  scoped_ptr<net::URLFetcher> url_fetcher_cache_;
  scoped_ptr<net::URLFetcher> url_fetcher_network_;
  int64_t response_bytes_;
  int64_t network_response_bytes_;

  DISALLOW_COPY_AND_ASSIGN(Fetcher);
};

}  // namespace precache

#endif  // COMPONENTS_PRECACHE_CORE_PRECACHE_FETCHER_H_
