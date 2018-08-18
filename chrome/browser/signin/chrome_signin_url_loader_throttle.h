// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_URL_LOADER_THROTTLE_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_URL_LOADER_THROTTLE_H_

#include "base/macros.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/url_loader_throttle.h"

namespace content {
class NavigationUIData;
}  // namespace content

namespace signin {

class ChromeRequestAdapter;
class ResponseAdapter;

// This class is used to modify the main frame request made when loading the
// GAIA signin realm.
class URLLoaderThrottle : public content::URLLoaderThrottle {
 public:
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    virtual bool ShouldIntercept(
        content::NavigationUIData* navigation_ui_data) = 0;
    virtual void ProcessRequest(ChromeRequestAdapter* request_adapter,
                                const GURL& redirect_url) = 0;
    virtual void ProcessResponse(ResponseAdapter* response_adapter,
                                 const GURL& redirect_url) = 0;
  };

  // Creates a new throttle if |delegate| says that this request should be
  // intercepted.
  static std::unique_ptr<URLLoaderThrottle> MaybeCreate(
      std::unique_ptr<Delegate> delegate,
      content::NavigationUIData* navigation_ui_data,
      content::ResourceRequestInfo::WebContentsGetter web_contents_getter);

  ~URLLoaderThrottle() override;

  // content::URLLoaderThrottle
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                           const network::ResourceResponseHead& response_head,
                           bool* defer,
                           std::vector<std::string>* headers_to_remove,
                           net::HttpRequestHeaders* modified_headers) override;
  void WillProcessResponse(const GURL& response_url,
                           network::ResourceResponseHead* response_head,
                           bool* defer) override;

 private:
  class ThrottleRequestAdapter;
  class ThrottleResponseAdapter;

  URLLoaderThrottle(
      std::unique_ptr<Delegate> delegate,
      content::ResourceRequestInfo::WebContentsGetter web_contents_getter);

  const std::unique_ptr<Delegate> delegate_;
  const content::ResourceRequestInfo::WebContentsGetter web_contents_getter_;

  // Information about the current request.
  GURL request_url_;
  GURL request_referrer_;
  net::HttpRequestHeaders request_headers_;
  content::ResourceType request_resource_type_;
  bool is_main_frame_ = false;

  base::OnceClosure destruction_callback_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderThrottle);
};

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_URL_LOADER_THROTTLE_H_
