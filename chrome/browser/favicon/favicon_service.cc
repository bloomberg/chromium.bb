// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_service.h"

#include "chrome/browser/favicon/select_favicon_frames.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/url_constants.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"

FaviconService::FaviconService(HistoryService* history_service)
    : history_service_(history_service) {
}

FaviconService::Handle FaviconService::GetFaviconImage(
    const GURL& icon_url,
    history::IconType icon_type,
    int desired_size_in_dip,
    CancelableRequestConsumerBase* consumer,
    const FaviconImageCallback& callback) {
  GetFaviconRequest* request = new GetFaviconRequest(base::Bind(
      &FaviconService::GetFaviconImageCallback,
      base::Unretained(this),
      desired_size_in_dip,
      callback));
  AddRequest(request, consumer);
  // TODO(pkotwicz): Pass in desired size and scale factors.
  if (history_service_)
    history_service_->GetFavicon(request, icon_url, icon_type);
  else
    ForwardEmptyResultAsync(request);
  return request->handle();
}

FaviconService::Handle FaviconService::GetRawFavicon(
    const GURL& icon_url,
    history::IconType icon_type,
    int desired_size_in_dip,
    ui::ScaleFactor desired_scale_factor,
    CancelableRequestConsumerBase* consumer,
    const FaviconRawCallback& callback) {
  GetFaviconRequest* request = new GetFaviconRequest(base::Bind(
      &FaviconService::GetRawFaviconCallback,
      base::Unretained(this),
      desired_size_in_dip,
      desired_scale_factor,
      callback));
  AddRequest(request, consumer);
  // TODO(pkotwicz): Pass in desired size and scale factor.
  if (history_service_)
    history_service_->GetFavicon(request, icon_url, icon_type);
  else
    ForwardEmptyResultAsync(request);
  return request->handle();
}

FaviconService::Handle FaviconService::GetFavicon(
    const GURL& icon_url,
    history::IconType icon_type,
    int desired_size_in_dip,
    const std::vector<ui::ScaleFactor>& desired_scale_factors,
    CancelableRequestConsumerBase* consumer,
    const FaviconResultsCallback& callback) {
  GetFaviconRequest* request = new GetFaviconRequest(callback);
  AddRequest(request, consumer);
  if (history_service_)
    history_service_->GetFavicon(request, icon_url, icon_type);
  else
    ForwardEmptyResultAsync(request);
  return request->handle();
}

FaviconService::Handle FaviconService::UpdateFaviconMappingAndFetch(
    const GURL& page_url,
    const GURL& icon_url,
    history::IconType icon_type,
    CancelableRequestConsumerBase* consumer,
    const FaviconResultsCallback& callback) {
  GetFaviconRequest* request = new GetFaviconRequest(callback);
  AddRequest(request, consumer);
  if (history_service_)
    history_service_->UpdateFaviconMappingAndFetch(request, page_url,
                                                   icon_url, icon_type);
  else
    ForwardEmptyResultAsync(request);
  return request->handle();
}

FaviconService::Handle FaviconService::GetFaviconImageForURL(
    Profile* profile,
    const GURL& page_url,
    int icon_types,
    int desired_size_in_dip,
    CancelableRequestConsumerBase* consumer,
    const FaviconImageCallback& callback) {
  GetFaviconRequest* request = new GetFaviconRequest(base::Bind(
      &FaviconService::GetFaviconImageCallback,
      base::Unretained(this),
      desired_size_in_dip,
      callback));

  std::vector<ui::ScaleFactor> desired_scale_factors =
      ui::GetSupportedScaleFactors();
  return GetFaviconForURLImpl(profile, page_url, icon_types,
      desired_size_in_dip, desired_scale_factors, consumer, request);
}

