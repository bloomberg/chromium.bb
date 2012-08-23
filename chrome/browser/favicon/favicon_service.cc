// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_service.h"

#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/url_constants.h"

FaviconService::FaviconService(HistoryService* history_service)
    : history_service_(history_service) {
}

FaviconService::Handle FaviconService::GetFavicon(
    const GURL& icon_url,
    history::IconType icon_type,
    CancelableRequestConsumerBase* consumer,
    const FaviconDataCallback& callback) {
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
    const FaviconDataCallback& callback) {
  GetFaviconRequest* request = new GetFaviconRequest(callback);
  AddRequest(request, consumer);
  if (history_service_)
    history_service_->UpdateFaviconMappingAndFetch(request, page_url,
                                                   icon_url, icon_type);
  else
    ForwardEmptyResultAsync(request);
  return request->handle();
}

FaviconService::Handle FaviconService::GetFaviconForURL(
    Profile* profile,
    const GURL& page_url,
    int icon_types,
    CancelableRequestConsumerBase* consumer,
    const FaviconDataCallback& callback) {
  GetFaviconRequest* request = new GetFaviconRequest(callback);
  AddRequest(request, consumer);
  FaviconService::Handle handle = request->handle();
  if (page_url.SchemeIs(chrome::kChromeUIScheme) ||
      page_url.SchemeIs(chrome::kExtensionScheme)) {
    ChromeWebUIControllerFactory::GetInstance()->GetFaviconForURL(
        profile, request, page_url);
  } else {
    if (history_service_)
      history_service_->GetFaviconForURL(request, page_url, icon_types);
    else
      ForwardEmptyResultAsync(request);
  }
  return handle;
}

// Requests the favicon for |favicon_id|. The |consumer| is notified when the
// bits have been fetched.
FaviconService::Handle FaviconService::GetFaviconForID(
    history::FaviconID favicon_id,
    CancelableRequestConsumerBase* consumer,
    const FaviconDataCallback& callback) {
  GetFaviconRequest* request = new GetFaviconRequest(callback);
  AddRequest(request, consumer);
  FaviconService::Handle handle = request->handle();
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

void FaviconService::ForwardEmptyResultAsync(GetFaviconRequest* request) {
  request->ForwardResultAsync(request->handle(), history::FaviconData());
}
