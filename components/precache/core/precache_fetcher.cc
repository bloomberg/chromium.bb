// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_fetcher.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "components/precache/core/precache_switches.h"
#include "components/precache/core/proto/precache.pb.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using net::URLFetcher;

namespace precache {

namespace {

GURL GetConfigURL() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kPrecacheConfigSettingsURL)) {
    return GURL(
        command_line.GetSwitchValueASCII(switches::kPrecacheConfigSettingsURL));
  }

#if defined(PRECACHE_CONFIG_SETTINGS_URL)
  return GURL(PRECACHE_CONFIG_SETTINGS_URL);
#else
  // The precache config settings URL could not be determined, so return an
  // empty, invalid GURL.
  return GURL();
#endif
}

std::string GetManifestURLPrefix() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kPrecacheManifestURLPrefix)) {
    return command_line.GetSwitchValueASCII(
        switches::kPrecacheManifestURLPrefix);
  }

#if defined(PRECACHE_MANIFEST_URL_PREFIX)
  return PRECACHE_MANIFEST_URL_PREFIX;
#else
  // The precache manifest URL prefix could not be determined, so return an
  // empty string.
  return std::string();
#endif
}

// Construct the URL of the precache manifest for the given starting URL.
// The server is expecting a request for a URL consisting of the manifest URL
// prefix followed by the doubly escaped starting URL.
GURL ConstructManifestURL(const GURL& starting_url) {
  return GURL(
      GetManifestURLPrefix() +
      net::EscapeQueryParamValue(
          net::EscapeQueryParamValue(starting_url.spec(), false), false));
}

// Attempts to parse a protobuf message from the response string of a
// URLFetcher. If parsing is successful, the message parameter will contain the
// parsed protobuf and this function will return true. Otherwise, returns false.
bool ParseProtoFromFetchResponse(const URLFetcher& source,
                                 ::google::protobuf::MessageLite* message) {
  std::string response_string;

  if (!source.GetStatus().is_success()) {
    DLOG(WARNING) << "Fetch failed: " << source.GetOriginalURL().spec();
    return false;
  }
  if (!source.GetResponseAsString(&response_string)) {
    DLOG(WARNING) << "No response string present: "
                  << source.GetOriginalURL().spec();
    return false;
  }
  if (!message->ParseFromString(response_string)) {
    DLOG(WARNING) << "Unable to parse proto served from "
                  << source.GetOriginalURL().spec();
    return false;
  }
  return true;
}

}  // namespace

// Class that fetches a URL, and runs the specified callback when the fetch is
// complete. This class exists so that a different method can be run in
// response to different kinds of fetches, e.g. OnConfigFetchComplete when
// configuration settings are fetched, OnManifestFetchComplete when a manifest
// is fetched, etc.
class PrecacheFetcher::Fetcher : public net::URLFetcherDelegate {
 public:
  // Construct a new Fetcher. This will create and start a new URLFetcher for
  // the specified URL using the specified request context.
  Fetcher(net::URLRequestContextGetter* request_context, const GURL& url,
          const base::Callback<void(const URLFetcher&)>& callback);
  virtual ~Fetcher() {}
  virtual void OnURLFetchComplete(const URLFetcher* source) OVERRIDE;

 private:
  const base::Callback<void(const URLFetcher&)> callback_;
  scoped_ptr<URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(Fetcher);
};

PrecacheFetcher::Fetcher::Fetcher(
    net::URLRequestContextGetter* request_context, const GURL& url,
    const base::Callback<void(const URLFetcher&)>& callback)
    : callback_(callback) {
  url_fetcher_.reset(URLFetcher::Create(url, URLFetcher::GET, this));
  url_fetcher_->SetRequestContext(request_context);
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_PROMPT_FOR_LOGIN);
  url_fetcher_->Start();
}

void PrecacheFetcher::Fetcher::OnURLFetchComplete(const URLFetcher* source) {
  callback_.Run(*source);
}

PrecacheFetcher::PrecacheFetcher(
    const std::list<GURL>& starting_urls,
    net::URLRequestContextGetter* request_context,
    PrecacheFetcher::PrecacheDelegate* precache_delegate)
    : starting_urls_(starting_urls),
      request_context_(request_context),
      precache_delegate_(precache_delegate) {
  DCHECK(request_context_.get());  // Request context must be non-NULL.
  DCHECK(precache_delegate_);  // Precache delegate must be non-NULL.

  DCHECK_NE(GURL(), GetConfigURL())
      << "Could not determine the precache config settings URL.";
  DCHECK_NE(std::string(), GetManifestURLPrefix())
      << "Could not determine the precache manifest URL prefix.";
}