FaviconService::Handle FaviconService::GetRawFaviconForURL(
    Profile* profile,
    const GURL& page_url,
    int icon_types,
    int desired_size_in_dip,
    ui::ScaleFactor desired_scale_factor,
    CancelableRequestConsumerBase* consumer,
    const FaviconRawCallback& callback) {
  GetFaviconRequest* request = new GetFaviconRequest(base::Bind(
      &FaviconService::GetRawFaviconCallback,
      base::Unretained(this),
      desired_size_in_dip,
      desired_scale_factor,
      callback));

  std::vector<ui::ScaleFactor> desired_scale_factors;
  desired_scale_factors.push_back(desired_scale_factor);
  return GetFaviconForURLImpl(profile, page_url, icon_types,
      desired_size_in_dip, desired_scale_factors, consumer, request);
}

FaviconService::Handle FaviconService::GetFaviconForURL(
    Profile* profile,
    const GURL& page_url,
    int icon_types,
    int desired_size_in_dip,
    const std::vector<ui::ScaleFactor>& desired_scale_factors,
    CancelableRequestConsumerBase* consumer,
    const FaviconResultsCallback& callback) {
  GetFaviconRequest* request = new GetFaviconRequest(callback);
  return GetFaviconForURLImpl(profile, page_url, icon_types,
      desired_size_in_dip, desired_scale_factors, consumer, request);
}

FaviconService::Handle FaviconService::GetRawFaviconForID(
    history::FaviconID favicon_id,
    int desired_size_in_dip,
    ui::ScaleFactor desired_scale_factor,
    CancelableRequestConsumerBase* consumer,
    const FaviconRawCallback& callback) {
  GetFaviconRequest* request = new GetFaviconRequest(base::Bind(
      &FaviconService::GetRawFaviconCallback,
      base::Unretained(this),
      desired_size_in_dip,
      desired_scale_factor,
      callback));

  AddRequest(request, consumer);
  FaviconService::Handle handle = request->handle();
  // TODO(pkotwicz): Pass in desired size and scale factor.
  if (history_service_)
    history_service_->GetFaviconForID(request, favicon_id);
  else
    ForwardEmptyResultAsync(request);

  return handle;
}


void FaviconService::SetFaviconOutOfDateForPage(const GURL& page_url) {
  if (history_service_)
    history_service_->SetFaviconOutOfDateForPage(page_url);
}

void FaviconService::CloneFavicon(const GURL& old_page_url,
                                  const GURL& new_page_url) {
  if (history_service_)
    history_service_->CloneFavicon(old_page_url, new_page_url);
}

void FaviconService::SetImportedFavicons(
    const std::vector<history::ImportedFaviconUsage>& favicon_usage) {
  if (history_service_)
    history_service_->SetImportedFavicons(favicon_usage);
}

void FaviconService::SetFavicon(const GURL& page_url,
                                const GURL& icon_url,
                                const std::vector<unsigned char>& image_data,
                                history::IconType icon_type) {
  if (history_service_)
    history_service_->SetFavicon(page_url, icon_url, image_data, icon_type);
}

FaviconService::~FaviconService() {
}

FaviconService::Handle FaviconService::GetFaviconForURLImpl(
    Profile* profile,
    const GURL& page_url,
    int icon_types,
    int desired_size_in_dip,
    const std::vector<ui::ScaleFactor>& desired_scale_factors,
    CancelableRequestConsumerBase* consumer,
    GetFaviconRequest* request) {
  AddRequest(request, consumer);
  FaviconService::Handle handle = request->handle();
  if (page_url.SchemeIs(chrome::kChromeUIScheme) ||
      page_url.SchemeIs(chrome::kExtensionScheme)) {
    // TODO(pkotwicz): Pass in desired size and desired scale factors.
    ChromeWebUIControllerFactory::GetInstance()->GetFaviconForURL(
        profile, request, page_url);
  } else {
    // TODO(pkotwicz): Pass in desired size and desired scale factors.
    if (history_service_)
      history_service_->GetFaviconForURL(request, page_url, icon_types);
    else
      ForwardEmptyResultAsync(request);
  }
  return handle;
}

