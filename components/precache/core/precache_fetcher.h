// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRECACHE_CORE_PRECACHE_FETCHER_H_
#define COMPONENTS_PRECACHE_CORE_PRECACHE_FETCHER_H_

#include <list>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace precache {

// Public interface to code that fetches resources that the user is likely to
// want to fetch in the future, putting them in the network stack disk cache.
// Precaching is intended to be done when Chrome is not actively in use, likely
// hours ahead of the time when the resources are actually needed.
//
// This class takes as input a prioritized list of page URLs that the user
// commonly visits, referred to as starting URLs. This class interacts with a
// server, sending it the list of starting URLs sequentially. For each starting
// URL, the server returns a manifest of resource URLs that are good candidates
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

  // Constructs a new PrecacheFetcher. The |starting_urls| parameter is a
  // prioritized list of page URLs that the user commonly visits. These URLs are
  // used by a server side component to construct a list of resource URLs that
  // the user is likely to fetch.
  PrecacheFetcher(const std::list<GURL>& starting_urls,
                  net::URLRequestContextGetter* request_context,
                  PrecacheDelegate* precache_delegate);

  virtual ~PrecacheFetcher();

  // Starts fetching resources to precache. URLs are fetched sequentially. Can
  // be called from any thread. Start should only be called once on a
  // PrecacheFetcher instance.
  void Start();

 private:
  class Fetcher;

  // Fetches the next resource or manifest URL, if any remain. Fetching is done
  // sequentially and depth-first: all resources are fetched for a manifest
  // before the next manifest is fetched. This is done to limit the length of
  // the |resource_urls_to_fetch_| list, reducing the memory usage.
  void StartNextFetch();

  // Called when the precache configuration settings have been fetched.
  // Determines the list of manifest URLs to fetch according to the list of
  // |starting_urls_| and information from the precache configuration settings.
  // If the fetch of the configuration settings fails, then precaching ends.
  void OnConfigFetchComplete(const net::URLFetcher& source);

  // Called when a precache manifest has been fetched. Builds the list of
  // resource URLs to fetch according to the URLs in the manifest. If the fetch
  // of a manifest fails, then it skips to the next manifest.
  void OnManifestFetchComplete(const net::URLFetcher& source);

  // Called when a resource has been fetched.
  void OnResourceFetchComplete(const net::URLFetcher& source);

  // The prioritized list of starting URLs that the server will pick resource
  // URLs to be precached for.
  const std::list<GURL> starting_urls_;

  // The request context used when fetching URLs.
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  // Non-owning pointer. Should not be NULL.
  PrecacheDelegate* precache_delegate_;

  scoped_ptr<Fetcher> fetcher_;

  std::list<GURL> manifest_urls_to_fetch_;
  std::list<GURL> resource_urls_to_fetch_;

  DISALLOW_COPY_AND_ASSIGN(PrecacheFetcher);
};

}  // namespace precache

#endif  // COMPONENTS_PRECACHE_CORE_PRECACHE_FETCHER_H_
