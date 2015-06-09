// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_DESKTOP_CAPTURE_ACCESS_HANDLER_H_
#define CHROME_BROWSER_MEDIA_DESKTOP_CAPTURE_ACCESS_HANDLER_H_

#include <list>

#include "chrome/browser/media/media_access_handler.h"

class DesktopStreamsRegistry;

// MediaAccessHandler for DesktopCapture API.
class DesktopCaptureAccessHandler : public MediaAccessHandler {
 public:
  DesktopCaptureAccessHandler();
  ~DesktopCaptureAccessHandler() override;

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
  void UpdateMediaRequestState(int render_process_id,
                               int render_frame_id,
                               int page_request_id,
                               content::MediaStreamType stream_type,
                               content::MediaRequestState state) override;

  bool IsCaptureInProgress();

 private:
  // Tracks MEDIA_DESKTOP_VIDEO_CAPTURE sessions which reach the
  // MEDIA_REQUEST_STATE_DONE state.  Sessions are remove when
  // MEDIA_REQUEST_STATE_CLOSING is encountered.
  struct DesktopCaptureSession {
    int render_process_id;
    int render_frame_id;
    int page_request_id;
  };
  typedef std::list<DesktopCaptureSession> DesktopCaptureSessions;

  void ProcessScreenCaptureAccessRequest(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback,
      const extensions::Extension* extension);

  DesktopCaptureSessions desktop_capture_sessions_;
};

#endif  // CHROME_BROWSER_MEDIA_DESKTOP_CAPTURE_ACCESS_HANDLER_H_