PrecacheFetcher::~PrecacheFetcher() {
}

void PrecacheFetcher::Start() {
  DCHECK(!fetcher_);  // Start shouldn't be called repeatedly.

  GURL config_url = GetConfigURL();
  DCHECK(config_url.is_valid());

  // Fetch the precache configuration settings from the server.
  fetcher_.reset(new Fetcher(request_context_.get(),
                             config_url,
                             base::Bind(&PrecacheFetcher::OnConfigFetchComplete,
                                        base::Unretained(this))));
}

void PrecacheFetcher::StartNextFetch() {
  if (!resource_urls_to_fetch_.empty()) {
    // Fetch the next resource URL.
    fetcher_.reset(
        new Fetcher(request_context_.get(),
                    resource_urls_to_fetch_.front(),
                    base::Bind(&PrecacheFetcher::OnResourceFetchComplete,
                               base::Unretained(this))));

    resource_urls_to_fetch_.pop_front();
    return;
  }

  if (!manifest_urls_to_fetch_.empty()) {
    // Fetch the next manifest URL.
    fetcher_.reset(
        new Fetcher(request_context_.get(),
                    manifest_urls_to_fetch_.front(),
                    base::Bind(&PrecacheFetcher::OnManifestFetchComplete,
                               base::Unretained(this))));

    manifest_urls_to_fetch_.pop_front();
    return;
  }

  // There are no more URLs to fetch, so end the precache cycle.
  precache_delegate_->OnDone();
  // OnDone may have deleted this PrecacheFetcher, so don't do anything after it
  // is called.
}

void PrecacheFetcher::OnConfigFetchComplete(const URLFetcher& source) {
  PrecacheConfigurationSettings config;

  if (ParseProtoFromFetchResponse(source, &config)) {
    // Keep track of starting URLs that manifests are being fetched for, in
    // order to remove duplicates. This is a hash set on strings, and not GURLs,
    // because there is no hash function defined for GURL.
    base::hash_set<std::string> unique_starting_urls;

    // Attempt to fetch manifests for starting URLs up to the maximum top sites
    // count. If a manifest does not exist for a particular starting URL, then
    // the fetch will fail, and that starting URL will be ignored.
    int64 rank = 0;
    for (std::list<GURL>::const_iterator it = starting_urls_.begin();
         it != starting_urls_.end() && rank < config.top_sites_count();
         ++it, ++rank) {
      if (unique_starting_urls.find(it->spec()) == unique_starting_urls.end()) {
        // Only add a fetch for the manifest URL if this manifest isn't already
        // going to be fetched.
        manifest_urls_to_fetch_.push_back(ConstructManifestURL(*it));
        unique_starting_urls.insert(it->spec());
      }
    }

    for (int i = 0; i < config.forced_starting_url_size(); ++i) {
      // Convert the string URL into a GURL and take the spec() of it so that
      // the URL string gets canonicalized.
      GURL url(config.forced_starting_url(i));
      if (unique_starting_urls.find(url.spec()) == unique_starting_urls.end()) {
        // Only add a fetch for the manifest URL if this manifest isn't already
        // going to be fetched.
        manifest_urls_to_fetch_.push_back(ConstructManifestURL(url));
        unique_starting_urls.insert(url.spec());
      }
    }
  }

  StartNextFetch();
}

void PrecacheFetcher::OnManifestFetchComplete(const URLFetcher& source) {
  PrecacheManifest manifest;

  if (ParseProtoFromFetchResponse(source, &manifest)) {
    for (int i = 0; i < manifest.resource_size(); ++i) {
      if (manifest.resource(i).has_url()) {
        resource_urls_to_fetch_.push_back(GURL(manifest.resource(i).url()));
      }
    }
  }

  StartNextFetch();
}

void PrecacheFetcher::OnResourceFetchComplete(const URLFetcher& source) {
  // The resource has already been put in the cache during the fetch process, so
  // nothing more needs to be done for the resource.
  StartNextFetch();
}

}  // namespace precache
