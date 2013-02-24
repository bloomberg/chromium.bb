// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_DELEGATE_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"

class GURL;

namespace content {

class RenderViewHost;

class DevToolsHttpHandlerDelegate {
 public:
  enum TargetType {
    kTargetTypeTab = 0,
    kTargetTypeOther,
  };

  virtual ~DevToolsHttpHandlerDelegate() {}

  // Should return discovery page HTML that should list available tabs
  // and provide attach links.
  virtual std::string GetDiscoveryPageHTML() = 0;

  // Returns true if and only if frontend resources are bundled.
  virtual bool BundlesFrontendResources() = 0;

  // Returns path to the front-end files on the local filesystem for debugging.
  virtual base::FilePath GetDebugFrontendDir() = 0;

  // Get a thumbnail for a given page. Returns non-empty string iff we have the
  // thumbnail.
  virtual std::string GetPageThumbnailData(const GURL& url) = 0;

  // Creates new inspectable target and returns its render view host.
  virtual RenderViewHost* CreateNewTarget() = 0;

  // Returns the type of the target.
  virtual TargetType GetTargetType(RenderViewHost*) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_HTTP_HANDLER_DELEGATE_H_
