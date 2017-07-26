// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DEVICE_DESCRIPTION_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DEVICE_DESCRIPTION_SERVICE_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/media/router/discovery/dial/parsed_dial_device_description.h"
#include "chrome/common/media_router/mojo/dial_device_description_parser.mojom.h"

namespace net {
class URLRequestContextGetter;
}

namespace media_router {

class DeviceDescriptionFetcher;
class SafeDialDeviceDescriptionParser;

// This class fetches and parses device description XML for DIAL devices. Actual
// parsing happens in a separate utility process via SafeDeviceDescriptionParser
// (instead of in this class). This class lives on IO thread.
class DeviceDescriptionService {
 public:
  // Represents cached device description data parsed from device description
  // XML.
  struct CacheEntry {
    CacheEntry();
    CacheEntry(const CacheEntry& other);
    ~CacheEntry();

    // The expiration time from the cache.
    base::Time expire_time;

    // The device description version number (non-negative).
    int32_t config_id;

    // Parsed device description data from XML.
    ParsedDialDeviceDescription description_data;
  };

  // Called if parsing device description XML in utility process succeeds, and
  // all fields are valid.
  // |device_data|: The device to look up.
  // |description_data|: Device description data from device description XML.
  using DeviceDescriptionParseSuccessCallback =
      base::Callback<void(const DialDeviceData& device_data,
                          const ParsedDialDeviceDescription& description_data)>;

  // Called if parsing device description XML in utility process fails, or some
  // parsed fields are missing or invalid.
  using DeviceDescriptionParseErrorCallback =
      base::Callback<void(const DialDeviceData& device_data,
                          const std::string& error_message)>;

  DeviceDescriptionService(
      const DeviceDescriptionParseSuccessCallback& success_cb,
      const DeviceDescriptionParseErrorCallback& error_cb);
  virtual ~DeviceDescriptionService();

  // For each device in |devices|, if there is a valid cache entry for it, call
  // |success_cb_| with cached device description; otherwise start fetching
  // device description XML and parsing XML in utility process. Call
  // |success_cb_| if both fetching and parsing succeeds; otherwise call
  // |error_cb_|.
  // |request_context|: Used by the background URLFetchers.
  virtual void GetDeviceDescriptions(
      const std::vector<DialDeviceData>& devices,
      net::URLRequestContextGetter* request_context);

 private:
  friend class DeviceDescriptionServiceTest;
  friend class TestDeviceDescriptionService;
  FRIEND_TEST_ALL_PREFIXES(DeviceDescriptionServiceTest,
                           TestGetDeviceDescriptionRemoveOutDatedFetchers);
  FRIEND_TEST_ALL_PREFIXES(DeviceDescriptionServiceTest,
                           TestGetDeviceDescriptionFetchURLError);
  FRIEND_TEST_ALL_PREFIXES(DeviceDescriptionServiceTest,
                           TestCleanUpCacheEntries);
  FRIEND_TEST_ALL_PREFIXES(DeviceDescriptionServiceTest,
                           TestSafeParserProperlyCreated);

  // Checks the cache for a valid device description. If the entry is found but
  // is expired, it is removed from the cache. Returns cached entry of
  // parsed device description. Returns nullptr if cache entry does not exist or
  // is not valid.
  // |device_data|: The device to look up.
  const CacheEntry* CheckAndUpdateCache(const DialDeviceData& device_data);

  // Issues a HTTP GET request for the device description. No-op if there is
  // already a pending request.
  // |device_data|: The device to look up.
  // |request_context|: Used by the background URLFetchers.
  void FetchDeviceDescription(const DialDeviceData& device_data,
                              net::URLRequestContextGetter* request_context);

  // Invoked when HTTP GET request finishes.
  // |device_data|: Device data initiating the HTTP request.
  // |description_data|: Response from HTTP request.
  void OnDeviceDescriptionFetchComplete(
      const DialDeviceData& device_data,
      const DialDeviceDescriptionData& description_data);

  // Invoked when HTTP GET request fails.
  // |device_data|: Device data initiating the HTTP request.
  // |error_message|: Error message from HTTP request.
  void OnDeviceDescriptionFetchError(const DialDeviceData& device_data,
                                     const std::string& error_message);

  // Invoked when SafeDialDeviceDescriptionParser finishes parsing device
  // description XML.
  // |device_data|: Device data initiating XML parsing in utility process.
  // |app_url|: The app Url.
  // |device_description_ptr|: Parsed device description from utility process,
  // or nullptr if parsing failed.
  // |parsing_error|: error encountered while parsing DIAL device description.
  void OnParsedDeviceDescription(
      const DialDeviceData& device_data,
      const GURL& app_url,
      chrome::mojom::DialDeviceDescriptionPtr device_description_ptr,
      chrome::mojom::DialParsingError parsing_error);

  // Remove expired cache entries from |description_map_|.
  void CleanUpCacheEntries();

  // Used by unit tests.
  virtual base::Time GetNow();

  // Map of current device description fetches in progress, keyed by device
  // label.
  std::map<std::string, std::unique_ptr<DeviceDescriptionFetcher>>
      device_description_fetcher_map_;

  // Set of device labels to be parsed in current utility process.
  std::set<std::string> pending_device_labels_;

  // Map of current cached device descriptions, keyed by device label.
  std::map<std::string, CacheEntry> description_cache_;

  // See comments for DeviceDescriptionParseSuccessCallback.
  DeviceDescriptionParseSuccessCallback success_cb_;

  // See comments for DeviceDescriptionParseErrorCallback.
  DeviceDescriptionParseErrorCallback error_cb_;

  // Timer for clean up expired cache entries.
  std::unique_ptr<base::RepeatingTimer> clean_up_timer_;

  // Safe DIAL parser associated with utility process. When this object is
  // destroyed, associating utility process will shutdown. Keep |parser_| alive
  // until finish parsing all device descriptions of devices passed in
  // |GetDeviceDescriptions()|. If second |GetDeviceDescriptions()| arrives and
  // |parser_| is still alive, reuse |parser_| instead of creating a new object.
  std::unique_ptr<SafeDialDeviceDescriptionParser> parser_;

  base::ThreadChecker thread_checker_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DEVICE_DESCRIPTION_SERVICE_H_
