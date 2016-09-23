// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/text_input_client_observer.h"

#include <stddef.h>

#include <memory>

#include "build/build_config.h"
#include "content/common/text_input_client_messages.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/mac/WebSubstringUtil.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

TextInputClientObserver::TextInputClientObserver(RenderWidget* render_widget)
    : render_widget_(render_widget) {}

TextInputClientObserver::~TextInputClientObserver() {
}

bool TextInputClientObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TextInputClientObserver, message)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_StringAtPoint,
                        OnStringAtPoint)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_CharacterIndexForPoint,
                        OnCharacterIndexForPoint)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_FirstRectForCharacterRange,
                        OnFirstRectForCharacterRange)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_StringForRange, OnStringForRange)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool TextInputClientObserver::Send(IPC::Message* message) {
  return render_widget_->Send(message);
}

blink::WebFrameWidget* TextInputClientObserver::GetWebFrameWidget() const {
  blink::WebWidget* widget = render_widget_->GetWebWidget();
  // We should always receive a WebFrameWidget in the call above. RenderViewImpl
  // however might return a WebView if the main frame is destroyed, but as long
  // as there is a rendered page, we should not be in that state and the RVImpl
  // should be returning a frame widget.
  DCHECK(widget->isWebFrameWidget());
  return static_cast<blink::WebFrameWidget*>(render_widget_->GetWebWidget());
}

blink::WebLocalFrame* TextInputClientObserver::GetFocusedFrame() const {
  blink::WebLocalFrame* focused =
      RenderFrameImpl::FromWebFrame(GetWebFrameWidget()->localRoot())
          ->render_view()
          ->webview()
          ->focusedFrame();
  return focused->localRoot() == GetWebFrameWidget()->localRoot() ? focused
                                                                  : nullptr;
}

#if defined(ENABLE_PLUGINS)
PepperPluginInstanceImpl* TextInputClientObserver::GetFocusedPepperPlugin()
    const {
  blink::WebLocalFrame* focusedFrame = GetFocusedFrame();
  return focusedFrame
             ? RenderFrameImpl::FromWebFrame(focusedFrame)
                   ->focused_pepper_plugin()
             : nullptr;
}
#endif

void TextInputClientObserver::OnStringAtPoint(gfx::Point point) {
#if defined(OS_MACOSX)
  blink::WebPoint baselinePoint;
  NSAttributedString* string = blink::WebSubstringUtil::attributedWordAtPoint(
      GetWebFrameWidget(), point, baselinePoint);

  std::unique_ptr<const mac::AttributedStringCoder::EncodedString> encoded(
      mac::AttributedStringCoder::Encode(string));
  Send(new TextInputClientReplyMsg_GotStringAtPoint(
      render_widget_->routing_id(), *encoded.get(), baselinePoint));
#else
  NOTIMPLEMENTED();
#endif
}

void TextInputClientObserver::OnCharacterIndexForPoint(gfx::Point point) {
  blink::WebPoint web_point(point);
  uint32_t index = static_cast<uint32_t>(
      GetFocusedFrame()->characterIndexForPoint(web_point));
  Send(new TextInputClientReplyMsg_GotCharacterIndexForPoint(
      render_widget_->routing_id(), index));
}

void TextInputClientObserver::OnFirstRectForCharacterRange(gfx::Range range) {
  gfx::Rect rect;
#if defined(ENABLE_PLUGINS)
  PepperPluginInstanceImpl* focused_plugin = GetFocusedPepperPlugin();
  if (focused_plugin) {
    rect = focused_plugin->GetCaretBounds();
  } else
#endif
  {
    blink::WebLocalFrame* frame = GetFocusedFrame();
    // TODO(yabinh): Null check should not be necessary.
    // See crbug.com/304341
    if (frame) {
      blink::WebRect web_rect;
      frame->firstRectForCharacterRange(range.start(), range.length(),
                                        web_rect);
      rect = web_rect;
    }
  }
  Send(new TextInputClientReplyMsg_GotFirstRectForRange(
      render_widget_->routing_id(), rect));
}

void TextInputClientObserver::OnStringForRange(gfx::Range range) {
#if defined(OS_MACOSX)
  blink::WebPoint baselinePoint;
  NSAttributedString* string = nil;
  blink::WebLocalFrame* frame = GetFocusedFrame();
  // TODO(yabinh): Null check should not be necessary.
  // See crbug.com/304341
  if (frame) {
    string = blink::WebSubstringUtil::attributedSubstringInRange(
        frame, range.start(), range.length(), &baselinePoint);
  }
  std::unique_ptr<const mac::AttributedStringCoder::EncodedString> encoded(
      mac::AttributedStringCoder::Encode(string));
  Send(new TextInputClientReplyMsg_GotStringForRange(
      render_widget_->routing_id(), *encoded.get(), baselinePoint));
#else
  NOTIMPLEMENTED();
#endif
}

}  // namespace content
