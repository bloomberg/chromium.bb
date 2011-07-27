// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/test_chrome_web_ui_factory.h"

#include "content/browser/tab_contents/tab_contents.h"

TestChromeWebUIFactory::WebUIProvider::~WebUIProvider() {
}

TestChromeWebUIFactory::TestChromeWebUIFactory() {
}

TestChromeWebUIFactory::~TestChromeWebUIFactory() {
}

void TestChromeWebUIFactory::AddFactoryOverride(const std::string& host,
                                                WebUIProvider* provider) {
  DCHECK_EQ(0U, GetInstance()->factory_overrides_.count(host));
  GetInstance()->factory_overrides_[host] = provider;
}

void TestChromeWebUIFactory::RemoveFactoryOverride(const std::string& host) {
  DCHECK_EQ(1U, GetInstance()->factory_overrides_.count(host));
  GetInstance()->factory_overrides_.erase(host);
}

WebUI::TypeID TestChromeWebUIFactory::GetWebUIType(
    content::BrowserContext* browser_context, const GURL& url) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  WebUIProvider* provider = GetWebUIProvider(profile, url);
  return provider ? reinterpret_cast<WebUI::TypeID>(provider) :
      ChromeWebUIFactory::GetWebUIType(profile, url);
}

WebUI* TestChromeWebUIFactory::CreateWebUIForURL(TabContents* tab_contents,
                                                 const GURL& url) const {
  WebUIProvider* provider = GetWebUIProvider(tab_contents->profile(), url);
  return provider ? provider->NewWebUI(tab_contents, url) :
      ChromeWebUIFactory::CreateWebUIForURL(tab_contents, url);
}

TestChromeWebUIFactory* TestChromeWebUIFactory::GetInstance() {
  return static_cast<TestChromeWebUIFactory*>(
      ChromeWebUIFactory::GetInstance());
}

TestChromeWebUIFactory::WebUIProvider* TestChromeWebUIFactory::GetWebUIProvider(
    Profile* profile, const GURL& url) const {
  FactoryOverridesMap::const_iterator found =
      factory_overrides_.find(url.host());
  return (found == factory_overrides_.end()) ? NULL : found->second;
}
