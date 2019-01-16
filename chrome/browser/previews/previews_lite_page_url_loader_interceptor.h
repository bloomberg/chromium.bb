// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_URL_LOADER_INTERCEPTOR_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_URL_LOADER_INTERCEPTOR_H_

#include "base/sequence_checker.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace previews {

// A class that attempts to intercept requests and fetch the Lite Page version
// of the request. Its lifetime matches that of the content/ navigation loader
// code. Currently, not fully implemented.
class PreviewsLitePageURLLoaderInterceptor
    : public content::URLLoaderRequestInterceptor {
 public:
  explicit PreviewsLitePageURLLoaderInterceptor(int frame_tree_node_id);
  ~PreviewsLitePageURLLoaderInterceptor() override;

  // content::URLLaoderRequestInterceptor:
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::ResourceContext* resource_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback) override;

 private:
  // Begins an attempt at fetching the lite page version of the URL.
  void CreateRedirectLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::ResourceContext* resource_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback);

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageURLLoaderInterceptor);
};

}  // namespace previews

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_URL_LOADER_INTERCEPTOR_H_
