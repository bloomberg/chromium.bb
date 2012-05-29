// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mock_content_browser_client.h"

#include <string>

#include "base/file_path.h"
#include "base/logging.h"
#include "content/test/test_web_contents_view.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"

namespace content {

MockContentBrowserClient::MockContentBrowserClient() {
}

MockContentBrowserClient::~MockContentBrowserClient() {
}

WebContentsView* MockContentBrowserClient::OverrideCreateWebContentsView(
    WebContents* web_contents,
    RenderViewHostDelegateView** render_view_host_delegate_view) {
  TestWebContentsView* rv = new TestWebContentsView;
  *render_view_host_delegate_view = rv;
  return rv;
}

ui::Clipboard* MockContentBrowserClient::GetClipboard() {
  static ui::Clipboard clipboard;
  return &clipboard;
}

FilePath MockContentBrowserClient::GetDefaultDownloadDirectory() {
  if (!download_dir_.IsValid()) {
    bool result = download_dir_.CreateUniqueTempDir();
    CHECK(result);
  }
  return download_dir_.path();
}

}  // namespace content
