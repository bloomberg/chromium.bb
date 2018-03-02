// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DEVICE_DESCRIPTION_FETCHER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DEVICE_DESCRIPTION_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/sequence_checker.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace net {
struct RedirectInfo;
}

namespace network {
class SimpleURLLoader;
struct ResourceResponseHead;
}

namespace media_router {

struct DialDeviceDescriptionData;

// Used to make a single HTTP GET request with |device_description_url| to fetch
// a uPnP device description from a DIAL device.  If successful, |success_cb| is
// invoked with the result; otherwise, |error_cb| is invoked with an error
// reason.
// This class is not sequence safe.
class DeviceDescriptionFetcher {
 public:
  DeviceDescriptionFetcher(
      const GURL& device_description_url,
      base::OnceCallback<void(const DialDeviceDescriptionData&)> success_cb,
      base::OnceCallback<void(const std::string&)> error_cb);

  virtual ~DeviceDescriptionFetcher();

  const GURL& device_description_url() { return device_description_url_; }

  void Start();

 private:
  friend class TestDeviceDescriptionFetcher;

  // Starts the download on |loader_|.
  virtual void StartDownload();

  // Processes the response from the GET request and invoke the success or
  // error callback.
  void ProcessResponse(std::unique_ptr<std::string> response);

  // Invokes the error callback due to a redirect that had occurred. Also
  // aborts the request.
  void ReportRedirectError(const net::RedirectInfo& redirect_info,
                           const network::ResourceResponseHead& response_head);

  // Runs |error_cb_| with |message| and clears it.
  void ReportError(const std::string& message);

  const GURL device_description_url_;

  base::OnceCallback<void(const DialDeviceDescriptionData&)> success_cb_;
  base::OnceCallback<void(const std::string&)> error_cb_;
  std::unique_ptr<network::SimpleURLLoader> loader_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(DeviceDescriptionFetcher);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DEVICE_DESCRIPTION_FETCHER_H_
