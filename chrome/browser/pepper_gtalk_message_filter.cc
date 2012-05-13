// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pepper_gtalk_message_filter.h"

#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/api_id.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "ui/aura/window.h"
#endif

PepperGtalkMessageFilter::PepperGtalkMessageFilter() {}

void PepperGtalkMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    content::BrowserThread::ID* thread) {
  if (message.type() == PpapiHostMsg_PPBTalk_GetPermission::ID) {
    *thread = content::BrowserThread::UI;
  }
}

bool PepperGtalkMessageFilter::OnMessageReceived(const IPC::Message& msg,
                                                 bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(PepperGtalkMessageFilter, msg, *message_was_ok)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTalk_GetPermission, OnTalkGetPermission)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

PepperGtalkMessageFilter::~PepperGtalkMessageFilter() {}

void PepperGtalkMessageFilter::OnTalkGetPermission(uint32 plugin_dispatcher_id,
                                                   PP_Resource resource) {
  bool user_response = false;
#if defined(USE_ASH)
  const string16 title = l10n_util::GetStringUTF16(
      IDS_GTALK_SCREEN_SHARE_DIALOG_TITLE);
  const string16 message = l10n_util::GetStringUTF16(
      IDS_GTALK_SCREEN_SHARE_DIALOG_MESSAGE);

  aura::Window* parent = ash::Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_SystemModalContainer);
  user_response = browser::ShowQuestionMessageBox(parent, title, message);
#else
  NOTIMPLEMENTED();
#endif
  Send(new PpapiMsg_PPBTalk_GetPermissionACK(ppapi::API_ID_PPB_TALK,
                                             plugin_dispatcher_id,
                                             resource,
                                             user_response));
}
