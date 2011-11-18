// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_service.h"

#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_factory.h"
#include "chrome/common/url_constants.h"

FaviconService::FaviconService(Profile* profile) : profile_(profile) {
}

FaviconService::Handle FaviconService::GetFavicon(
    const GURL& icon_url,
    history::IconType icon_type,
    CancelableRequestConsumerBase* consumer,
    const FaviconDataCallback& callback) {
  GetFaviconRequest* request = new GetFaviconRequest(callback);
  AddRequest(request, consumer);
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs)
    hs->GetFavicon(request, icon_url, icon_type);
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
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs)
    hs->UpdateFaviconMappingAndFetch(request, page_url, icon_url, icon_type);
  else
    ForwardEmptyResultAsync(request);
  return request->handle();
}

FaviconService::Handle FaviconService::GetFaviconForURL(
    const GURL& page_url,
    int icon_types,
    CancelableRequestConsumerBase* consumer,
    const FaviconDataCallback& callback) {
  GetFaviconRequest* request = new GetFaviconRequest(callback);
  AddRequest(request, consumer);
  FaviconService::Handle handle = request->handle();
  if (page_url.SchemeIs(chrome::kChromeUIScheme) ||
      page_url.SchemeIs(chrome::kExtensionScheme)) {
    ChromeWebUIFactory::GetInstance()->GetFaviconForURL(
        profile_, request, page_url);
  } else {
    HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (hs)
      hs->GetFaviconForURL(request, page_url, icon_types);
    else
      ForwardEmptyResultAsync(request);
  }
  return handle;
}

void FaviconService::SetFaviconOutOfDateForPage(const GURL& page_url) {
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs)
    hs->SetFaviconOutOfDateForPage(page_url);
}

void FaviconService::CloneFavicon(const GURL& old_page_url,
                                  const GURL& new_page_url) {
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs)
    hs->CloneFavicon(old_page_url, new_page_url);
}

void FaviconService::SetImportedFavicons(
    const std::vector<history::ImportedFaviconUsage>& favicon_usage) {
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs)
    hs->SetImportedFavicons(favicon_usage);
}

void FaviconService::SetFavicon(const GURL& page_url,
                                const GURL& icon_url,
                                const std::vector<unsigned char>& image_data,
                                history::IconType icon_type) {
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs)
    hs->SetFavicon(page_url, icon_url, image_data, icon_type);
}

FaviconService::~FaviconService() {
}

void FaviconService::ForwardEmptyResultAsync(GetFaviconRequest* request) {
  request->ForwardResultAsync(request->handle(), history::FaviconData());
}
