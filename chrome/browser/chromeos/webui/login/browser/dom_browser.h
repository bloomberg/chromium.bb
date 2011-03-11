// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_BROWSER_DOM_BROWSER_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_BROWSER_DOM_BROWSER_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/ui/browser.h"

class Profile;

namespace chromeos {

// DOMBrowser is an alternate implementation of Browser that is used to display
// DOM login screens. These screens in addition to having a webpage in a DOMView
// also display the Status Area and the touch enabled keyboard.
// TODO(rharrison): Implement DOM versions of BrowserView, BrowserFrame,
// BrowserFrameView.
// TODO(rharrison): Add support for OOBE and screen lock DOM screens.
class DOMBrowser : public Browser {
 public:
  explicit DOMBrowser(Profile* profile);
  virtual ~DOMBrowser();

  // This is a factory method for creating a DOMBrowser. It is based off of the
  // CreateFor* methods from the Browser class.
  static DOMBrowser* CreateForDOM(Profile* profile);

 protected:
  // Creates Window for DOMBrowser.
  // TODO(rharrison): Change to create window that uses DOMBrowser* objects.
  virtual BrowserWindow* CreateBrowserWindow();

 private:
  DISALLOW_COPY_AND_ASSIGN(DOMBrowser);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_LOGIN_BROWSER_DOM_BROWSER_H_