void FaviconService::GetFaviconImageCallback(
    int desired_size_in_dip,
    FaviconImageCallback callback,
    Handle handle,
    std::vector<history::FaviconBitmapResult> favicon_bitmap_results,
    history::IconURLSizesMap icon_url_sizes_map) {
  std::vector<SkBitmap> sk_bitmaps;
  for (size_t i = 0; i < favicon_bitmap_results.size(); ++i) {
    if (favicon_bitmap_results[i].is_valid()) {
      scoped_refptr<base::RefCountedMemory> bitmap_data =
          favicon_bitmap_results[i].bitmap_data;
      SkBitmap out_bitmap;
      if (gfx::PNGCodec::Decode(bitmap_data->front(), bitmap_data->size(),
                                &out_bitmap)) {
        sk_bitmaps.push_back(out_bitmap);
      }
    }
  }
  history::FaviconImageResult image_result;
  image_result.image = gfx::Image(SelectFaviconFrames(
      sk_bitmaps, ui::GetSupportedScaleFactors(), desired_size_in_dip, NULL));
  image_result.icon_url = favicon_bitmap_results.empty() ?
      GURL() : favicon_bitmap_results[0].icon_url;

  callback.Run(handle, image_result);
}

void FaviconService::GetRawFaviconCallback(
    int desired_size_in_dip,
    ui::ScaleFactor desired_scale_factor,
    FaviconRawCallback callback,
    Handle handle,
    std::vector<history::FaviconBitmapResult> favicon_bitmap_results,
    history::IconURLSizesMap icon_url_sizes_map) {
  if (favicon_bitmap_results.empty() || !favicon_bitmap_results[0].is_valid()) {
    callback.Run(handle, history::FaviconBitmapResult());
    return;
  }

  DCHECK_EQ(1u, favicon_bitmap_results.size());
  history::FaviconBitmapResult bitmap_result = favicon_bitmap_results[0];

  // If history bitmap is already desired pixel size, return early.
  float desired_scale = ui::GetScaleFactorScale(desired_scale_factor);
  int desired_edge_width_in_pixel = static_cast<int>(
      desired_size_in_dip * desired_scale + 0.5f);
  gfx::Size desired_size_in_pixel(desired_edge_width_in_pixel,
                                  desired_edge_width_in_pixel);
  if (bitmap_result.pixel_size == desired_size_in_pixel) {
    callback.Run(handle, bitmap_result);
    return;
  }

  // Convert raw bytes to SkBitmap, resize via SelectFaviconFrames(), then
  // convert back.
  SkBitmap bitmap;
  if (!gfx::PNGCodec::Decode(bitmap_result.bitmap_data->front(),
                             bitmap_result.bitmap_data->size(),
                             &bitmap)) {
    callback.Run(handle, history::FaviconBitmapResult());
    return;
  }

  std::vector<SkBitmap> bitmaps;
  bitmaps.push_back(bitmap);
  std::vector<ui::ScaleFactor> desired_scale_factors;
  desired_scale_factors.push_back(desired_scale_factor);
  gfx::ImageSkia resized_image = SelectFaviconFrames(bitmaps,
      desired_scale_factors, desired_size_in_dip, NULL);

  std::vector<unsigned char> resized_bitmap_data;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(*resized_image.bitmap(), false,
                                         &resized_bitmap_data)) {
    callback.Run(handle, history::FaviconBitmapResult());
    return;
  }

  bitmap_result.bitmap_data = base::RefCountedBytes::TakeVector(
      &resized_bitmap_data);
  callback.Run(handle, bitmap_result);
}

void FaviconService::ForwardEmptyResultAsync(GetFaviconRequest* request) {
  request->ForwardResultAsync(request->handle(),
                              std::vector<history::FaviconBitmapResult>(),
                              history::IconURLSizesMap());
}
