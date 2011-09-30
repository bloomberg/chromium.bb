// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_EMPTY_WEB_UI_FACTORY_H_
#define CONTENT_BROWSER_WEBUI_EMPTY_WEB_UI_FACTORY_H_

#include "base/memory/singleton.h"
#include "content/browser/webui/web_ui_factory.h"
#include "content/common/content_export.h"

namespace content {

// A stubbed out version of WebUIFactory.
class CONTENT_EXPORT EmptyWebUIFactory : public content::WebUIFactory {
 public:
  // Returns the singleton instance.
  static EmptyWebUIFactory* GetInstance();

  virtual WebUI* CreateWebUIForURL(TabContents* source,
                                   const GURL& url) const OVERRIDE;
  virtual WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                     const GURL& url) const OVERRIDE;
  virtual bool UseWebUIForURL(content::BrowserContext* browser_context,
                              const GURL& url) const OVERRIDE;
  virtual bool HasWebUIScheme(const GURL& url) const OVERRIDE;
  virtual bool IsURLAcceptableForWebUI(content::BrowserContext* browser_context,
                                       const GURL& url) const OVERRIDE;

 protected:
  EmptyWebUIFactory();
  virtual ~EmptyWebUIFactory();

 private:
  friend struct DefaultSingletonTraits<EmptyWebUIFactory>;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_EMPTY_WEB_UI_FACTORY_H_
