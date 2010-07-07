// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/dom_ui_thumbnail_source.h"

#include "app/resource_bundle.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "gfx/codec/jpeg_codec.h"
#include "googleurl/src/gurl.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"

DOMUIThumbnailSource::DOMUIThumbnailSource(Profile* profile)
    : DataSource(chrome::kChromeUIThumbnailPath, MessageLoop::current()),
      profile_(profile) {
}

void DOMUIThumbnailSource::StartDataRequest(const std::string& path,
                                            bool is_off_the_record,
                                            int request_id) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kTopSites)) {
    scoped_refptr<history::TopSites> top_sites = profile_->GetTopSites();
    RefCountedBytes* data = NULL;
    if (top_sites->GetPageThumbnail(GURL(path), &data)) {
      // We have the thumbnail.
      SendResponse(request_id, data);
    } else {
      SendDefaultThumbnail(request_id);
    }
    return;
  }  // end --top-sites switch

  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    HistoryService::Handle handle = hs->GetPageThumbnail(
        GURL(path),
        &cancelable_consumer_,
        NewCallback(this, &DOMUIThumbnailSource::OnThumbnailDataAvailable));
    // Attach the ChromeURLDataManager request ID to the history request.
    cancelable_consumer_.SetClientData(hs, handle, request_id);
  } else {
    // Tell the caller that no thumbnail is available.
    SendResponse(request_id, NULL);
  }
}

void DOMUIThumbnailSource::SendDefaultThumbnail(int request_id) {
  // Use placeholder thumbnail.
  if (!default_thumbnail_.get()) {
    default_thumbnail_ =
        ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
            IDR_DEFAULT_THUMBNAIL);
  }
  SendResponse(request_id, default_thumbnail_);
}

void DOMUIThumbnailSource::OnThumbnailDataAvailable(
    HistoryService::Handle request_handle,
    scoped_refptr<RefCountedBytes> data) {
  HistoryService* hs =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  int request_id = cancelable_consumer_.GetClientData(hs, request_handle);
  // Forward the data along to the networking system.
  if (data.get() && !data->data.empty()) {
    SendResponse(request_id, data);
  } else {
    SendDefaultThumbnail(request_id);
  }
}
