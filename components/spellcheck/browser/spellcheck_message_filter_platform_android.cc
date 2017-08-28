// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spellcheck_message_filter_platform.h"

#include "components/spellcheck/browser/spellchecker_session_bridge_android.h"
#include "components/spellcheck/common/spellcheck_messages.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

SpellCheckMessageFilterPlatform::SpellCheckMessageFilterPlatform(
    int render_process_id)
    : BrowserMessageFilter(SpellCheckMsgStart),
      render_process_id_(render_process_id),
      impl_(new SpellCheckerSessionBridge(render_process_id)) {}

void SpellCheckMessageFilterPlatform::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  switch (message.type()) {
    case SpellCheckHostMsg_RequestTextCheck::ID:
    case SpellCheckHostMsg_ToggleSpellCheck::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      break;
  }
}

bool SpellCheckMessageFilterPlatform::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SpellCheckMessageFilterPlatform, message)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_RequestTextCheck, OnRequestTextCheck)
    IPC_MESSAGE_HANDLER(SpellCheckHostMsg_ToggleSpellCheck, OnToggleSpellCheck)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

SpellCheckMessageFilterPlatform::~SpellCheckMessageFilterPlatform() {}

void SpellCheckMessageFilterPlatform::OnRequestTextCheck(
    int route_id,
    int identifier,
    const base::string16& text) {
  DCHECK(!text.empty());
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  impl_->RequestTextCheck(route_id, identifier, text);
}

void SpellCheckMessageFilterPlatform::OnToggleSpellCheck(
    bool enabled,
    bool checked) {
  if (!enabled)
    impl_->DisconnectSession();
}

void SpellCheckMessageFilterPlatform::OnDestruct() const {
  // Needs to be destroyed on the UI thread, to avoid race conditions
  // on the java side during clean-up.
  BrowserThread::DeleteOnUIThread::Destruct(this);
}
