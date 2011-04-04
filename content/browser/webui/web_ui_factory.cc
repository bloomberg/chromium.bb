// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_factory.h"

#include "content/browser/content_browser_client.h"

namespace content {

// static
WebUIFactory* WebUIFactory::Get() {
  return content::GetContentClient()->browser()->GetWebUIFactory();
}

}  // namespace content
