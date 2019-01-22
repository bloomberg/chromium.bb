// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_serving_url_loader.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/previews_state.h"
#include "services/network/public/cpp/resource_request.h"

namespace previews {

PreviewsLitePageServingURLLoader::PreviewsLitePageServingURLLoader(
    const network::ResourceRequest& request,
    FallbackCallback fallback_callback)
    : fallback_callback_(std::move(fallback_callback)),
      weak_ptr_factory_(this) {
  // TODO(ryansturm): Set up a network service URLLoader. For now, simulate a
  // a fallback asynchronously.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PreviewsLitePageServingURLLoader::Fallback,
                                weak_ptr_factory_.GetWeakPtr()));
}

PreviewsLitePageServingURLLoader::~PreviewsLitePageServingURLLoader() = default;

void PreviewsLitePageServingURLLoader::Fallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(fallback_callback_).Run();
}

void PreviewsLitePageServingURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const base::Optional<GURL>& new_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This should never be called for a non-network service URLLoader.
  NOTREACHED();
}

void PreviewsLitePageServingURLLoader::ProceedWithResponse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(ryansturm): Pass through calls to a network service URLLoader.
}

void PreviewsLitePageServingURLLoader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(ryansturm): Pass through calls to a network service URLLoader.
}

void PreviewsLitePageServingURLLoader::PauseReadingBodyFromNet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(ryansturm): Pass through calls to a network service URLLoader.
}

void PreviewsLitePageServingURLLoader::ResumeReadingBodyFromNet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(ryansturm): Pass through calls to a network service URLLoader.
}

}  // namespace previews
