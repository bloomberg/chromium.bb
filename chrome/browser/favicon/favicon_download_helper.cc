// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_download_helper.h"

#include "chrome/browser/favicon/favicon_download_helper_delegate.h"
#include "chrome/common/icon_messages.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

using content::WebContents;

namespace {
static int StartDownload(content::RenderViewHost* rvh,
                           const GURL& url,
                           int image_size) {
  static int id = 0;
  rvh->Send(new IconMsg_DownloadFavicon(rvh->GetRoutingID(), ++id, url,
                                        image_size));
  return id;
}
}  // namespace.

FaviconDownloadHelper::FaviconDownloadHelper(
    WebContents* web_contents,
    FaviconDownloadHelperDelegate* delegate)
    : content::WebContentsObserver(web_contents),
      delegate_(delegate) {
  DCHECK(delegate_);
}

FaviconDownloadHelper::~FaviconDownloadHelper() {
}

int FaviconDownloadHelper::DownloadFavicon(const GURL& url, int image_size) {
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  int id = StartDownload(host, url, image_size);
  DownloadIdList::iterator i =
      std::find(download_ids_.begin(), download_ids_.end(), id);
  DCHECK(i == download_ids_.end());
  download_ids_.insert(id);
  return id;
}

bool FaviconDownloadHelper::OnMessageReceived(const IPC::Message& message) {
  bool message_handled = false;   // Allow other handlers to receive these.
  IPC_BEGIN_MESSAGE_MAP(FaviconDownloadHelper, message)
    IPC_MESSAGE_HANDLER(IconHostMsg_DidDownloadFavicon, OnDidDownloadFavicon)
    IPC_MESSAGE_HANDLER(IconHostMsg_UpdateFaviconURL, OnUpdateFaviconURL)
    IPC_MESSAGE_UNHANDLED(message_handled = false)
  IPC_END_MESSAGE_MAP()
  return message_handled;
}

void FaviconDownloadHelper::OnDidDownloadFavicon(
    int id,
    const GURL& image_url,
    bool errored,
    int requested_size,
    const std::vector<SkBitmap>& bitmaps) {
  DownloadIdList::iterator i =
      std::find(download_ids_.begin(), download_ids_.end(), id);
  if (i == download_ids_.end()) {
    // Currently WebContents notifies us of ANY downloads so that it is
    // possible to get here.
    return;
  }
  delegate_->OnDidDownloadFavicon(
      id, image_url, errored, requested_size, bitmaps);
  download_ids_.erase(i);
}

void FaviconDownloadHelper::OnUpdateFaviconURL(
    int32 page_id,
    const std::vector<FaviconURL>& candidates) {
  delegate_->OnUpdateFaviconURL(page_id, candidates);
}
