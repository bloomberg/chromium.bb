// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_util.h"

#include "chrome/common/icon_messages.h"
#include "content/public/browser/render_view_host.h"
#include "googleurl/src/gurl.h"

// static
int FaviconUtil::DownloadFavicon(content::RenderViewHost* rvh,
                                 const GURL& url,
                                 int image_size) {
  static int id = 0;
  rvh->Send(new IconMsg_DownloadFavicon(rvh->GetRoutingID(), ++id, url,
            image_size));
  return id;
}
