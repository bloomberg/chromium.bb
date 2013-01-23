// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_WEBRTC_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_WEBRTC_INTERNALS_MESSAGE_HANDLER_H_

#include "base/memory/ref_counted.h"
#include "chrome/browser/media/webrtc_internals_ui_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

// This class handles messages to and from WebRTCInternalsUI.
// It delegates all its work to WebRTCInternalsProxy on the IO thread.
class WebRTCInternalsMessageHandler : public content::WebUIMessageHandler,
                                      public WebRTCInternalsUIObserver{
 public:
  WebRTCInternalsMessageHandler();
  virtual ~WebRTCInternalsMessageHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // WebRTCInternalsUIObserver override.
  virtual void OnUpdate(const std::string& command,
                        const base::Value* args) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRTCInternalsMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_WEBRTC_INTERNALS_MESSAGE_HANDLER_H_
