// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_fetcher.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/metrics/histogram_macros.h"
#include "components/precache/core/precache_switches.h"
#include "components/precache/core/proto/precache.pb.h"
#include "net/base/completion_callback.h"
#include "net/base/escape.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using net::URLFetcher;

namespace precache {

// The following flags are for privacy reasons. For example, if a user clears
// their cookies, but a tracking beacon is prefetched and the beacon specifies
// its source URL in a URL param, the beacon site would be able to rebuild a
// profile of the user. All three flags should occur together, or not at all,
// per
// https://groups.google.com/a/chromium.org/d/topic/net-dev/vvcodRV6SdM/discussion.
const int kNoTracking =
    net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES |
    net::LOAD_DO_NOT_SEND_AUTH_DATA;

namespace {

// The maximum for the Precache.Fetch.ResponseBytes.* histograms. We set this to
// a number we expect to be in the 99th percentile for the histogram, give or
// take.
const int kMaxResponseBytes = 500 * 1024 * 1024;

GURL GetDefaultConfigURL() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
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

std::string GetDefaultManifestURLPrefix() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
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

// Construct the URL of the precache manifest for the given name (either host or
// URL). The server is expecting a request for a URL consisting of the manifest
// URL prefix followed by the doubly escaped name.
std::string ConstructManifestURL(const std::string& prefix,
                                 const std::string& name) {
  return prefix + net::EscapeQueryParamValue(
                      net::EscapeQueryParamValue(name, false), false);
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

// URLFetcherResponseWriter that ignores the response body, in order to avoid
// the unnecessary memory usage. Use it rather than the default if you don't
// care about parsing the response body. We use it below as a means to populate
// the cache with requested resource URLs.
class URLFetcherNullWriter : public net::URLFetcherResponseWriter {
 public:
  int Initialize(const net::CompletionCallback& callback) override {
    return net::OK;
  }

  int Write(net::IOBuffer* buffer,
            int num_bytes,
            const net::CompletionCallback& callback) override {
    return num_bytes;
  }

  int Finish(const net::CompletionCallback& callback) override {
    return net::OK;
  }
};

}  // namespace

PrecacheFetcher::Fetcher::Fetcher(
    net::URLRequestContextGetter* request_context,
    const GURL& url,
    const base::Callback<void(const URLFetcher&)>& callback,
    bool is_resource_request)
    : request_context_(request_context),
      url_(url),
      callback_(callback),
      is_resource_request_(is_resource_request),
      response_bytes_(0),
      network_response_bytes_(0) {
  if (is_resource_request_)
    LoadFromCache();
  else
    LoadFromNetwork();
}

PrecacheFetcher::Fetcher::~Fetcher() {}

void PrecacheFetcher::Fetcher::LoadFromCache() {
  fetch_stage_ = FetchStage::CACHE;
  url_fetcher_cache_ = URLFetcher::Create(url_, URLFetcher::GET, this);
  url_fetcher_cache_->SetRequestContext(request_context_);
  url_fetcher_cache_->SetLoadFlags(net::LOAD_ONLY_FROM_CACHE | kNoTracking);
  scoped_ptr<URLFetcherNullWriter> null_writer(new URLFetcherNullWriter);
  url_fetcher_cache_->SaveResponseWithWriter(std::move(null_writer));
  url_fetcher_cache_->Start();
}

void PrecacheFetcher::Fetcher::LoadFromNetwork() {
  fetch_stage_ = FetchStage::NETWORK;
  url_fetcher_network_ = URLFetcher::Create(url_, URLFetcher::GET, this);
  url_fetcher_network_->SetRequestContext(request_context_);
  if (is_resource_request_) {
    // LOAD_VALIDATE_CACHE allows us to refresh Date headers for resources
    // already in the cache. The Date headers are updated from 304s as well as
    // 200s.
    url_fetcher_network_->SetLoadFlags(net::LOAD_VALIDATE_CACHE | kNoTracking);
    // We don't need a copy of the response body for resource requests. The
    // request is issued only to populate the browser cache.
    scoped_ptr<URLFetcherNullWriter> null_writer(new URLFetcherNullWriter);
    url_fetcher_network_->SaveResponseWithWriter(std::move(null_writer));
  } else {
    // Config and manifest requests do not need to be revalidated. It's okay if
    // they expire from the cache minutes after we request them.
    url_fetcher_network_->SetLoadFlags(kNoTracking);
  }
  url_fetcher_network_->Start();
}

void PrecacheFetcher::Fetcher::OnURLFetchComplete(const URLFetcher* source) {
  if (fetch_stage_ == FetchStage::CACHE &&
      (source->GetStatus().error() == net::ERR_CACHE_MISS ||
       (source->GetResponseHeaders() &&
        source->GetResponseHeaders()->HasStrongValidators()))) {
    // If the resource was not found in the cache, request it from the
    // network.
    //
    // If the resource was found in the cache, but contains validators,
    // request a refresh. The presence of validators increases the chance that
    // we get a 304 response rather than a full one, thus allowing us to
    // refresh the cache with minimal network load.
    //
    // TODO(twifkak): Add support for weak validators, which should be just as
    // likely a guarantee that the response will be a 304.
    LoadFromNetwork();
    return;
  }

  // If any of:
  // - The request was for a config or manifest.
  // - The resource was a cache hit without validators.
  // - The response came from the network.
  // Then Fetcher is done with this URL and can return control to the caller.
  response_bytes_ = source->GetReceivedResponseContentLength();
  network_response_bytes_ = source->GetTotalReceivedBytes();
  callback_.Run(*source);
}

PrecacheFetcher::PrecacheFetcher(
    const std::vector<std::string>& starting_hosts,
    net::URLRequestContextGetter* request_context,
    const GURL& config_url,
    const std::string& manifest_url_prefix,
    PrecacheFetcher::PrecacheDelegate* precache_delegate)
    : starting_hosts_(starting_hosts),
      request_context_(request_context),
      config_url_(config_url),
      manifest_url_prefix_(manifest_url_prefix),
      precache_delegate_(precache_delegate),
      total_response_bytes_(0),
      network_response_bytes_(0),
      num_manifest_urls_to_fetch_(0) {
  DCHECK(request_context_.get());  // Request context must be non-NULL.
  DCHECK(precache_delegate_);  // Precache delegate must be non-NULL.

  DCHECK_NE(GURL(), GetDefaultConfigURL())
      << "Could not determine the precache config settings URL.";
  DCHECK_NE(std::string(), GetDefaultManifestURLPrefix())
      << "Could not determine the default precache manifest URL prefix.";
}

PrecacheFetcher::~PrecacheFetcher() {
  // Number of manifests for which we have downloaded all resources.
  int manifests_completed =
      num_manifest_urls_to_fetch_ - manifest_urls_to_fetch_.size();

  // If there are resource URLs left to fetch, the last manifest is not yet
  // completed.
  if (!resource_urls_to_fetch_.empty())
    --manifests_completed;

  DCHECK_GE(manifests_completed, 0);
  int percent_completed = num_manifest_urls_to_fetch_ == 0
                              ? 0
                              : (static_cast<double>(manifests_completed) /
                                 num_manifest_urls_to_fetch_ * 100);
  UMA_HISTOGRAM_PERCENTAGE("Precache.Fetch.PercentCompleted",
                           percent_completed);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Precache.Fetch.ResponseBytes.Total",
                              total_response_bytes_, 1, kMaxResponseBytes, 100);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Precache.Fetch.ResponseBytes.Network",
                              network_response_bytes_, 1, kMaxResponseBytes,
                              100);
}

