// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TEST_CHROME_WEB_UI_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_TEST_CHROME_WEB_UI_FACTORY_H_
#pragma once

#include <functional>
#include <map>
#include <string>

#include "chrome/browser/ui/webui/chrome_web_ui_factory.h"
#include "content/browser/webui/web_ui.h"

class GURL;
class Profile;
class TabContents;

// This class replaces the ChromeWebUIFactory when the switches::kTestType flag
// is passed. It provides a registry to override CreateWebUIForURL() by host.
class TestChromeWebUIFactory : public ChromeWebUIFactory {
 public:
  // Interface to create a new WebUI object.
  class WebUIProvider {
   public:
    // Create and return a new WebUI object for the |tab_contents| based on the
    // |url|.
    virtual WebUI* NewWebUI(TabContents* tab_contents,
                            const GURL& url) = 0;

   protected:
    virtual ~WebUIProvider();
  };

  typedef std::map<std::string, WebUIProvider*> FactoryOverridesMap;

  // Override the creation for urls having |host| with |provider|.
  static void AddFactoryOverride(const std::string& host,
                                 WebUIProvider* provider);

  // Remove the override for urls having |host|.
  static void RemoveFactoryOverride(const std::string& host);

  // ChromeWebUIFactory overrides.
  virtual WebUI::TypeID GetWebUIType(Profile* profile,
                                     const GURL& url) const OVERRIDE;
  virtual WebUI* CreateWebUIForURL(TabContents* tab_contents,
                                   const GURL& url) const OVERRIDE;

  // Return the singleton instance.
  static TestChromeWebUIFactory* GetInstance();

 private:
  TestChromeWebUIFactory();
  virtual ~TestChromeWebUIFactory();

  friend struct DefaultSingletonTraits<TestChromeWebUIFactory>;

  // Return the WebUIProvider for the |url|'s host if it exists, otherwise NULL.
  WebUIProvider* GetWebUIProvider(Profile* profile, const GURL& url) const;

  // Stores the mapping of host to WebUIProvider.
  FactoryOverridesMap factory_overrides_;

  DISALLOW_COPY_AND_ASSIGN(TestChromeWebUIFactory);
};

#endif  // CHROME_BROWSER_UI_WEBUI_TEST_CHROME_WEB_UI_FACTORY_H_
