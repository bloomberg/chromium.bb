// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellcheck_message_filter.h"

#include "chrome/browser/spellchecker_platform_engine.h"
#include "chrome/common/render_messages.h"

SpellCheckMessageFilter::SpellCheckMessageFilter() {
}

SpellCheckMessageFilter::~SpellCheckMessageFilter() {
}

bool SpellCheckMessageFilter::OnMessageReceived(const IPC::Message& message,
                                          bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(SpellCheckMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SpellChecker_PlatformRequestTextCheck,
                        OnPlatformRequestTextCheck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SpellCheckMessageFilter::OnPlatformRequestTextCheck(
    int route_id,
    int identifier,
    int document_tag,
    const string16& text) {
  SpellCheckerPlatform::RequestTextCheck(
      route_id, identifier, document_tag, text, this);
}
