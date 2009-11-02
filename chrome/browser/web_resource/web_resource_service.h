// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_RESOURCE_WEB_RESOURCE_SERVICE_H_
#define CHROME_BROWSER_WEB_RESOURCE_WEB_RESOURCE_SERVICE_H_

#include <string>

#include "chrome/browser/utility_process_host.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/web_resource/web_resource_unpacker.h"

class Profile;

class WebResourceService
    : public UtilityProcessHost::Client {
 public:
  WebResourceService(Profile* profile);
  ~WebResourceService();

  // Sleep until cache needs to be updated, but always for at least 5 seconds
  // so we don't interfere with startup.  Then begin updating resources.
  void StartAfterDelay();

  // We have successfully pulled data from a resource server; now launch
  // the process that will parse the JSON, and then update the cache.
  void UpdateResourceCache(const std::string& json_data);

  // Get the language appropriate for delivery of the web resources.
  static std::wstring GetWebResourceLanguage(PrefService* prefs);

  static const wchar_t* kTipDictionaryPrefName;
  static const wchar_t* kCurrentTipPrefName;
  static const wchar_t* kTipCachePrefName;

  // Default server from which to gather resources.
  static const wchar_t* kDefaultResourceServer;

 private:
  class WebResourceFetcher;
  friend class WebResourceFetcher;

  class UnpackerClient;

  void Init();

  // Set in_fetch_ to false, clean up temp directories (in the future).
  void EndFetch();

  // Puts parsed json data in the right places, and writes to prefs file.
  void OnWebResourceUnpacked(const DictionaryValue& parsed_json);

  // We need to be able to load parsed resource data into preferences file,
  // and get proper install directory.
  PrefService* prefs_;

  FilePath web_resource_dir_;

  // Server from which we are currently pulling web resource data.
  std::wstring web_resource_server_;

  WebResourceFetcher* web_resource_fetcher_;

  ResourceDispatcherHost* resource_dispatcher_host_;

  // Gets mutable dictionary attached to user's preferences, so that we
  // can write resource data back to user's pref file.
  DictionaryValue* web_resource_cache_;

  // True if we are currently mid-fetch.  If we are asked to start a fetch
  // when we are still fetching resource data, schedule another one in
  // kCacheUpdateDelay time, and silently exit.
  bool in_fetch_;

  // Maximum number of cached resources available.
  static const int kMaxResourceCacheSize = 6;

  // Delay on first fetch so we don't interfere with startup.
  static const int kStartResourceFetchDelay = 5000;

  // Delay between calls to update the cache (48 hours).
  static const int kCacheUpdateDelay = 48 * 60 * 60 * 1000;

  // Name of directory inside the profile where we will store resource-related
  // data (for now, thumbnail images).
  static const char* kResourceDirectoryName;

  DISALLOW_COPY_AND_ASSIGN(WebResourceService);
};

#endif  // CHROME_BROWSER_WEB_RESOURCE_WEB_RESOURCE_SERVICE_H_

