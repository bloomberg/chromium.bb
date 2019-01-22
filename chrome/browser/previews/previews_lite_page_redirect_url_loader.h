// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_URL_LOADER_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_URL_LOADER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/previews/previews_lite_page_serving_url_loader.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace previews {

using HandleRequest = base::OnceCallback<void(
    std::unique_ptr<PreviewsLitePageServingURLLoader> serving_url_loader,
    content::URLLoaderRequestInterceptor::RequestHandler handler)>;

// A URL loader that attempts to fetch an HTTPS server lite page, and if
// successful, redirects to the lite page URL, and hands the underlying
// network URLLoader to a success callback. Currently, it can only fallback to
// default behavior.
class PreviewsLitePageRedirectURLLoader : public network::mojom::URLLoader {
 public:
  static std::unique_ptr<PreviewsLitePageRedirectURLLoader>
  AttemptRedirectToPreview(
      const network::ResourceRequest& tentative_resource_request,
      HandleRequest callback);

  ~PreviewsLitePageRedirectURLLoader() override;

 private:
  PreviewsLitePageRedirectURLLoader(
      const network::ResourceRequest& tentative_resource_request,
      HandleRequest callback);

  // network::mojom::URLLoader:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // Called when |serving_url_loader_| wants to fallback.
  void OnFallback();

  // Creates and starts |serving_url_loader_|.
  void StartRedirectToPreview();

  // The underlying URLLoader that speculatively tries to fetch the lite page.
  std::unique_ptr<PreviewsLitePageServingURLLoader> serving_url_loader_;

  // A copy of the initial resource request that has been modified to fetch the
  // lite page.
  network::ResourceRequest modified_resource_request_;

  // Called upon success or failure to let content/ know whether this class
  // intends to intercept the request. Must be passed a handler if this class
  // intends to intercept the request.
  HandleRequest callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PreviewsLitePageRedirectURLLoader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageRedirectURLLoader);
};

}  // namespace previews

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_URL_LOADER_H_
