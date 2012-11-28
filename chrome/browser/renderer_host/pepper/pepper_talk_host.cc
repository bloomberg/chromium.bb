// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/pepper_talk_host.h"

#include "base/bind.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "grit/generated_resources.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "ui/aura/window.h"
#endif

namespace chrome {

namespace {

ppapi::host::ReplyMessageContext GetPermissionOnUIThread(
    int render_process_id,
    int render_view_id,
    ppapi::host::ReplyMessageContext reply) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  reply.params.set_result(0);

  content::RenderViewHost* render_view_host =
      content::RenderViewHost::FromID(render_process_id, render_view_id);
  if (!render_view_host)
    return reply;  // RVH destroyed while task was pending.

#if defined(USE_ASH)
  // TODO(brettw). We should not be grabbing the active toplevel window, we
  // should use the toplevel window associated with the render view.
  const string16 title = l10n_util::GetStringUTF16(
      IDS_GTALK_SCREEN_SHARE_DIALOG_TITLE);
  const string16 message = l10n_util::GetStringUTF16(
      IDS_GTALK_SCREEN_SHARE_DIALOG_MESSAGE);

  aura::Window* parent = ash::Shell::GetContainer(
      ash::Shell::GetActiveRootWindow(),
      ash::internal::kShellWindowId_SystemModalContainer);
  reply.params.set_result(static_cast<int32_t>(
      chrome::ShowMessageBox(parent, title, message,
                             chrome::MESSAGE_BOX_TYPE_QUESTION) ==
      chrome::MESSAGE_BOX_RESULT_YES));
#else
  NOTIMPLEMENTED();
#endif
  return reply;
}

}  // namespace

PepperTalkHost::PepperTalkHost(content::BrowserPpapiHost* host,
                               PP_Instance instance,
                               PP_Resource resource)
    : ppapi::host::ResourceHost(host->GetPpapiHost(), instance, resource),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      browser_ppapi_host_(host) {
}

PepperTalkHost::~PepperTalkHost() {
}

int32_t PepperTalkHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  // We only have one message with no parameters.
  if (msg.type() != PpapiHostMsg_Talk_GetPermission::ID)
    return 0;

  int render_process_id = 0;
  int render_view_id = 0;
  browser_ppapi_host_->GetRenderViewIDsForInstance(
      pp_instance(), &render_process_id, &render_view_id);

  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&GetPermissionOnUIThread, render_process_id, render_view_id,
                 context->MakeReplyMessageContext()),
      base::Bind(&PepperTalkHost::GotTalkPermission,
                 weak_factory_.GetWeakPtr()));
  return PP_OK_COMPLETIONPENDING;
}

void PepperTalkHost::GotTalkPermission(
    ppapi::host::ReplyMessageContext reply) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  host()->SendReply(reply, PpapiPluginMsg_Talk_GetPermissionReply());
}

}  // namespace chrome
