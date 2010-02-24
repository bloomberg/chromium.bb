// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/cookie_message_filter.h"

#include "chrome/common/render_messages.h"

CookieMessageFilter::CookieMessageFilter()
    : event_(true, false) {
}

bool CookieMessageFilter::OnMessageReceived(const IPC::Message& message) {
  if (message.type() == ViewMsg_SignalCookiePromptEvent::ID) {
    event_.Signal();
    return true;
  }
  return false;
}
