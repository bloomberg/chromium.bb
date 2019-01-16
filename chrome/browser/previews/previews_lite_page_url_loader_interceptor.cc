// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_url_loader_interceptor.h"

#include "base/metrics/histogram_macros.h"
#include "components/previews/core/previews_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"

namespace previews {

namespace {

void RecordInterceptAttempt(bool attempted) {
  UMA_HISTOGRAM_BOOLEAN("Previews.ServerLitePage.URLLoader.Attempted",
                        attempted);
}

bool ShouldCreateLoader(const network::ResourceRequest& resource_request) {
  if (!(resource_request.previews_state & content::LITE_PAGE_REDIRECT_ON))
    return false;

  DCHECK_EQ(resource_request.resource_type, content::RESOURCE_TYPE_MAIN_FRAME);
  DCHECK(resource_request.url.SchemeIsHTTPOrHTTPS());
  DCHECK_EQ(resource_request.method, "GET");

  return true;
}

}  // namespace

PreviewsLitePageURLLoaderInterceptor::PreviewsLitePageURLLoaderInterceptor(
    int frame_tree_node_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(ryansturm): use frame_tree_node_id and pass in other members.
  // https://crbug.com/921740
}

PreviewsLitePageURLLoaderInterceptor::~PreviewsLitePageURLLoaderInterceptor() {}

void PreviewsLitePageURLLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::ResourceContext* resource_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ShouldCreateLoader(tentative_resource_request)) {
    CreateRedirectLoader(tentative_resource_request, resource_context,
                         std::move(callback));
    return;
  }
  RecordInterceptAttempt(false);
  std::move(callback).Run({});
}

void PreviewsLitePageURLLoaderInterceptor::CreateRedirectLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::ResourceContext* resource_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(ryansturm): handle fetching the lite page.
  // https://crbug.com/921740
  RecordInterceptAttempt(true);
  std::move(callback).Run({});
}

}  // namespace previews
