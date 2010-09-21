// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_RESOURCE_WEB_RESOURCE_SERVICE_H_
#define CHROME_BROWSER_WEB_RESOURCE_WEB_RESOURCE_SERVICE_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/utility_process_host.h"
#include "chrome/common/web_resource/web_resource_unpacker.h"

class Profile;

class WebResourceService
    : public UtilityProcessHost::Client {
 public:
  explicit WebResourceService(Profile* profile);

  // Sleep until cache needs to be updated, but always for at least 5 seconds
  // so we don't interfere with startup.  Then begin updating resources.
  void StartAfterDelay();

  // We have successfully pulled data from a resource server; now launch
  // the process that will parse the JSON, and then update the cache.
  void UpdateResourceCache(const std::string& json_data);

  static const char* kCurrentTipPrefName;
  static const char* kTipCachePrefName;
  static const char* kCustomLogoId;

  // Default server from which to gather resources.
  static const char* kDefaultResourceServer;

 private:
  class WebResourceFetcher;
  friend class WebResourceFetcher;

  class UnpackerClient;

  ~WebResourceService();

  void Init();

  // Set in_fetch_ to false, clean up temp directories (in the future).
  void EndFetch();

  // Puts parsed json data in the right places, and writes to prefs file.
  void OnWebResourceUnpacked(const DictionaryValue& parsed_json);

  // Unpack the web resource as a set of tips. Expects json in the form of:
  // {
  //   "lang": "en",
  //   "topic": {
  //     "topid_id": "24013",
  //     "topics": [
  //     ],
  //     "answers": [
  //       {
  //         "answer_id": "18625",
  //         "inproduct": "Text here will be shown as a tip",
  //       },
  //       ...
  //     ]
  //   }
  // }
  //
  void UnpackTips(const DictionaryValue& parsed_json);

  // Unpack the web resource as a custom logo signal. Expects json in the form
  // of:
  // {
  //   "topic": {
  //     "answers": [
  //       {
  //         "logo_id": "1"
  //       },
  //       ...
  //     ]
  //   }
  // }
  //
  void UnpackLogoSignal(const DictionaryValue& parsed_json);

  // We need to be able to load parsed resource data into preferences file,
  // and get proper install directory.
  PrefService* prefs_;

  // Server from which we are currently pulling web resource data.
  std::string web_resource_server_;

  WebResourceFetcher* web_resource_fetcher_;

  ResourceDispatcherHost* resource_dispatcher_host_;

  // Gets mutable dictionary attached to user's preferences, so that we
  // can write resource data back to user's pref file.
  DictionaryValue* web_resource_cache_;

  // True if we are currently mid-fetch.  If we are asked to start a fetch
  // when we are still fetching resource data, schedule another one in
  // kCacheUpdateDelay time, and silently exit.
  bool in_fetch_;

  // Delay on first fetch so we don't interfere with startup.
  static const int kStartResourceFetchDelay = 5000;

  // Delay between calls to update the cache (12 hours).
  static const int kCacheUpdateDelay = 12 * 60 * 60 * 1000;

  DISALLOW_COPY_AND_ASSIGN(WebResourceService);
};

#endif  // CHROME_BROWSER_WEB_RESOURCE_WEB_RESOURCE_SERVICE_H_
