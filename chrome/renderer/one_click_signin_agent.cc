// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/one_click_signin_agent.h"

#include "chrome/common/one_click_signin_messages.h"
#include "content/public/common/password_form.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

OneClickSigninAgent::OneClickSigninAgent(
    content::RenderView* render_view)
    : content::RenderViewObserver(render_view) {
}

OneClickSigninAgent::~OneClickSigninAgent() {}

void OneClickSigninAgent::WillSubmitForm(WebKit::WebFrame* frame,
                                         const WebKit::WebFormElement& form) {
  content::DocumentState* document_state =
      content::DocumentState::FromDataSource(frame->provisionalDataSource());
  if (document_state->password_form_data()) {
    Send(new OneClickSigninHostMsg_FormSubmitted(routing_id(),
         *(document_state->password_form_data())));
  }
}
