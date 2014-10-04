// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BROWSER_EXTENSION_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_BROWSER_EXTENSION_WINDOW_CONTROLLER_H_

#include "chrome/browser/extensions/window_controller.h"

class Browser;

namespace extensions {
class Extension;
}

class BrowserExtensionWindowController : public extensions::WindowController {
 public:
  explicit BrowserExtensionWindowController(Browser* browser);
  virtual ~BrowserExtensionWindowController();

  // extensions::WindowController implementation.
  virtual int GetWindowId() const override;
  virtual std::string GetWindowTypeText() const override;
  virtual base::DictionaryValue* CreateWindowValue() const override;
  virtual base::DictionaryValue* CreateWindowValueWithTabs(
      const extensions::Extension* extension) const override;
  virtual base::DictionaryValue* CreateTabValue(
      const extensions::Extension* extension, int tab_index) const override;
  virtual bool CanClose(Reason* reason) const override;
  virtual void SetFullscreenMode(bool is_fullscreen,
                                 const GURL& extension_url) const override;
  virtual Browser* GetBrowser() const override;
  virtual bool IsVisibleToExtension(
      const extensions::Extension* extension) const override;

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserExtensionWindowController);
};

#endif  // CHROME_BROWSER_EXTENSIONS_BROWSER_EXTENSION_WINDOW_CONTROLLER_H_
