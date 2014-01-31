// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_TOOLBAR_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_TOOLBAR_MODEL_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/toolbar/toolbar_model_delegate.h"

class Browser;

// Implementation of ToolbarModelDelegate which uses an instance of
// Browser in order to fulfil its duties.
class BrowserToolbarModelDelegate : public ToolbarModelDelegate {
 public:
  explicit BrowserToolbarModelDelegate(Browser* browser);
  virtual ~BrowserToolbarModelDelegate();

  // ToolbarModelDelegate:
  virtual content::WebContents* GetActiveWebContents() const OVERRIDE;
  virtual bool InTabbedBrowser() const OVERRIDE;

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserToolbarModelDelegate);
};

#endif  // CHROME_BROWSER_UI_BROWSER_TOOLBAR_MODEL_DELEGATE_H_
