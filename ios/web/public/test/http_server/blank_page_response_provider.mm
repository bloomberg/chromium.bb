// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/http_server/blank_page_response_provider.h"

#include <map>

#include "base/memory/ptr_util.h"
#import "ios/web/public/test/http_server/html_response_provider.h"
#include "url/gurl.h"

namespace web {
namespace test {

// The HTML used for the blank page ResponseProvider.
const char kBlankTestPageHtml[] = "<!DOCTYPE html><html><body></body></html>";

std::unique_ptr<ResponseProvider> CreateBlankPageResponseProvider(
    const GURL& url) {
  std::map<GURL, std::string> responses;
  responses[url] = kBlankTestPageHtml;
  return base::MakeUnique<HtmlResponseProvider>(responses);
}

}  // namespace test
}  // namespace web
