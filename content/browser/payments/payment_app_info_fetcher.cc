// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_info_fetcher.h"

#include "base/base64.h"
#include "base/bind_helpers.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/manifest_icon_selector.h"
#include "ui/gfx/image/image.h"

namespace content {

PaymentAppInfoFetcher::PaymentAppInfo::PaymentAppInfo() {}
PaymentAppInfoFetcher::PaymentAppInfo::~PaymentAppInfo() {}

PaymentAppInfoFetcher::PaymentAppInfoFetcher()
    : context_process_id_(-1),
      context_frame_id_(-1),
      fetched_payment_app_info_(std::make_unique<PaymentAppInfo>()) {}
PaymentAppInfoFetcher::~PaymentAppInfoFetcher() {}

void PaymentAppInfoFetcher::Start(
    const GURL& context_url,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    PaymentAppInfoFetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  context_url_ = context_url;
  callback_ = std::move(callback);

  std::unique_ptr<std::vector<std::pair<int, int>>> provider_hosts =
      service_worker_context->GetProviderHostIds(context_url.GetOrigin());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&PaymentAppInfoFetcher::StartFromUIThread, this,
                     std::move(provider_hosts)));
}

void PaymentAppInfoFetcher::StartFromUIThread(
    const std::unique_ptr<std::vector<std::pair<int, int>>>& provider_hosts) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (provider_hosts->size() == 0U) {
    PostPaymentAppInfoFetchResultToIOThread();
    return;
  }

  for (const auto& frame : *provider_hosts) {
    RenderFrameHostImpl* render_frame_host =
        RenderFrameHostImpl::FromID(frame.first, frame.second);
    if (!render_frame_host)
      continue;

    WebContentsImpl* web_content = static_cast<WebContentsImpl*>(
        WebContents::FromRenderFrameHost(render_frame_host));
    if (!web_content || web_content->IsHidden() ||
        context_url_.spec().compare(
            web_content->GetLastCommittedURL().spec()) != 0) {
      continue;
    }

    context_process_id_ = frame.first;
    context_frame_id_ = frame.second;

    web_content->GetManifest(base::Bind(
        &PaymentAppInfoFetcher::FetchPaymentAppManifestCallback, this));
    return;
  }

  PostPaymentAppInfoFetchResultToIOThread();
}

void PaymentAppInfoFetcher::FetchPaymentAppManifestCallback(
    const GURL& url,
    const Manifest& manifest) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (url.is_empty() || manifest.IsEmpty()) {
    PostPaymentAppInfoFetchResultToIOThread();
    return;
  }

  fetched_payment_app_info_->prefer_related_applications =
      manifest.prefer_related_applications;
  for (const auto& related_application : manifest.related_applications) {
    fetched_payment_app_info_->related_applications.emplace_back(
        StoredRelatedApplication());
    if (!related_application.platform.is_null()) {
      base::UTF16ToUTF8(
          related_application.platform.string().c_str(),
          related_application.platform.string().length(),
          &(fetched_payment_app_info_->related_applications.back().platform));
    }
    if (!related_application.id.is_null()) {
      base::UTF16ToUTF8(
          related_application.id.string().c_str(),
          related_application.id.string().length(),
          &(fetched_payment_app_info_->related_applications.back().id));
    }
  }

  if (manifest.name.is_null() ||
      !base::UTF16ToUTF8(manifest.name.string().c_str(),
                         manifest.name.string().length(),
                         &(fetched_payment_app_info_->name))) {
    PostPaymentAppInfoFetchResultToIOThread();
    return;
  }

  // TODO(gogerald): Choose appropriate icon size dynamically on different
  // platforms.
  // Here we choose a large ideal icon size to be big enough for all platforms.
  // Note that we only scale down for this icon size but not scale up.
  const int kPaymentAppIdealIconSize = 0xFFFF;
  const int kPaymentAppMinimumIconSize = 0;

  GURL icon_url = ManifestIconSelector::FindBestMatchingIcon(
      manifest.icons, kPaymentAppIdealIconSize, kPaymentAppMinimumIconSize,
      Manifest::Icon::ANY);
  if (!icon_url.is_valid()) {
    PostPaymentAppInfoFetchResultToIOThread();
    return;
  }

  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(context_process_id_, context_frame_id_);
  if (!render_frame_host) {
    PostPaymentAppInfoFetchResultToIOThread();
    return;
  }

  WebContents* web_content =
      WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_content) {
    PostPaymentAppInfoFetchResultToIOThread();
    return;
  }

  if (!content::ManifestIconDownloader::Download(
          web_content, icon_url, kPaymentAppIdealIconSize,
          kPaymentAppMinimumIconSize,
          base::Bind(&PaymentAppInfoFetcher::OnIconFetched, this))) {
    PostPaymentAppInfoFetchResultToIOThread();
  }
}

void PaymentAppInfoFetcher::OnIconFetched(const SkBitmap& icon) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (icon.drawsNothing()) {
    PostPaymentAppInfoFetchResultToIOThread();
    return;
  }

  gfx::Image decoded_image = gfx::Image::CreateFrom1xBitmap(icon);
  scoped_refptr<base::RefCountedMemory> raw_data = decoded_image.As1xPNGBytes();
  base::Base64Encode(
      base::StringPiece(raw_data->front_as<char>(), raw_data->size()),
      &(fetched_payment_app_info_->icon));
  PostPaymentAppInfoFetchResultToIOThread();
}

void PaymentAppInfoFetcher::PostPaymentAppInfoFetchResultToIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::BindOnce(std::move(callback_),
                                         std::move(fetched_payment_app_info_)));
}

}  // namespace content
