// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/thumbnail_source.h"

#include "base/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "chrome/common/url_constants.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

// The delimiter between the first url and the fallback url passed to
// StartDataRequest.
const char kUrlDelimiter[] = "?fb=";

// Set ThumbnailService now as Profile isn't thread safe.
ThumbnailSource::ThumbnailSource(Profile* profile, bool capture_thumbnails)
    : thumbnail_service_(ThumbnailServiceFactory::GetForProfile(profile)),
      capture_thumbnails_(capture_thumbnails),
      image_data_fetcher_(profile->GetRequestContext()),
      weak_ptr_factory_(this) {}

ThumbnailSource::~ThumbnailSource() = default;

std::string ThumbnailSource::GetSource() const {
  return capture_thumbnails_ ?
      chrome::kChromeUIThumbnailHost2 : chrome::kChromeUIThumbnailHost;
}

void ThumbnailSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  GURL page_url;
  GURL fallback_thumbnail_url;
  ExtractPageAndThumbnailUrls(path, &page_url, &fallback_thumbnail_url);

  scoped_refptr<base::RefCountedMemory> data;
  if (page_url.is_valid() &&
      thumbnail_service_->GetPageThumbnail(page_url, capture_thumbnails_,
                                           &data)) {
    // If a local thumbnail is available for the page's URL, provide it.
    callback.Run(data.get());
  } else if (fallback_thumbnail_url.is_valid()) {
    // Otherwise, if a fallback thumbnail URL was provided, fetch it and
    // eventually return it.
    image_data_fetcher_.FetchImageData(
        fallback_thumbnail_url,
        base::Bind(&ThumbnailSource::SendFetchedUrlImage,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  } else {
    callback.Run(nullptr);
  }
  if (page_url.is_valid() && capture_thumbnails_)
    thumbnail_service_->AddForcedURL(page_url);
}

std::string ThumbnailSource::GetMimeType(const std::string&) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  // TODO(treib): This isn't correct for remote thumbnails (in
  // SendFetchedUrlImage), which are usually jpeg.
  return "image/png";
}

scoped_refptr<base::SingleThreadTaskRunner>
ThumbnailSource::TaskRunnerForRequestPath(const std::string& path) const {
  // TopSites can be accessed from the IO thread. Otherwise, the URLs should be
  // accessed on the UI thread.
  // TODO(treib): |thumbnail_service_| is assumed to be non-null in other
  // places, so probably this check isn't necessary?
  return thumbnail_service_.get()
             ? nullptr
             : content::URLDataSource::TaskRunnerForRequestPath(path);
}

bool ThumbnailSource::AllowCaching() const {
  return false;
}

bool ThumbnailSource::ShouldServiceRequest(
    const GURL& url,
    content::ResourceContext* resource_context,
    int render_process_id) const {
  if (url.SchemeIs(chrome::kChromeSearchScheme)) {
    return InstantIOContext::ShouldServiceRequest(url, resource_context,
                                                  render_process_id);
  }
  return URLDataSource::ShouldServiceRequest(url, resource_context,
                                             render_process_id);
}

void ThumbnailSource::ExtractPageAndThumbnailUrls(
    const std::string& path,
    GURL* page_url,
    GURL* fallback_thumbnail_url) {
  std::string page_url_str = path;
  std::string fallback_thumbnail_url_str;

  size_t pos = path.find(kUrlDelimiter);
  if (pos != std::string::npos) {
    page_url_str = path.substr(0, pos);
    fallback_thumbnail_url_str = path.substr(pos + strlen(kUrlDelimiter));
  }

  *page_url = GURL(page_url_str);
  *fallback_thumbnail_url = GURL(fallback_thumbnail_url_str);
}

void ThumbnailSource::SendFetchedUrlImage(
    const content::URLDataSource::GotDataCallback& callback,
    const std::string& image_data,
    const image_fetcher::RequestMetadata& metadata) {
  if (image_data.empty()) {
    callback.Run(nullptr);
    return;
  }
  std::string image_data_copy = image_data;
  callback.Run(base::RefCountedString::TakeString(&image_data_copy));
}
