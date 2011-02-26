// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_RESOURCE_WEB_RESOURCE_SERVICE_H_
#define CHROME_BROWSER_WEB_RESOURCE_WEB_RESOURCE_SERVICE_H_
#pragma once

#include <string>

#include "chrome/browser/utility_process_host.h"
#include "chrome/common/notification_type.h"

class PrefService;
class Profile;

// A WebResourceService fetches data from a web resource server and store
// locally as user preference.
class WebResourceService
    : public UtilityProcessHost::Client {
 public:
  WebResourceService(Profile* profile,
                     const char* web_resource_server,
                     bool apply_locale_to_url_,
                     NotificationType::Type notification_type,
                     const char* last_update_time_pref_name,
                     int start_fetch_delay,
                     int cache_update_delay);

  // Sleep until cache needs to be updated, but always for at least 5 seconds
  // so we don't interfere with startup.  Then begin updating resources.
  void StartAfterDelay();

  // We have successfully pulled data from a resource server; now launch
  // the process that will parse the JSON, and then update the cache.
  void UpdateResourceCache(const std::string& json_data);

 protected:
  virtual ~WebResourceService();

  virtual void Unpack(const DictionaryValue& parsed_json) = 0;

  // If delay_ms is positive, schedule notification with the delay.
  // If delay_ms is 0, notify immediately by calling WebResourceStateChange().
  // If delay_ms is negative, do nothing.
  void PostNotification(int64 delay_ms);

  // We need to be able to load parsed resource data into preferences file,
  // and get proper install directory.
  PrefService* prefs_;

 private:
  class WebResourceFetcher;
  friend class WebResourceFetcher;

  class UnpackerClient;

  // Set in_fetch_ to false, clean up temp directories (in the future).
  void EndFetch();

  // Puts parsed json data in the right places, and writes to prefs file.
  void OnWebResourceUnpacked(const DictionaryValue& parsed_json);

  // Notify listeners that the state of a web resource has changed.
  void WebResourceStateChange();

  Profile* profile_;

  scoped_ptr<WebResourceFetcher> web_resource_fetcher_;

  ResourceDispatcherHost* resource_dispatcher_host_;

  // Allows the creation of tasks to send a WEB_RESOURCE_STATE_CHANGED
  // notification. This allows the WebResourceService to notify the New Tab
  // Page immediately when a new web resource should be shown or removed.
  ScopedRunnableMethodFactory<WebResourceService> service_factory_;

  // True if we are currently mid-fetch.  If we are asked to start a fetch
  // when we are still fetching resource data, schedule another one in
  // kCacheUpdateDelay time, and silently exit.
  bool in_fetch_;

  // URL that hosts the web resource.
  const char* web_resource_server_;

  // Indicates whether we should append locale to the web resource server URL.
  bool apply_locale_to_url_;

  // Notification type when an update is done.
  NotificationType::Type notification_type_;

  // Pref name to store the last update's time.
  const char* last_update_time_pref_name_;

  // Delay on first fetch so we don't interfere with startup.
  int start_fetch_delay_;

  // Delay between calls to update the web resource cache. This delay may be
  // different for different builds of Chrome.
  int cache_update_delay_;

  // True if a task has been set to update the cache when a new web resource
  // becomes available.
  bool web_resource_update_scheduled_;

  DISALLOW_COPY_AND_ASSIGN(WebResourceService);
};

#endif  // CHROME_BROWSER_WEB_RESOURCE_WEB_RESOURCE_SERVICE_H_
