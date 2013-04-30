// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_VALIDATION_MESSAGE_AGENT_H_
#define CHROME_RENDERER_VALIDATION_MESSAGE_AGENT_H_

#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebValidationMessageClient.h"

namespace content {
class RenderView;
}

// An impelemntation of WebKit::WebValidationMessageClient. This dispatches
// messages to the browser processes.
class ValidationMessageAgent : public content::RenderViewObserver,
                               public WebKit::WebValidationMessageClient {
 public:
  explicit ValidationMessageAgent(content::RenderView* render_view);
  virtual ~ValidationMessageAgent();

 private:
  // WebValidationMessageClient functions:
  virtual void showValidationMessage(const WebKit::WebRect& anchor_in_screen,
                                     const WebKit::WebString& main_text,
                                     const WebKit::WebString& sub_text,
                                     WebKit::WebTextDirection hint) OVERRIDE;
  virtual void hideValidationMessage() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ValidationMessageAgent);
};

#endif  // CHROME_RENDERER_VALIDATION_MESSAGE_AGENT_H_
