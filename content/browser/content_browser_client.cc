// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_browser_client.h"

#include "base/memory/singleton.h"
#include "content/browser/webui/empty_web_ui_factory.h"

namespace content {

WebUIFactory* ContentBrowserClient::GetWebUIFactory() {
  // Return an empty factory so callsites don't have to check for NULL.
  return EmptyWebUIFactory::Get();
}

}  // namespace content
