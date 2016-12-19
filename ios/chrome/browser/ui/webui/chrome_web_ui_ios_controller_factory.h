// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_IOS_CONTROLLER_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_IOS_CONTROLLER_FACTORY_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/favicon_base/favicon_callback.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_controller_factory.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedMemory;
}

namespace ios {
class ChromeBrowserState;
}

class GURL;

class ChromeWebUIIOSControllerFactory : public web::WebUIIOSControllerFactory {
 public:
  web::WebUIIOSController* CreateWebUIIOSControllerForURL(
      web::WebUIIOS* web_ui,
      const GURL& url) const override;

  static ChromeWebUIIOSControllerFactory* GetInstance();

  // Get the favicon for |page_url| and run |callback| with result when loaded.
  // Note. |callback| is always run asynchronously.
  void GetFaviconForURL(
      ios::ChromeBrowserState* browser_state,
      const GURL& page_url,
      const std::vector<int>& desired_sizes_in_pixel,
      const favicon_base::FaviconResultsCallback& callback) const;

 protected:
  ChromeWebUIIOSControllerFactory();
  ~ChromeWebUIIOSControllerFactory() override;

 private:
  friend struct base::DefaultSingletonTraits<ChromeWebUIIOSControllerFactory>;

  // Gets the data for the favicon for a WebUIIOS page. Returns nullptr if the
  // WebUIIOS does not have a favicon.
  // The returned favicon data must be |gfx::kFaviconSize| x |gfx::kFaviconSize|
  // DIP. GetFaviconForURL() should be updated if this changes.
  base::RefCountedMemory* GetFaviconResourceBytes(
      const GURL& page_url,
      ui::ScaleFactor scale_factor) const;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebUIIOSControllerFactory);
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_CHROME_WEB_UI_IOS_CONTROLLER_FACTORY_H_
