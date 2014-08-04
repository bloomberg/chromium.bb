// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_WEB_CONTENTS_HELPER_H_
#define APPS_APP_WEB_CONTENTS_HELPER_H_

#include "content/public/common/console_message_level.h"
#include "content/public/common/media_stream_request.h"

namespace blink {
class WebGestureEvent;
}

namespace content {
class BrowserContext;
struct OpenURLParams;
class WebContents;
}

namespace extensions {
class Extension;
}

namespace apps {

class AppDelegate;

// Provides common functionality for apps and launcher pages to respond to
// messages from a WebContents.
class AppWebContentsHelper {
 public:
  AppWebContentsHelper(content::BrowserContext* browser_context,
                       const std::string& extension_id,
                       content::WebContents* web_contents,
                       AppDelegate* app_delegate);

  // Returns true if the given |event| should not be handled by the renderer.
  static bool ShouldSuppressGestureEvent(const blink::WebGestureEvent& event);

  // Opens a new URL inside the passed in WebContents. See WebContentsDelegate.
  content::WebContents* OpenURLFromTab(
      const content::OpenURLParams& params) const;

  // Requests to lock the mouse. See WebContentsDelegate.
  void RequestToLockMouse() const;

  // Asks permission to use the camera and/or microphone. See
  // WebContentsDelegate.
  void RequestMediaAccessPermission(
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) const;

 private:
  const extensions::Extension* GetExtension() const;

  // Helper method to add a message to the renderer's DevTools console.
  void AddMessageToDevToolsConsole(content::ConsoleMessageLevel level,
                                   const std::string& message) const;

  // The browser context with which this window is associated.
  // AppWindowWebContentsDelegate does not own this object.
  content::BrowserContext* browser_context_;

  const std::string extension_id_;

  content::WebContents* web_contents_;

  AppDelegate* app_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppWebContentsHelper);
};

}  // namespace apps

#endif  // APPS_APP_WEB_CONTENTS_HELPER_H_
