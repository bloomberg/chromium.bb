// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/pepper_talk_host.h"

#include "base/bind.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "ui/aura/window.h"
#endif

namespace chrome {

namespace {

ppapi::host::ReplyMessageContext GetPermissionOnUIThread(
    PP_TalkPermission permission,
    int render_process_id,
    int render_frame_id,
    ppapi::host::ReplyMessageContext reply) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  reply.params.set_result(0);

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return reply;  // RFH destroyed while task was pending.

  // crbug.com/381398, crbug.com/413906 for !USE_ATHENA
#if defined(USE_ASH) && !defined(USE_ATHENA)
  base::string16 title;
  base::string16 message;

  switch (permission) {
    case PP_TALKPERMISSION_SCREENCAST:
      title = l10n_util::GetStringUTF16(IDS_GTALK_SCREEN_SHARE_DIALOG_TITLE);
      message =
          l10n_util::GetStringUTF16(IDS_GTALK_SCREEN_SHARE_DIALOG_MESSAGE);
      break;
    case PP_TALKPERMISSION_REMOTING:
      title = l10n_util::GetStringUTF16(IDS_GTALK_REMOTING_DIALOG_TITLE);
      message = l10n_util::GetStringUTF16(IDS_GTALK_REMOTING_DIALOG_MESSAGE);
      break;
    case PP_TALKPERMISSION_REMOTING_CONTINUE:
      title = l10n_util::GetStringUTF16(IDS_GTALK_REMOTING_DIALOG_TITLE);
      message =
          l10n_util::GetStringUTF16(IDS_GTALK_REMOTING_CONTINUE_DIALOG_MESSAGE);
      break;
    default:
      NOTREACHED();
      return reply;
  }

  // TODO(brettw). We should not be grabbing the active toplevel window, we
  // should use the toplevel window associated with the render view.
  aura::Window* parent =
      ash::Shell::GetContainer(ash::Shell::GetTargetRootWindow(),
                               ash::kShellWindowId_SystemModalContainer);
  reply.params.set_result(static_cast<int32_t>(
      chrome::ShowMessageBox(
          parent, title, message, chrome::MESSAGE_BOX_TYPE_QUESTION) ==
      chrome::MESSAGE_BOX_RESULT_YES));
#else
  NOTIMPLEMENTED();
#endif
  return reply;
}

#if defined(USE_ASH) && defined(OS_CHROMEOS) && !defined(USE_ATHENA)
void OnTerminateRemotingEventOnUIThread(const base::Closure& stop_callback) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE, stop_callback);
}
#endif  // defined(USE_ASH) && defined(OS_CHROMEOS)

ppapi::host::ReplyMessageContext StartRemotingOnUIThread(
    const base::Closure& stop_callback,
    int render_process_id,
    int render_frame_id,
    ppapi::host::ReplyMessageContext reply) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host) {
    reply.params.set_result(PP_ERROR_FAILED);
    return reply;  // RFH destroyed while task was pending.
  }

#if defined(USE_ASH) && defined(OS_CHROMEOS) && !defined(USE_ATHENA)
  base::Closure stop_callback_ui_thread =
      base::Bind(&OnTerminateRemotingEventOnUIThread, stop_callback);

  ash::Shell::GetInstance()->system_tray_notifier()->NotifyScreenShareStart(
      stop_callback_ui_thread, base::string16());
  reply.params.set_result(PP_OK);
#else
  NOTIMPLEMENTED();
  reply.params.set_result(PP_ERROR_NOTSUPPORTED);
#endif
  return reply;
}

void StopRemotingOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
#if defined(USE_ASH) && defined(OS_CHROMEOS) && !defined(USE_ATHENA)
  if (ash::Shell::GetInstance()) {
    ash::Shell::GetInstance()->system_tray_notifier()->NotifyScreenShareStop();
  }
#else
  NOTIMPLEMENTED();
#endif
}

