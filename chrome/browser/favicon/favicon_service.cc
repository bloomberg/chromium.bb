// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_service.h"

#include "chrome/browser/favicon/favicon_util.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/select_favicon_frames.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/url_constants.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
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
  if (history_service_) {
    std::vector<GURL> icon_urls;
    icon_urls.push_back(icon_url);
    history_service_->GetFavicons(request, icon_urls, icon_type,
        desired_size_in_dip, ui::GetSupportedScaleFactors());
  } else {
    ForwardEmptyResultAsync(request);
  }
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
  if (history_service_) {
    std::vector<GURL> icon_urls;
    icon_urls.push_back(icon_url);
    std::vector<ui::ScaleFactor> desired_scale_factors;
    desired_scale_factors.push_back(desired_scale_factor);
    history_service_->GetFavicons(request, icon_urls, icon_type,
        desired_size_in_dip, desired_scale_factors);
  } else {
    ForwardEmptyResultAsync(request);
  }
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
  if (history_service_) {
    std::vector<GURL> icon_urls;
    icon_urls.push_back(icon_url);
    history_service_->GetFavicons(request, icon_urls, icon_type,
        desired_size_in_dip, desired_scale_factors);
  } else {
    ForwardEmptyResultAsync(request);
  }
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
  if (history_service_) {
    std::vector<GURL> icon_urls;
    icon_urls.push_back(icon_url);
    // TODO(pkotwicz): Pass in |desired_size_in_dip| and |desired_scale_factors|
    // from FaviconHandler.
    history_service_->UpdateFaviconMappingsAndFetch(request, page_url,
        icon_urls, icon_type, gfx::kFaviconSize,
        ui::GetSupportedScaleFactors());
  } else {
    ForwardEmptyResultAsync(request);
  }
  return request->handle();
}

FaviconService::Handle FaviconService::GetFaviconImageForURL(
    const FaviconForURLParams& params,
    const FaviconImageCallback& callback) {
  GetFaviconRequest* request = new GetFaviconRequest(base::Bind(
      &FaviconService::GetFaviconImageCallback,
      base::Unretained(this),
      params.desired_size_in_dip,
      callback));

  std::vector<ui::ScaleFactor> desired_scale_factors =
      ui::GetSupportedScaleFactors();
  return GetFaviconForURLImpl(params, desired_scale_factors, request);
}

FaviconService::Handle FaviconService::GetRawFaviconForURL(
    const FaviconForURLParams& params,
    ui::ScaleFactor desired_scale_factor,
    const FaviconRawCallback& callback) {
  GetFaviconRequest* request = new GetFaviconRequest(base::Bind(
      &FaviconService::GetRawFaviconCallback,
      base::Unretained(this),
      params.desired_size_in_dip,
      desired_scale_factor,
      callback));

  std::vector<ui::ScaleFactor> desired_scale_factors;
  desired_scale_factors.push_back(desired_scale_factor);
  return GetFaviconForURLImpl(params, desired_scale_factors, request);
}

FaviconService::Handle FaviconService::GetFaviconForURL(
    const FaviconForURLParams& params,
    const std::vector<ui::ScaleFactor>& desired_scale_factors,
    const FaviconResultsCallback& callback) {
  GetFaviconRequest* request = new GetFaviconRequest(callback);
  return GetFaviconForURLImpl(params, desired_scale_factors, request);
}

FaviconService::Handle FaviconService::GetLargestRawFaviconForID(
      history::FaviconID favicon_id,
      CancelableRequestConsumerBase* consumer,
      const FaviconRawCallback& callback) {
   // Use 0 as |desired_size_in_dip| to get the largest bitmap for |favicon_id|
   // without any resizing.
   int desired_size_in_dip = 0;
   ui::ScaleFactor desired_scale_factor = ui::SCALE_FACTOR_100P;
   GetFaviconRequest* request = new GetFaviconRequest(base::Bind(
      &FaviconService::GetRawFaviconCallback,
      base::Unretained(this),
      desired_size_in_dip,
      desired_scale_factor,
      callback));

  AddRequest(request, consumer);
  FaviconService::Handle handle = request->handle();
  if (history_service_) {
    history_service_->GetFaviconForID(request, favicon_id, desired_size_in_dip,
                                      desired_scale_factor);
  } else {
    ForwardEmptyResultAsync(request);
  }
  return handle;
}

void FaviconService::SetFaviconOutOfDateForPage(const GURL& page_url) {
  if (history_service_)
    history_service_->SetFaviconsOutOfDateForPage(page_url);
}

void FaviconService::CloneFavicon(const GURL& old_page_url,
                                  const GURL& new_page_url) {
  if (history_service_)
    history_service_->CloneFavicons(old_page_url, new_page_url);
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
  if (history_service_) {
    // TODO(pkotwicz): Pass in the real pixel size of |image_data|.
    history::FaviconBitmapData bitmap_data_element;
    bitmap_data_element.bitmap_data = new base::RefCountedBytes(image_data);
    bitmap_data_element.pixel_size = gfx::Size();
    bitmap_data_element.icon_url = icon_url;
    std::vector<history::FaviconBitmapData> favicon_bitmap_data;
    favicon_bitmap_data.push_back(bitmap_data_element);
    history::FaviconSizes favicon_sizes;
    favicon_sizes.push_back(gfx::Size());
    history::IconURLSizesMap icon_url_sizes;
    icon_url_sizes[icon_url] = favicon_sizes;
    history_service_->SetFavicons(page_url, icon_type,
        favicon_bitmap_data, icon_url_sizes);
  }
}

FaviconService::~FaviconService() {
}

FaviconService::Handle FaviconService::GetFaviconForURLImpl(
    const FaviconForURLParams& params,
    const std::vector<ui::ScaleFactor>& desired_scale_factors,
    GetFaviconRequest* request) {
  AddRequest(request, params.consumer);
  FaviconService::Handle handle = request->handle();
  if (params.page_url.SchemeIs(chrome::kChromeUIScheme) ||
      params.page_url.SchemeIs(chrome::kExtensionScheme)) {
    ChromeWebUIControllerFactory::GetInstance()->GetFaviconForURL(
        params.profile, request, params.page_url, desired_scale_factors);
  } else {
    if (history_service_) {
      history_service_->GetFaviconsForURL(request,
                                          params.page_url,
                                          params.icon_types,
                                          params.desired_size_in_dip,
                                          desired_scale_factors);
    } else {
      ForwardEmptyResultAsync(request);
    }
  }
  return handle;
}

void FaviconService::GetFaviconImageCallback(
    int desired_size_in_dip,
    FaviconImageCallback callback,
    Handle handle,
    std::vector<history::FaviconBitmapResult> favicon_bitmap_results,
    history::IconURLSizesMap icon_url_sizes_map) {
  history::FaviconImageResult image_result;
  image_result.image = FaviconUtil::SelectFaviconFramesFromPNGs(
      favicon_bitmap_results,
      ui::GetSupportedScaleFactors(),
      desired_size_in_dip);
  image_result.icon_url = image_result.image.IsEmpty() ?
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

  // If the desired size is 0, SelectFaviconFrames() will return the largest
  // bitmap without doing any resizing. As |favicon_bitmap_results| has bitmap
  // data for a single bitmap, return it and avoid an unnecessary decode.
  if (desired_size_in_dip == 0) {
    callback.Run(handle, bitmap_result);
    return;
  }

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
