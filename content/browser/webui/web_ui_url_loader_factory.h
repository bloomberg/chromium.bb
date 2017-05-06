// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_URL_LOADER_FACTORY_H_

#include "content/common/url_loader_factory.mojom.h"

namespace content {
class FrameTreeNode;

// Returns a URLLoaderFactory interface pointer for serving WebUI requests.
mojom::URLLoaderFactoryPtr GetWebUIURLLoader(FrameTreeNode* node);

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_URL_LOADER_FACTORY_H_
