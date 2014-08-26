// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/content_activity_factory.h"

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

ActivityFactory* CreateContentActivityFactory() {
  return new ContentActivityFactory();
}

}  // namespace athena
