// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_auto_fetcher.h"

#include <utility>

#include "chrome/browser/offline_pages/offline_page_auto_fetcher_service.h"
#include "chrome/browser/offline_pages/offline_page_auto_fetcher_service_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "url/gurl.h"

namespace offline_pages {

OfflinePageAutoFetcher::OfflinePageAutoFetcher(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {}

OfflinePageAutoFetcher::~OfflinePageAutoFetcher() = default;

void OfflinePageAutoFetcher::TrySchedule(bool user_requested,
                                         TryScheduleCallback callback) {
  GetService()->TrySchedule(user_requested,
                            render_frame_host_->GetLastCommittedURL(),
                            std::move(callback));
}

void OfflinePageAutoFetcher::CancelSchedule() {
  GetService()->CancelSchedule(render_frame_host_->GetLastCommittedURL());
}

// static
void OfflinePageAutoFetcher::Create(
    chrome::mojom::OfflinePageAutoFetcherRequest request,
    content::RenderFrameHost* render_frame_host) {
  mojo::MakeStrongBinding(
      std::make_unique<OfflinePageAutoFetcher>(render_frame_host),
      std::move(request));
}

OfflinePageAutoFetcherService* OfflinePageAutoFetcher::GetService() {
  return OfflinePageAutoFetcherServiceFactory::GetForBrowserContext(
      render_frame_host_->GetProcess()->GetBrowserContext());
}

}  // namespace offline_pages
