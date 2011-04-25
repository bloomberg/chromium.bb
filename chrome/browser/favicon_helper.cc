// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon_helper.h"

#include "chrome/browser/defaults.h"
#include "chrome/browser/favicon_handler.h"
#include "chrome/common/icon_messages.h"

FaviconHelper::FaviconHelper(TabContents* tab_contents)
    : TabContentsObserver(tab_contents) {
  favicon_handler_.reset(new FaviconHandler(tab_contents,
                                            FaviconHandler::FAVICON));
  if (browser_defaults::kEnableTouchIcon)
    touch_icon_handler_.reset(new FaviconHandler(tab_contents,
                                                 FaviconHandler::TOUCH));
}

FaviconHelper::~FaviconHelper() {
}

void FaviconHelper::FetchFavicon(const GURL& url) {
  favicon_handler_->FetchFavicon(url);
  if (touch_icon_handler_.get())
    touch_icon_handler_->FetchFavicon(url);
}

int FaviconHelper::DownloadImage(const GURL& image_url,
                                 int image_size,
                                 history::IconType icon_type,
                                 ImageDownloadCallback* callback) {
  if (icon_type == history::FAVICON)
    return favicon_handler_->DownloadImage(image_url, image_size, icon_type,
                                           callback);
  else if (touch_icon_handler_.get())
    return touch_icon_handler_->DownloadImage(image_url, image_size, icon_type,
                                              callback);
  return 0;
}

bool FaviconHelper::OnMessageReceived(const IPC::Message& message) {
  bool message_handled = true;
  IPC_BEGIN_MESSAGE_MAP(FaviconHelper, message)
    IPC_MESSAGE_HANDLER(IconHostMsg_DidDownloadFavicon, OnDidDownloadFavicon)
    IPC_MESSAGE_HANDLER(IconHostMsg_UpdateFaviconURL, OnUpdateFaviconURL)
    IPC_MESSAGE_UNHANDLED(message_handled = false)
  IPC_END_MESSAGE_MAP()
  return message_handled;
}

void FaviconHelper::OnDidDownloadFavicon(int id,
                                         const GURL& image_url,
                                         bool errored,
                                         const SkBitmap& image) {
  favicon_handler_->OnDidDownloadFavicon(id, image_url, errored, image);
  if (touch_icon_handler_.get())
    touch_icon_handler_->OnDidDownloadFavicon(id, image_url, errored, image);
}

void FaviconHelper::OnUpdateFaviconURL(
    int32 page_id,
    const std::vector<FaviconURL>& candidates) {
  favicon_handler_->OnUpdateFaviconURL(page_id, candidates);
  if (touch_icon_handler_.get())
    touch_icon_handler_->OnUpdateFaviconURL(page_id, candidates);
}
