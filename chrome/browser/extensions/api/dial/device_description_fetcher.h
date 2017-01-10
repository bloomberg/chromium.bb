// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DIAL_DEVICE_DESCRIPTION_FETCHER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DIAL_DEVICE_DESCRIPTION_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

class Profile;

namespace net {
class URLFetcher;
}

namespace extensions {
namespace api {
namespace dial {

struct DialDeviceDescriptionData;

// Used to make a single HTTP GET request with |device_description_url| to fetch
// a uPnP device description from a DIAL device.  If successful, |success_cb| is
// invoked with the result; otherwise, |error_cb| is invoked with an error
// reason.  This class is not thread safe and must be used on the UI thread.
class DeviceDescriptionFetcher : public net::URLFetcherDelegate {
 public:
  // Used to identify the net::URLFetcher instance for tests.
  static constexpr int kURLFetcherIDForTest = 1;

  // |profile| is unowned; the caller must ensure that this object does not
  // outlive it.
  DeviceDescriptionFetcher(
      const GURL& device_description_url,
      Profile* profile,
      base::OnceCallback<void(const DialDeviceDescriptionData&)> success_cb,
      base::OnceCallback<void(const std::string&)> error_cb);

  ~DeviceDescriptionFetcher() override;

  void Start();

 private:
  // net::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;
  void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                  int64_t current,
                                  int64_t total,
                                  int64_t current_network_bytes) override;
  void OnURLFetchUploadProgress(const net::URLFetcher* source,
                                int64_t current,
                                int64_t total) override;

  // Runs |error_cb_| with |message| and clears it.
  void ReportError(const std::string& message);

  const GURL device_description_url_;
  Profile* const profile_;
  base::OnceCallback<void(const DialDeviceDescriptionData&)> success_cb_;
  base::OnceCallback<void(const std::string&)> error_cb_;
  std::unique_ptr<net::URLFetcher> fetcher_;
};

}  // namespace dial
}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DIAL_DEVICE_DESCRIPTION_FETCHER_H_
