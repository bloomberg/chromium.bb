// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_content_browser_client.h"

#include "base/files/file_path.h"
#include "base/logging.h"

namespace content {

TestContentBrowserClient::TestContentBrowserClient() {
}

TestContentBrowserClient::~TestContentBrowserClient() {
}

base::FilePath TestContentBrowserClient::GetDefaultDownloadDirectory() {
  if (!download_dir_.IsValid()) {
    bool result = download_dir_.CreateUniqueTempDir();
    CHECK(result);
  }
  return download_dir_.path();
}

}  // namespace content
