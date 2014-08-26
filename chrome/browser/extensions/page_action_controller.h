// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_PAGE_ACTION_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/extensions/location_bar_controller.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {
class Extension;

// A LocationBarControllerProvider which populates the location bar with icons
// based on the page_action extension API.
// TODO(rdevlin.cronin): This isn't really a controller.
class PageActionController : public LocationBarController::ActionProvider {
 public:
  explicit PageActionController(content::WebContents* web_contents);
  virtual ~PageActionController();

  // LocationBarController::Provider implementation.
  virtual ExtensionAction* GetActionForExtension(const Extension* extension)
      OVERRIDE;
  virtual void OnNavigated() OVERRIDE;

 private:
  // The associated WebContents.
  content::WebContents* web_contents_;

  // The associated browser context.
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(PageActionController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PAGE_ACTION_CONTROLLER_H_
