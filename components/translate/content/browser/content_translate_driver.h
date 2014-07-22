// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_BROWSER_CONTENT_TRANSLATE_DRIVER_H_
#define COMPONENTS_TRANSLATE_CONTENT_BROWSER_CONTENT_TRANSLATE_DRIVER_H_

#include "base/basictypes.h"
#include "components/translate/core/browser/translate_driver.h"

namespace content {
class NavigationController;
class WebContents;
}

namespace translate {

// Content implementation of TranslateDriver.
class ContentTranslateDriver : public TranslateDriver {
 public:

  // The observer for the ContentTranslateDriver.
  class Observer {
   public:
    // Handles when the value of IsPageTranslated is changed.
    virtual void OnIsPageTranslatedChanged(content::WebContents* source) = 0;

    // Handles when the value of translate_enabled is changed.
    virtual void OnTranslateEnabledChanged(content::WebContents* source) = 0;

   protected:
    virtual ~Observer() {}
  };

  ContentTranslateDriver(content::NavigationController* nav_controller);
  virtual ~ContentTranslateDriver();

  // Sets the Observer. Calling this method is optional.
  void set_observer(Observer* observer) { observer_ = observer; }

  // TranslateDriver methods.
  virtual void OnIsPageTranslatedChanged() OVERRIDE;
  virtual void OnTranslateEnabledChanged() OVERRIDE;
  virtual bool IsLinkNavigation() OVERRIDE;
  virtual void TranslatePage(int page_seq_no,
                             const std::string& translate_script,
                             const std::string& source_lang,
                             const std::string& target_lang) OVERRIDE;
  virtual void RevertTranslation(int page_seq_no) OVERRIDE;
  virtual bool IsOffTheRecord() OVERRIDE;
  virtual const std::string& GetContentsMimeType() OVERRIDE;
  virtual const GURL& GetLastCommittedURL() OVERRIDE;
  virtual const GURL& GetActiveURL() OVERRIDE;
  virtual const GURL& GetVisibleURL() OVERRIDE;
  virtual bool HasCurrentPage() OVERRIDE;
  virtual void OpenUrlInNewTab(const GURL& url) OVERRIDE;

 private:
  // The navigation controller of the tab we are associated with.
  content::NavigationController* navigation_controller_;

  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(ContentTranslateDriver);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_BROWSER_CONTENT_TRANSLATE_DRIVER_H_
