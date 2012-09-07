// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_PAGE_ACTION_CONTROLLER_H_

#include <set>
#include <string>

#include "base/observer_list.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "content/public/browser/web_contents_observer.h"

class ExtensionService;

namespace extensions {

// A LocationBarController which populates the location bar with icons based
// on the page_action extension API.
class PageActionController : public LocationBarController,
                             public content::WebContentsObserver {
 public:
  explicit PageActionController(content::WebContents* web_contents);
  virtual ~PageActionController();

  // LocationBarController implementation.
  virtual std::vector<ExtensionAction*> GetCurrentActions() const OVERRIDE;
  // Page actions can't try to get attention.
  virtual void GetAttentionFor(const std::string& extension_id) OVERRIDE {}
  virtual Action OnClicked(const std::string& extension_id,
                           int mouse_button) OVERRIDE;
  virtual void NotifyChange() OVERRIDE;

  // content::WebContentsObserver implementation.
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

 private:
  // Gets the ExtensionService for the web contents.
  ExtensionService* GetExtensionService() const;

  DISALLOW_COPY_AND_ASSIGN(PageActionController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PAGE_ACTION_CONTROLLER_H_
