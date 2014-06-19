// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/web_resource_service.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "components/google/core/browser/google_util.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

WebResourceService::WebResourceService(
    PrefService* prefs,
    const GURL& web_resource_server,
    bool apply_locale_to_url,
    const char* last_update_time_pref_name,
    int start_fetch_delay_ms,
    int cache_update_delay_ms)
    : prefs_(prefs),
      json_unpacker_(NULL),
      in_fetch_(false),
      web_resource_server_(web_resource_server),
      apply_locale_to_url_(apply_locale_to_url),
      last_update_time_pref_name_(last_update_time_pref_name),
      start_fetch_delay_ms_(start_fetch_delay_ms),
      cache_update_delay_ms_(cache_update_delay_ms),
      weak_ptr_factory_(this) {
  resource_request_allowed_notifier_.Init(this);
  DCHECK(prefs);
}

WebResourceService::~WebResourceService() {
  if (in_fetch_)
    EndFetch();
}

void WebResourceService::OnUnpackFinished(
    const base::DictionaryValue& parsed_json) {
  Unpack(parsed_json);
  EndFetch();
}

void WebResourceService::OnUnpackError(const std::string& error_message) {
  LOG(ERROR) << error_message;
  EndFetch();
}

void WebResourceService::EndFetch() {
  if (json_unpacker_) {
    json_unpacker_->ClearDelegate();
    json_unpacker_ = NULL;
  }
  in_fetch_ = false;
}

void WebResourceService::StartAfterDelay() {
  // If resource requests are not allowed, we'll get a callback when they are.
  if (resource_request_allowed_notifier_.ResourceRequestsAllowed())
    OnResourceRequestsAllowed();
}

void WebResourceService::OnResourceRequestsAllowed() {
  int64 delay = start_fetch_delay_ms_;
  // Check whether we have ever put a value in the web resource cache;
  // if so, pull it out and see if it's time to update again.
  if (prefs_->HasPrefPath(last_update_time_pref_name_)) {
    std::string last_update_pref =
        prefs_->GetString(last_update_time_pref_name_);
    if (!last_update_pref.empty()) {
      double last_update_value;
      base::StringToDouble(last_update_pref, &last_update_value);
      int64 ms_until_update = cache_update_delay_ms_ -
          static_cast<int64>((base::Time::Now() - base::Time::FromDoubleT(
          last_update_value)).InMilliseconds());
      // Wait at least |start_fetch_delay_ms_|.
      if (ms_until_update > start_fetch_delay_ms_)
        delay = ms_until_update;
    }
  }
  // Start fetch and wait for UpdateResourceCache.
  ScheduleFetch(delay);
}

// Delay initial load of resource data into cache so as not to interfere
// with startup time.
void WebResourceService::ScheduleFetch(int64 delay_ms) {
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WebResourceService::StartFetch,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

// Initializes the fetching of data from the resource server.  Data
// load calls OnURLFetchComplete.
void WebResourceService::StartFetch() {
  // First, put our next cache load on the MessageLoop.
  ScheduleFetch(cache_update_delay_ms_);

  // Set cache update time in preferences.
  prefs_->SetString(last_update_time_pref_name_,
      base::DoubleToString(base::Time::Now().ToDoubleT()));

  // If we are still fetching data, exit.
  if (in_fetch_)
    return;
  in_fetch_ = true;

  // Balanced in OnURLFetchComplete.
  AddRef();

  GURL web_resource_server =
      apply_locale_to_url_
          ? google_util::AppendGoogleLocaleParam(
                web_resource_server_, g_browser_process->GetApplicationLocale())
          : web_resource_server_;

  DVLOG(1) << "WebResourceService StartFetch " << web_resource_server;
  url_fetcher_.reset(net::URLFetcher::Create(
      web_resource_server, net::URLFetcher::GET, this));
  // Do not let url fetcher affect existing state in system context
  // (by setting cookies, for example).
  url_fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE |
                             net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SAVE_COOKIES);
  net::URLRequestContextGetter* url_request_context_getter =
      g_browser_process->system_request_context();
  url_fetcher_->SetRequestContext(url_request_context_getter);
  url_fetcher_->Start();
}

void WebResourceService::OnURLFetchComplete(const net::URLFetcher* source) {
  // Delete the URLFetcher when this function exits.
  scoped_ptr<net::URLFetcher> clean_up_fetcher(url_fetcher_.release());

  if (source->GetStatus().is_success() && source->GetResponseCode() == 200) {
    std::string data;
    source->GetResponseAsString(&data);

    // UnpackerClient calls EndFetch and releases itself on completion.
    json_unpacker_ = JSONAsynchronousUnpacker::Create(this);
    json_unpacker_->Start(data);
  } else {
    // Don't parse data if attempt to download was unsuccessful.
    // Stop loading new web resource data, and silently exit.
    // We do not call UnpackerClient, so we need to call EndFetch ourselves.
    EndFetch();
  }

  Release();
}
