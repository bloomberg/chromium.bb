// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_SERVING_URL_LOADER_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_SERVING_URL_LOADER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace previews {

using FallbackCallback = base::OnceCallback<void()>;

// This class attempts to fetch a LitePage from the LitePage server, and if
// successful, calls a success callback. Otherwise, it calls fallback in the
// case of a failure and redirect in the case of a redirect served from the lite
// pages service. For now, it is only partially implemented.
class PreviewsLitePageServingURLLoader : public network::mojom::URLLoader {
 public:
  // Creates a network service URLLoader, binds to the URL Loader, and stores
  // the various callbacks.
  PreviewsLitePageServingURLLoader(const network::ResourceRequest& request,
                                   FallbackCallback fallback_callback);
  ~PreviewsLitePageServingURLLoader() override;

  // network::mojom::URLLoader:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

 private:
  // Calls |fallback_callback_| and cleans up.
  void Fallback();

  // When an error occurs or the LitePage is not suitable, this callback resumes
  // default behavior.
  FallbackCallback fallback_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PreviewsLitePageServingURLLoader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageServingURLLoader);
};

}  // namespace previews

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_SERVING_URL_LOADER_H_
