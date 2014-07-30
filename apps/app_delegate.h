// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_DELEGATE_H_
#define APPS_APP_DELEGATE_H_

#include "content/public/common/media_stream_request.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"

namespace content {
class BrowserContext;
class ColorChooser;
struct FileChooserParams;
struct OpenURLParams;
class WebContents;
}

namespace extensions {
class Extension;
}

namespace gfx {
class Rect;
}

namespace apps {

// Interface to give packaged apps access to services in the browser, for things
// like handling links and showing UI prompts to the user.
class AppDelegate {
 public:
  virtual ~AppDelegate() {}

  // General initialization.
  virtual void InitWebContents(content::WebContents* web_contents) = 0;

  // Link handling.
  virtual content::WebContents* OpenURLFromTab(
      content::BrowserContext* context,
      content::WebContents* source,
      const content::OpenURLParams& params) = 0;
  virtual void AddNewContents(content::BrowserContext* context,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) = 0;

  // Feature support.
  virtual content::ColorChooser* ShowColorChooser(
      content::WebContents* web_contents,
      SkColor initial_color) = 0;
  virtual void RunFileChooser(content::WebContents* tab,
                              const content::FileChooserParams& params) = 0;
  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback,
      const extensions::Extension* extension) = 0;
  virtual int PreferredIconSize() = 0;

  // Web contents modal dialog support.
  virtual void SetWebContentsBlocked(content::WebContents* web_contents,
                                     bool blocked) = 0;
  virtual bool IsWebContentsVisible(content::WebContents* web_contents) = 0;
};

}  // namespace apps

#endif  // APPS_APP_DELEGATE_H_
