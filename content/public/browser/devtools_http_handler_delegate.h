// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_DELEGATE_H_

#include <string>
#include <vector>

#include "base/file_path.h"

class GURL;

namespace content {

class WebContents;

class DevToolsHttpHandlerDelegate {
 public:
  virtual ~DevToolsHttpHandlerDelegate() {}

  // Should return discovery page HTML that should list available tabs
  // and provide attach links. Called on the IO thread.
  virtual std::string GetDiscoveryPageHTML() = 0;

  // Returns true if and only if frontend resources are bundled.
  virtual bool BundlesFrontendResources() = 0;

  // Returns path to the front-end files on the local filesystem for debugging.
  virtual FilePath GetDebugFrontendDir() = 0;

  // Get a thumbnail for a given page. Returns non-empty string iff we have the
  // thumbnail.
  virtual std::string GetPageThumbnailData(const GURL& url) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_DELEGATE_H_
