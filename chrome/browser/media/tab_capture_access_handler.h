// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_TAB_CAPTURE_ACCESS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_TAB_CAPTURE_ACCESS_HANDLER_H_

#include "chrome/browser/media/media_access_handler.h"

class MediaStreamCaptureIndicator;

// MediaAccessHandler for TabCapture API.
class TabCaptureAccessHandler : public MediaAccessHandler {
 public:
  TabCaptureAccessHandler();
  ~TabCaptureAccessHandler() override;

  // MediaAccessHandler implementation.
  bool SupportsStreamType(const content::MediaStreamType type,
                          const extensions::Extension* extension) override;
  bool CheckMediaAccessPermission(
      content::WebContents* web_contents,
      const GURL& security_origin,
      content::MediaStreamType type,
      const extensions::Extension* extension) override;
  void HandleRequest(content::WebContents* web_contents,
                     const content::MediaStreamRequest& request,
                     const content::MediaResponseCallback& callback,
                     const extensions::Extension* extension) override;
};

#endif  // CHROME_BROWSER_MEDIA_TAB_CAPTURE_ACCESS_HANDLER_H_