void PrecacheFetcher::Start() {
  DCHECK(!fetcher_);  // Start shouldn't be called repeatedly.

  GURL config_url =
      config_url_.is_empty() ? GetDefaultConfigURL() : config_url_;

  DCHECK(config_url.is_valid()) << "Config URL not valid: "
                                << config_url.possibly_invalid_spec();

  start_time_ = base::TimeTicks::Now();

  // Fetch the precache configuration settings from the server.
  fetcher_.reset(new Fetcher(request_context_.get(), config_url,
                             base::Bind(&PrecacheFetcher::OnConfigFetchComplete,
                                        base::Unretained(this)),
                             false /* is_resource_request */));
}

void PrecacheFetcher::StartNextFetch() {
  total_response_bytes_ += fetcher_->response_bytes();
  network_response_bytes_ += fetcher_->network_response_bytes();

  if (!resource_urls_to_fetch_.empty()) {
    // Fetch the next resource URL.
    fetcher_.reset(
        new Fetcher(request_context_.get(), resource_urls_to_fetch_.front(),
                    base::Bind(&PrecacheFetcher::OnResourceFetchComplete,
                               base::Unretained(this)),
                    true /* is_resource_request */));

    resource_urls_to_fetch_.pop_front();
    return;
  }

  if (!manifest_urls_to_fetch_.empty()) {
    // Fetch the next manifest URL.
    fetcher_.reset(
        new Fetcher(request_context_.get(), manifest_urls_to_fetch_.front(),
                    base::Bind(&PrecacheFetcher::OnManifestFetchComplete,
                               base::Unretained(this)),
                    false /* is_resource_request */));

    manifest_urls_to_fetch_.pop_front();
    return;
  }

  // There are no more URLs to fetch, so end the precache cycle.
  base::TimeDelta time_to_fetch = base::TimeTicks::Now() - start_time_;
  UMA_HISTOGRAM_CUSTOM_TIMES("Precache.Fetch.TimeToComplete", time_to_fetch,
                             base::TimeDelta::FromSeconds(1),
                             base::TimeDelta::FromHours(4), 50);

  precache_delegate_->OnDone();
  // OnDone may have deleted this PrecacheFetcher, so don't do anything after it
  // is called.
}

void PrecacheFetcher::OnConfigFetchComplete(const URLFetcher& source) {
  // Attempt to parse the config proto. On failure, continue on with the default
  // configuration.
  PrecacheConfigurationSettings config;
  ParseProtoFromFetchResponse(source, &config);

  std::string prefix = manifest_url_prefix_.empty()
                           ? GetDefaultManifestURLPrefix()
                           : manifest_url_prefix_;
  DCHECK_NE(std::string(), prefix)
      << "Could not determine the precache manifest URL prefix.";

  // Keep track of manifest URLs that are being fetched, in order to remove
  // duplicates.
  base::hash_set<std::string> unique_manifest_urls;

  // Attempt to fetch manifests for starting hosts up to the maximum top sites
  // count. If a manifest does not exist for a particular starting host, then
  // the fetch will fail, and that starting host will be ignored.
  int64_t rank = 0;
  for (const std::string& host : starting_hosts_) {
    ++rank;
    if (rank > config.top_sites_count())
      break;
    unique_manifest_urls.insert(ConstructManifestURL(prefix, host));
  }

  for (const std::string& url : config.forced_site())
    unique_manifest_urls.insert(ConstructManifestURL(prefix, url));

  for (const std::string& manifest_url : unique_manifest_urls)
    manifest_urls_to_fetch_.push_back(GURL(manifest_url));
  num_manifest_urls_to_fetch_ = manifest_urls_to_fetch_.size();

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
