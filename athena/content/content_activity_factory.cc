// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/public/content_activity_factory.h"

#include "athena/content/web_activity.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace athena {

ContentActivityFactory::ContentActivityFactory() {
}

ContentActivityFactory::~ContentActivityFactory() {}

Activity* ContentActivityFactory::CreateWebActivity(
    content::BrowserContext* browser_context,
    const GURL& url) {
  content::WebContents::CreateParams params(browser_context);
  content::WebContents* contents = content::WebContents::Create(params);
  contents->GetController().LoadURL(url,
                                    content::Referrer(),
                                    content::PAGE_TRANSITION_TYPED,
                                    std::string());
  WebActivity* activity = new WebActivity(contents);
  // TODO(mukai): it might be better to move Focus to another place.
  contents->Focus();
  return activity;
}

}  // namespace athena
