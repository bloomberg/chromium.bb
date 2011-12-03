// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/text_input_client_observer.h"

#include "base/memory/scoped_ptr.h"
#include "content/common/text_input_client_messages.h"
#include "content/renderer/render_view_impl.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/mac/WebSubstringUtil.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/rect.h"

TextInputClientObserver::TextInputClientObserver(RenderViewImpl* render_view)
    : content::RenderViewObserver(render_view) {
}

TextInputClientObserver::~TextInputClientObserver() {
}

bool TextInputClientObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TextInputClientObserver, message)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_CharacterIndexForPoint,
                        OnCharacterIndexForPoint)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_FirstRectForCharacterRange,
                        OnFirstRectForCharacterRange)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_StringForRange, OnStringForRange)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

WebKit::WebView* TextInputClientObserver::webview() {
  return render_view()->GetWebView();
}

void TextInputClientObserver::OnCharacterIndexForPoint(gfx::Point point) {
  WebKit::WebPoint web_point(point);
  size_t index = webview()->focusedFrame()->characterIndexForPoint(web_point);
  Send(new TextInputClientReplyMsg_GotCharacterIndexForPoint(routing_id(),
      index));
}

void TextInputClientObserver::OnFirstRectForCharacterRange(ui::Range range) {
  WebKit::WebFrame* frame = webview()->focusedFrame();
  WebKit::WebRect web_rect;
  frame->firstRectForCharacterRange(range.start(), range.length(), web_rect);
  gfx::Rect rect(web_rect);
  Send(new TextInputClientReplyMsg_GotFirstRectForRange(routing_id(), rect));
}

void TextInputClientObserver::OnStringForRange(ui::Range range) {
#if defined(OS_MACOSX)
  NSAttributedString* string =
      WebKit::WebSubstringUtil::attributedSubstringInRange(
          webview()->focusedFrame(), range.start(), range.length());
  scoped_ptr<const mac::AttributedStringCoder::EncodedString> encoded(
      mac::AttributedStringCoder::Encode(string));
  Send(new TextInputClientReplyMsg_GotStringForRange(routing_id(),
      *encoded.get()));
#else
  NOTIMPLEMENTED();
#endif
}
