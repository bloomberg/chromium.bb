// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/icon_helper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/favicon_url.h"
#include "third_party/skia/include/core/SkBitmap.h"

using content::BrowserThread;
using content::WebContents;

namespace android_webview {

IconHelper::IconHelper(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      listener_(NULL) {
}

IconHelper::~IconHelper() {
}

void IconHelper::SetListener(Listener* listener) {
  listener_ = listener;
}

void IconHelper::DownloadFaviconCallback(
  int id, const GURL& image_url, int requested_size,
  const std::vector<SkBitmap>& bitmaps) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (bitmaps.size() == 0) {
    return;
  }

  // We can protentially have multiple frames of the icon
  // in different sizes. We need more fine grain API spec
  // to let clients pick out the frame they want.

  // TODO(acleung): Pick the best icon to return based on size.
  if (listener_)
    listener_->OnReceivedIcon(bitmaps[0]);
}

void IconHelper::DidUpdateFaviconURL(int32 page_id,
    const std::vector<content::FaviconURL>& candidates) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (std::vector<content::FaviconURL>::const_iterator i = candidates.begin();
       i != candidates.end(); ++i) {
    if (!i->icon_url.is_valid())
      continue;

    switch(i->icon_type) {
      case content::FaviconURL::FAVICON:
        // TODO(acleung): only fetch the URL if favicon downloading is enabled.
        // (currently that is, the app has called WebIconDatabase.open()
        // but we should decouple that setting via a boolean setting)
        web_contents()->DownloadFavicon(i->icon_url,
            true,
            0,
            base::Bind(
                &IconHelper::DownloadFaviconCallback, base::Unretained(this)));
        break;
      case content::FaviconURL::TOUCH_ICON:
        if (listener_)
          listener_->OnReceivedTouchIconUrl(i->icon_url.spec(), false);
        break;
      case content::FaviconURL::TOUCH_PRECOMPOSED_ICON:
        if (listener_)
          listener_->OnReceivedTouchIconUrl(i->icon_url.spec(), true);
        break;
      case content::FaviconURL::INVALID_ICON:
        // Silently ignore it. Only trigger a callback on valid icons.
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

}  // namespace android_webview
