// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_redirect_url_loader.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"
#include "content/public/common/previews_state.h"
#include "services/network/public/cpp/resource_request.h"

namespace previews {

PreviewsLitePageRedirectURLLoader::PreviewsLitePageRedirectURLLoader(
    const network::ResourceRequest& tentative_resource_request,
    HandleRequest callback)
    : modified_resource_request_(tentative_resource_request),
      callback_(std::move(callback)),
      weak_ptr_factory_(this) {}

PreviewsLitePageRedirectURLLoader::~PreviewsLitePageRedirectURLLoader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<PreviewsLitePageRedirectURLLoader>
PreviewsLitePageRedirectURLLoader::AttemptRedirectToPreview(
    const network::ResourceRequest& tentative_resource_request,
    HandleRequest callback) {
  auto redirect_loader = base::WrapUnique(new PreviewsLitePageRedirectURLLoader(
      tentative_resource_request, std::move(callback)));

  redirect_loader->StartRedirectToPreview();

  return redirect_loader;
}

void PreviewsLitePageRedirectURLLoader::StartRedirectToPreview() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  modified_resource_request_.url =
      PreviewsLitePageNavigationThrottle::GetPreviewsURLForURL(
          modified_resource_request_.url);

  serving_url_loader_ = std::make_unique<PreviewsLitePageServingURLLoader>(
      modified_resource_request_,
      base::BindOnce(&PreviewsLitePageRedirectURLLoader::OnFallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PreviewsLitePageRedirectURLLoader::OnFallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback_).Run(nullptr, {});
}

void PreviewsLitePageRedirectURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const base::Optional<GURL>& new_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Content should not hang onto old URLLoaders for redirects.
  NOTREACHED();
}

void PreviewsLitePageRedirectURLLoader::ProceedWithResponse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This class never provides a response past the headers.
  NOTREACHED();
}

void PreviewsLitePageRedirectURLLoader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pass through.
  serving_url_loader_->SetPriority(priority, intra_priority_value);
}

void PreviewsLitePageRedirectURLLoader::PauseReadingBodyFromNet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pass through.
  serving_url_loader_->PauseReadingBodyFromNet();
}

void PreviewsLitePageRedirectURLLoader::ResumeReadingBodyFromNet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pass through.
  serving_url_loader_->ResumeReadingBodyFromNet();
}

}  // namespace previews
