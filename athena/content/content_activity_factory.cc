// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/public/content_activity_factory.h"

#include "athena/content/web_activity.h"
#include "base/logging.h"

namespace athena {

ContentActivityFactory::ContentActivityFactory() {
}

ContentActivityFactory::~ContentActivityFactory() {}

Activity* ContentActivityFactory::CreateWebActivity(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return new WebActivity(browser_context, url);
}

Activity* ContentActivityFactory::CreateAppActivity(
    content::BrowserContext* browser_context,
    const std::string& app_id) {
  // TODO(mukai): port the extension system and launch the app actually.
  GURL url;
  if (app_id == "mail")
    url = GURL("https://mail.google.com/");
  else if (app_id == "calendar")
    url = GURL("https://calendar.google.com/");
  else if (app_id == "video")
    url = GURL("http://youtube.com/");
  else if (app_id == "music")
    url = GURL("https://play.google.com/music");
  else if (app_id == "contact")
    url = GURL("https://www.google.com/contacts");
  else
    LOG(FATAL) << "Unknown app id: " << app_id;
  DCHECK(!url.is_empty());
  return CreateWebActivity(browser_context, url);
}

}  // namespace athena
