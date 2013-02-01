// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ONE_CLICK_SIGNIN_AGENT_H_
#define CHROME_RENDERER_ONE_CLICK_SIGNIN_AGENT_H_

#include "content/public/renderer/render_view_observer.h"

// OneClickSigninAgent deals with oneclick signin related communications between
// the renderer and the browser.  There is one OneClickSigninAgent per
// RenderView.
class OneClickSigninAgent : public content::RenderViewObserver {
  public:
    explicit OneClickSigninAgent(content::RenderView* render_view);
    virtual ~OneClickSigninAgent();

  private:
    virtual void WillSubmitForm(WebKit::WebFrame* frame,
                                const WebKit::WebFormElement& form) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninAgent);
};

#endif  // CHROME_RENDERER_ONE_CLICK_SIGNIN_AGENT_H_
