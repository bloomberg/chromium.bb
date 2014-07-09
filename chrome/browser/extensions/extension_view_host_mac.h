// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_MAC_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_MAC_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_view_host.h"

namespace extensions {
class Extension;

// TODO(mpcomplete): I don't know what this does or if it is needed anymore,
// now that ExtensionHost is restructured to rely on WebContents.
class ExtensionViewHostMac : public ExtensionViewHost {
 public:
  ExtensionViewHostMac(const Extension* extension,
                       content::SiteInstance* site_instance,
                       const GURL& url,
                       ViewType host_type)
      : ExtensionViewHost(extension, site_instance, url, host_type) {}
  virtual ~ExtensionViewHostMac();

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionViewHostMac);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_MAC_H_
