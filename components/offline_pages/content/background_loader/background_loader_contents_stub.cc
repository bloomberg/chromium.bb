// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/content/background_loader/background_loader_contents_stub.h"

#include "base/logging.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"

namespace background_loader {

BackgroundLoaderContentsStub::BackgroundLoaderContentsStub(
    content::BrowserContext* browser_context)
    : BackgroundLoaderContents(), is_loading_(false) {
  BackgroundLoaderContents::web_contents_.reset(
      content::WebContentsTester::CreateTestWebContents(browser_context, NULL));
  web_contents_.get()->SetDelegate(this);
}

BackgroundLoaderContentsStub::~BackgroundLoaderContentsStub() {}

void BackgroundLoaderContentsStub::LoadPage(const GURL& url) {
  is_loading_ = true;
}

}  // namespace background_loader
