// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_MAC_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_MAC_H_
#pragma once

#include "chrome/browser/extensions/extension_host.h"

// TODO(mpcomplete): I don't know what this does or if it is needed anymore,
// now that ExtensionHost is restructured to rely on WebContents.
class ExtensionHostMac : public ExtensionHost {
 public:
  ExtensionHostMac(const Extension* extension,
                   content::SiteInstance* site_instance,
                   const GURL& url, content::ViewType host_type) :
      ExtensionHost(extension, site_instance, url, host_type) {}
  virtual ~ExtensionHostMac();

 private:
  virtual void UnhandledKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ExtensionHostMac);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_HOST_MAC_H_
