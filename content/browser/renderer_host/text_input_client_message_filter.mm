// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/text_input_client_message_filter.h"

#include "base/memory/scoped_nsobject.h"
#include "base/string16.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/text_input_client_mac.h"
#include "content/common/text_input_client_messages.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ipc/ipc_message_macros.h"
#include "ui/base/range/range.h"
#include "ui/gfx/rect.h"

namespace content {

TextInputClientMessageFilter::TextInputClientMessageFilter(int child_id)
    : BrowserMessageFilter(),
      child_process_id_(child_id) {
}

bool TextInputClientMessageFilter::OnMessageReceived(
    const IPC::Message& message,
    bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(TextInputClientMessageFilter, message,
      *message_was_ok)
    IPC_MESSAGE_HANDLER(TextInputClientReplyMsg_GotCharacterIndexForPoint,
                        OnGotCharacterIndexForPoint)
    IPC_MESSAGE_HANDLER(TextInputClientReplyMsg_GotFirstRectForRange,
                        OnGotFirstRectForRange)
    IPC_MESSAGE_HANDLER(TextInputClientReplyMsg_GotStringForRange,
                        OnGotStringFromRange)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

TextInputClientMessageFilter::~TextInputClientMessageFilter() {}

void TextInputClientMessageFilter::OnGotCharacterIndexForPoint(size_t index) {
  TextInputClientMac* service = TextInputClientMac::GetInstance();
  // |index| could be WTF::notFound (-1) and its value is different from
  // NSNotFound so we need to convert it.
  if (index == static_cast<size_t>(-1)) {
    index = NSNotFound;
  }
  service->SetCharacterIndexAndSignal(index);
}

void TextInputClientMessageFilter::OnGotFirstRectForRange(
    const gfx::Rect& rect) {
  TextInputClientMac* service = TextInputClientMac::GetInstance();
  service->SetFirstRectAndSignal(NSRectFromCGRect(rect.ToCGRect()));
}

void TextInputClientMessageFilter::OnGotStringFromRange(
    const mac::AttributedStringCoder::EncodedString& encoded_string) {
  TextInputClientMac* service = TextInputClientMac::GetInstance();
  NSAttributedString* string =
      mac::AttributedStringCoder::Decode(&encoded_string);
  if (![string length])
    string = nil;
  service->SetSubstringAndSignal(string);
}

}  // namespace content