ppapi::host::ReplyMessageContext StopRemotingOnUIThreadWithResult(
    ppapi::host::ReplyMessageContext reply) {
  reply.params.set_result(PP_OK);
  StopRemotingOnUIThread();
  return reply;
}

}  // namespace

PepperTalkHost::PepperTalkHost(content::BrowserPpapiHost* host,
                               PP_Instance instance,
                               PP_Resource resource)
    : ppapi::host::ResourceHost(host->GetPpapiHost(), instance, resource),
      browser_ppapi_host_(host),
      remoting_started_(false),
      weak_factory_(this) {}

PepperTalkHost::~PepperTalkHost() {
  if (remoting_started_) {
    content::BrowserThread::PostTask(content::BrowserThread::UI,
                                     FROM_HERE,
                                     base::Bind(&StopRemotingOnUIThread));
  }
}

int32_t PepperTalkHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperTalkHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_Talk_RequestPermission,
                                      OnRequestPermission)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_Talk_StartRemoting,
                                        OnStartRemoting)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_Talk_StopRemoting,
                                        OnStopRemoting)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperTalkHost::OnRequestPermission(
    ppapi::host::HostMessageContext* context,
    PP_TalkPermission permission) {
  if (permission < PP_TALKPERMISSION_SCREENCAST ||
      permission >= PP_TALKPERMISSION_NUM_PERMISSIONS)
    return PP_ERROR_BADARGUMENT;

  int render_process_id = 0;
  int render_frame_id = 0;
  browser_ppapi_host_->GetRenderFrameIDsForInstance(
      pp_instance(), &render_process_id, &render_frame_id);

  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GetPermissionOnUIThread,
                 permission,
                 render_process_id,
                 render_frame_id,
                 context->MakeReplyMessageContext()),
      base::Bind(&PepperTalkHost::OnRequestPermissionCompleted,
                 weak_factory_.GetWeakPtr()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTalkHost::OnStartRemoting(
    ppapi::host::HostMessageContext* context) {
  int render_process_id = 0;
  int render_frame_id = 0;
  browser_ppapi_host_->GetRenderFrameIDsForInstance(
      pp_instance(), &render_process_id, &render_frame_id);

  base::Closure remoting_stop_callback = base::Bind(
      &PepperTalkHost::OnRemotingStopEvent, weak_factory_.GetWeakPtr());

  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&StartRemotingOnUIThread,
                 remoting_stop_callback,
                 render_process_id,
                 render_frame_id,
                 context->MakeReplyMessageContext()),
      base::Bind(&PepperTalkHost::OnStartRemotingCompleted,
                 weak_factory_.GetWeakPtr()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperTalkHost::OnStopRemoting(
    ppapi::host::HostMessageContext* context) {
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&StopRemotingOnUIThreadWithResult,
                 context->MakeReplyMessageContext()),
      base::Bind(&PepperTalkHost::OnStopRemotingCompleted,
                 weak_factory_.GetWeakPtr()));
  return PP_OK_COMPLETIONPENDING;
}

void PepperTalkHost::OnRemotingStopEvent() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  remoting_started_ = false;
  host()->SendUnsolicitedReply(
      pp_resource(), PpapiPluginMsg_Talk_NotifyEvent(PP_TALKEVENT_TERMINATE));
}

void PepperTalkHost::OnRequestPermissionCompleted(
    ppapi::host::ReplyMessageContext reply) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  host()->SendReply(reply, PpapiPluginMsg_Talk_RequestPermissionReply());
}

void PepperTalkHost::OnStartRemotingCompleted(
    ppapi::host::ReplyMessageContext reply) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  // Remember to hide remoting UI when resource is deleted.
  if (reply.params.result() == PP_OK)
    remoting_started_ = true;

  host()->SendReply(reply, PpapiPluginMsg_Talk_StartRemotingReply());
}

void PepperTalkHost::OnStopRemotingCompleted(
    ppapi::host::ReplyMessageContext reply) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  remoting_started_ = false;
  host()->SendReply(reply, PpapiPluginMsg_Talk_StopRemotingReply());
}

}  // namespace chrome
