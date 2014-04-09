// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_PLATFORM_APP_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_PLATFORM_APP_H_

#include "chrome/browser/renderer_context_menu/context_menu_content_type.h"

class ContextMenuContentTypePlatformApp : public ContextMenuContentType {
 public:
  virtual ~ContextMenuContentTypePlatformApp();

  // ContextMenuContentType overrides.
  virtual bool SupportsGroup(int group) OVERRIDE;

 protected:
  ContextMenuContentTypePlatformApp(content::WebContents* web_contents,
                                    const content::ContextMenuParams& params);

 private:
  friend class ContextMenuContentTypeFactory;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuContentTypePlatformApp);
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_CONTENT_TYPE_PLATFORM_APP_H_
