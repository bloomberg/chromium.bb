// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_RESOURCE_WEB_RESOURCE_SERVICE_H_
#define CHROME_BROWSER_WEB_RESOURCE_WEB_RESOURCE_SERVICE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_resource/json_asynchronous_unpacker.h"
#include "chrome/browser/web_resource/resource_request_allowed_notifier.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

class PrefService;

namespace base {
class DictionaryValue;
}

namespace net {
class URLFetcher;
}

// A WebResourceService fetches JSON data from a web server and periodically
// refreshes it.
class WebResourceService
    : public net::URLFetcherDelegate,
      public JSONAsynchronousUnpackerDelegate,
      public base::RefCountedThreadSafe<WebResourceService>,
      public ResourceRequestAllowedNotifier::Observer {
 public:
  WebResourceService(PrefService* prefs,
                     const GURL& web_resource_server,
                     bool apply_locale_to_url_,
                     const char* last_update_time_pref_name,
                     int start_fetch_delay_ms,
                     int cache_update_delay_ms);

  // Sleep until cache needs to be updated, but always for at least
  // |start_fetch_delay_ms| so we don't interfere with startup.
  // Then begin updating resources.
  void StartAfterDelay();

  // JSONAsynchronousUnpackerDelegate methods.
  virtual void OnUnpackFinished(
      const base::DictionaryValue& parsed_json) OVERRIDE;
  virtual void OnUnpackError(const std::string& error_message) OVERRIDE;

 protected:
  virtual ~WebResourceService();

  // For the subclasses to process the result of a fetch.
  virtual void Unpack(const base::DictionaryValue& parsed_json) = 0;

  PrefService* prefs_;

 private:
  class UnpackerClient;
  friend class base::RefCountedThreadSafe<WebResourceService>;

  // net::URLFetcherDelegate implementation:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Schedules a fetch after |delay_ms| milliseconds.
  void ScheduleFetch(int64 delay_ms);

  // Starts fetching data from the server.
  void StartFetch();

  // Set |in_fetch_| to false, clean up temp directories (in the future).
  void EndFetch();

  // Implements ResourceRequestAllowedNotifier::Observer.
  virtual void OnResourceRequestsAllowed() OVERRIDE;

  // Helper class used to tell this service if it's allowed to make network
  // resource requests.
  ResourceRequestAllowedNotifier resource_request_allowed_notifier_;

  // The tool that fetches the url data from the server.
  scoped_ptr<net::URLFetcher> url_fetcher_;

  // The tool that parses and transforms the json data. Weak reference as it
  // deletes itself once the unpack is done.
  JSONAsynchronousUnpacker* json_unpacker_;

  // True if we are currently fetching or unpacking data. If we are asked to
  // start a fetch when we are still fetching resource data, schedule another
  // one in |cache_update_delay_ms_| time, and silently exit.
  bool in_fetch_;

  // URL that hosts the web resource.
  GURL web_resource_server_;

  // Indicates whether we should append locale to the web resource server URL.
  bool apply_locale_to_url_;

  // Pref name to store the last update's time.
  const char* last_update_time_pref_name_;

  // Delay on first fetch so we don't interfere with startup.
  int start_fetch_delay_ms_;

  // Delay between calls to update the web resource cache. This delay may be
  // different for different builds of Chrome.
  int cache_update_delay_ms_;

  // So that we can delay our start so as not to affect start-up time; also,
  // so that we can schedule future cache updates.
  base::WeakPtrFactory<WebResourceService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebResourceService);
};

#endif  // CHROME_BROWSER_WEB_RESOURCE_WEB_RESOURCE_SERVICE_H_
