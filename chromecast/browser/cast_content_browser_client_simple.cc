// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_browser_client.h"

#include "content/public/browser/browser_message_filter.h"

namespace chromecast {
namespace shell {

void CastContentBrowserClient::PlatformAppendExtraCommandLineSwitches(
    base::CommandLine* command_line) {
}

std::vector<scoped_refptr<content::BrowserMessageFilter>>
CastContentBrowserClient::PlatformGetBrowserMessageFilters() {
  return std::vector<scoped_refptr<content::BrowserMessageFilter>>();
}

}  // namespace shell
}  // namespace chromecast
