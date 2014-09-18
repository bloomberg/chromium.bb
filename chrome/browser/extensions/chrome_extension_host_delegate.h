// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_HOST_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_HOST_DELEGATE_H_

#include "extensions/browser/extension_host_delegate.h"

namespace extensions {

// Chrome support for ExtensionHost.
class ChromeExtensionHostDelegate : public ExtensionHostDelegate {
 public:
  ChromeExtensionHostDelegate();
  virtual ~ChromeExtensionHostDelegate();

  // ExtensionHostDelegate implementation.
  virtual void OnExtensionHostCreated(content::WebContents* web_contents)
      OVERRIDE;
  virtual void OnRenderViewCreatedForBackgroundPage(ExtensionHost* host)
      OVERRIDE;
  virtual content::JavaScriptDialogManager* GetJavaScriptDialogManager()
      OVERRIDE;
  virtual void CreateTab(content::WebContents* web_contents,
                         const std::string& extension_id,
                         WindowOpenDisposition disposition,
                         const gfx::Rect& initial_pos,
                         bool user_gesture) OVERRIDE;
  virtual void ProcessMediaAccessRequest(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback,
      const Extension* extension) OVERRIDE;
  virtual bool CheckMediaAccessPermission(content::WebContents* web_contents,
                                          const GURL& security_origin,
                                          content::MediaStreamType type,
                                          const Extension* extension) OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_HOST_DELEGATE_H_
