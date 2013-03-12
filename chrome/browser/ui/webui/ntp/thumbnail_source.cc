// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/thumbnail_source.h"

#include "base/callback.h"
#include "base/message_loop.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/instant/instant_io_context.h"
#include "chrome/browser/instant/instant_service.h"
#include "chrome/browser/instant/instant_service_factory.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/theme_resources.h"
#include "net/url_request/url_request.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;

// Set ThumbnailService now as Profile isn't thread safe.
ThumbnailSource::ThumbnailSource(Profile* profile)
    : thumbnail_service_(ThumbnailServiceFactory::GetForProfile(profile)),
      current_request_(NULL),
      profile_(profile) {
}

ThumbnailSource::~ThumbnailSource() {
}

std::string ThumbnailSource::GetSource() {
  return chrome::kChromeUIThumbnailHost;
}

void ThumbnailSource::StartDataRequest(
    const std::string& raw_path,
    bool is_incognito,
    const content::URLDataSource::GotDataCallback& callback) {
  // Translate to regular path if |raw_path| is of the form
  // chrome-search://favicon/<rid>, where rid is a uint64.
  std::string path = raw_path;
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    path = InstantService::MaybeTranslateInstantPathOnIO(
        current_request_, raw_path);
  } else if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    path = InstantService::MaybeTranslateInstantPathOnUI(profile_, raw_path);
  }

  scoped_refptr<base::RefCountedMemory> data;
  if (thumbnail_service_->GetPageThumbnail(GURL(path), &data)) {
    // We have the thumbnail.
    callback.Run(data.get());
  } else {
    callback.Run(default_thumbnail_);
  }
  current_request_ = NULL;
}

std::string ThumbnailSource::GetMimeType(const std::string&) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}

MessageLoop* ThumbnailSource::MessageLoopForRequestPath(
    const std::string& path) const {
  // TopSites can be accessed from the IO thread.
  return thumbnail_service_.get() ?
      NULL : content::URLDataSource::MessageLoopForRequestPath(path);
}

bool ThumbnailSource::ShouldServiceRequest(
    const net::URLRequest* request) const {
  current_request_ = request;
  if (request->url().SchemeIs(chrome::kChromeSearchScheme)) {
    return InstantService::IsInstantPath(request->url()) &&
        InstantIOContext::ShouldServiceRequest(request);
  }
  return URLDataSource::ShouldServiceRequest(request);
}
