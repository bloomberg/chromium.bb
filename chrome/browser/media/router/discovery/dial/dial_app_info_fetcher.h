// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_APP_INFO_FETCHER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_APP_INFO_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace net {
struct RedirectInfo;
}

namespace network {
class SimpleURLLoader;
struct ResourceResponseHead;
}  // namespace network

namespace media_router {

// Used to make a single HTTP GET request with |app_url| to fetch an app info
// from a DIAL device.  If successful, |success_cb| is invoked with the result;
// otherwise, |error_cb| is invoked with an error reason.
// This class is not sequence safe.
class DialAppInfoFetcher {
 public:
  DialAppInfoFetcher(
      const GURL& app_url,
      base::OnceCallback<void(const std::string&)> success_cb,
      base::OnceCallback<void(int, const std::string&)> error_cb);

  virtual ~DialAppInfoFetcher();

  const GURL& app_url() { return app_url_; }

  // Starts the fetch. |ProcessResponse| will be invoked on completion.
  // |ReportRedirectError| will be invoked when a redirect occurrs.
  void Start();

 private:
  friend class TestDialAppInfoFetcher;

  // Starts the download on |loader_|.
  virtual void StartDownload();

  // Processes the response and invokes the success or error callback.
  void ProcessResponse(std::unique_ptr<std::string> response);

  // Invokes the error callback due to redirect, and aborts the request.
  void ReportRedirectError(const net::RedirectInfo& redirect_info,
                           const network::ResourceResponseHead& response_head);

  // Runs |error_cb_| with |message| and clears it.
  void ReportError(int response_code, const std::string& message);

  const GURL app_url_;
  base::OnceCallback<void(const std::string&)> success_cb_;
  base::OnceCallback<void(int, const std::string&)> error_cb_;
  std::unique_ptr<network::SimpleURLLoader> loader_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(DialAppInfoFetcher);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_APP_INFO_FETCHER_H_
