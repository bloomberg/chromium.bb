// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_content_browser_client.h"

#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "content/test/test_web_contents_view.h"
#include "googleurl/src/gurl.h"

namespace content {

TestContentBrowserClient::TestContentBrowserClient() {
}

TestContentBrowserClient::~TestContentBrowserClient() {
}

WebContentsView* TestContentBrowserClient::OverrideCreateWebContentsView(
    WebContents* web_contents,
    RenderViewHostDelegateView** render_view_host_delegate_view) {
#if defined(OS_IOS)
  return NULL;
#else
  TestWebContentsView* rv = new TestWebContentsView;
  *render_view_host_delegate_view = rv;
  return rv;
#endif
}

base::FilePath TestContentBrowserClient::GetDefaultDownloadDirectory() {
  if (!download_dir_.IsValid()) {
    bool result = download_dir_.CreateUniqueTempDir();
    CHECK(result);
  }
  return download_dir_.path();
}

}  // namespace content
