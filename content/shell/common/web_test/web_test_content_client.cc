// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/common/web_test/web_test_content_client.h"

#include "content/shell/common/shell_messages.h"

namespace content {

bool WebTestContentClient::CanSendWhileSwappedOut(const IPC::Message* message) {
  switch (message->type()) {
    // Used in web tests; handled in BlinkTestController.
    case ShellViewHostMsg_PrintMessage::ID:
      return true;

    default:
      return false;
  }
}

}  // namespace content
