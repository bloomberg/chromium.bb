// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/empty_web_ui_factory.h"

namespace content {

EmptyWebUIFactory::EmptyWebUIFactory() {
}

EmptyWebUIFactory::~EmptyWebUIFactory() {
}

WebUI* EmptyWebUIFactory::CreateWebUIForURL(TabContents* source,
                                            const GURL& url) const {
  return NULL;
}

WebUI::TypeID EmptyWebUIFactory::GetWebUIType(
    content::BrowserContext* browser_context, const GURL& url) const {
  return WebUI::kNoWebUI;
}

bool EmptyWebUIFactory::UseWebUIForURL(
    content::BrowserContext* browser_context, const GURL& url) const {
  return false;
}

bool EmptyWebUIFactory::HasWebUIScheme(const GURL& url) const {
  return false;
}

bool EmptyWebUIFactory::IsURLAcceptableForWebUI(
    content::BrowserContext* browser_context,
    const GURL& url) const {
  return false;
}

// static
EmptyWebUIFactory* EmptyWebUIFactory::GetInstance() {
  return Singleton<EmptyWebUIFactory>::get();
}

}  // namespace content
