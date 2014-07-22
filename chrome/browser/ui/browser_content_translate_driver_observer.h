// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_CONTENT_TRANSLATE_DRIVER_OBSERVER_H_
#define CHROME_BROWSER_UI_BROWSER_CONTENT_TRANSLATE_DRIVER_OBSERVER_H_

#include "components/translate/content/browser/content_translate_driver.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"

class Browser;

// Implementation of ContentTranslateDriver::Observer for Browser. This observes
// wether Translate is enabled or not, and updates toolbar in response to
// changes in state of translate.
class BrowserContentTranslateDriverObserver
    : public translate::ContentTranslateDriver::Observer {
 public:
  explicit BrowserContentTranslateDriverObserver(Browser* browser);

  virtual ~BrowserContentTranslateDriverObserver();

  // Overridden from ContentTranslateDriver::Observer
  virtual void OnIsPageTranslatedChanged(content::WebContents* source) OVERRIDE;
  virtual void OnTranslateEnabledChanged(content::WebContents* source) OVERRIDE;

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserContentTranslateDriverObserver);
};

#endif  // CHROME_BROWSER_UI_BROWSER_CONTENT_TRANSLATE_DRIVER_OBSERVER_H_
