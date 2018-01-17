// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_CAST_EXTENSION_HOST_DELEGATE_H_
#define CHROMECAST_BROWSER_EXTENSIONS_CAST_EXTENSION_HOST_DELEGATE_H_

#include "base/macros.h"
#include "extensions/browser/extension_host_delegate.h"

namespace extensions {

// A minimal ExtensionHostDelegate.
class CastExtensionHostDelegate : public ExtensionHostDelegate {
 public:
  CastExtensionHostDelegate();
  ~CastExtensionHostDelegate() override;

  // ExtensionHostDelegate implementation.
  void OnExtensionHostCreated(content::WebContents* web_contents) override;
  void OnRenderViewCreatedForBackgroundPage(ExtensionHost* host) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager() override;
  void CreateTab(content::WebContents* web_contents,
                 const std::string& extension_id,
                 WindowOpenDisposition disposition,
                 const gfx::Rect& initial_rect,
                 bool user_gesture) override;
  void ProcessMediaAccessRequest(content::WebContents* web_contents,
                                 const content::MediaStreamRequest& request,
                                 const content::MediaResponseCallback& callback,
                                 const Extension* extension) override;
  bool CheckMediaAccessPermission(content::WebContents* web_contents,
                                  const GURL& security_origin,
                                  content::MediaStreamType type,
                                  const Extension* extension) override;
  ExtensionHostQueue* GetExtensionHostQueue() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastExtensionHostDelegate);
};

}  // namespace extensions

#endif  // CHROMECAST_BROWSER_EXTENSIONS_CAST_EXTENSION_HOST_DELEGATE_H_
