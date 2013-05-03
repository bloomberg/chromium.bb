// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/validation_message_agent.h"

#include "base/i18n/rtl.h"
#include "chrome/common/validation_message_messages.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

ValidationMessageAgent::ValidationMessageAgent(content::RenderView* render_view)
    : content::RenderViewObserver(render_view)
{
#if !defined(OS_ANDROID)
  // TODO(tkent): enable this for Android. crbug.com/235721.
  render_view->GetWebView()->setValidationMessageClient(this);
#endif
}

ValidationMessageAgent::~ValidationMessageAgent() {}

void ValidationMessageAgent::showValidationMessage(
    const WebKit::WebRect& anchor_in_screen,
    const WebKit::WebString& main_text,
    const WebKit::WebString& sub_text,
    WebKit::WebTextDirection hint) {
  string16 wrapped_main_text = main_text;
  string16 wrapped_sub_text = sub_text;
  if (hint == WebKit::WebTextDirectionLeftToRight) {
    wrapped_main_text
        = base::i18n::GetDisplayStringInLTRDirectionality(wrapped_main_text);
    if (!wrapped_sub_text.empty()) {
      wrapped_sub_text
          = base::i18n::GetDisplayStringInLTRDirectionality(wrapped_sub_text);
    }
  } else if (hint == WebKit::WebTextDirectionRightToLeft
             && !base::i18n::IsRTL()) {
    base::i18n::WrapStringWithRTLFormatting(&wrapped_main_text);
    if (!wrapped_sub_text.empty()) {
      base::i18n::WrapStringWithRTLFormatting(&wrapped_sub_text);
    }
  }

  Send(new ValidationMessageMsg_ShowValidationMessage(
      routing_id(), anchor_in_screen, wrapped_main_text, wrapped_sub_text));
}

void ValidationMessageAgent::hideValidationMessage() {
  Send(new ValidationMessageMsg_HideValidationMessage());
}

