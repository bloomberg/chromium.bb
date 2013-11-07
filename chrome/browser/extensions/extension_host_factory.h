// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_FACTORY_H_

#include "base/basictypes.h"

class Browser;
class GURL;
class Profile;

namespace extensions {

class ExtensionHost;

// A utility class to make ExtensionHosts, specifically those with browser-
// specific needs.
class ExtensionHostFactory {
 public:
  // Creates a new ExtensionHost with its associated view, grouping it in the
  // appropriate SiteInstance (and therefore process) based on the URL and
  // profile.
  static ExtensionHost* CreatePopupHost(const GURL& url, Browser* browser);
  static ExtensionHost* CreateInfobarHost(const GURL& url, Browser* browser);

  // Some dialogs may not be associated with a particular browser window and
  // hence only require a |profile|.
  static ExtensionHost* CreateDialogHost(const GURL& url, Profile* profile);

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionHostFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_FACTORY_H_
