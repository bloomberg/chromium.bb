// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_host_message_filter.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/nacl_host/nacl_file_host.h"
#include "chrome/browser/nacl_host/nacl_infobar.h"
#include "chrome/browser/nacl_host/nacl_process_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/nacl_host_messages.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

NaClHostMessageFilter::NaClHostMessageFilter(
    int render_process_id,
    Profile* profile,
    net::URLRequestContextGetter* request_context)
    : render_process_id_(render_process_id),
      profile_(profile),
      off_the_record_(profile_->IsOffTheRecord()),
      request_context_(request_context),
      extension_info_map_(
          extensions::ExtensionSystem::Get(profile)->info_map()),
      weak_ptr_factory_(this) {
}

NaClHostMessageFilter::~NaClHostMessageFilter() {
}

bool NaClHostMessageFilter::OnMessageReceived(const IPC::Message& message,
                                              bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(NaClHostMessageFilter, message, *message_was_ok)
#if !defined(DISABLE_NACL)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NaClHostMsg_LaunchNaCl, OnLaunchNaCl)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NaClHostMsg_GetReadonlyPnaclFD,
                                    OnGetReadonlyPnaclFd)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NaClHostMsg_NaClCreateTemporaryFile,
                                    OnNaClCreateTemporaryFile)
    IPC_MESSAGE_HANDLER(NaClHostMsg_NaClErrorStatus, OnNaClErrorStatus)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NaClHostMsg_OpenNaClExecutable,
                                    OnOpenNaClExecutable)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

net::HostResolver* NaClHostMessageFilter::GetHostResolver() {
  return request_context_->GetURLRequestContext()->host_resolver();
}

#if !defined(DISABLE_NACL)
void NaClHostMessageFilter::OnLaunchNaCl(
    const nacl::NaClLaunchParams& launch_params,
    IPC::Message* reply_msg) {
  NaClProcessHost* host = new NaClProcessHost(
      GURL(launch_params.manifest_url),
      launch_params.render_view_id,
      launch_params.permission_bits,
      launch_params.uses_irt,
      launch_params.enable_dyncode_syscalls,
      launch_params.enable_exception_handling,
      off_the_record_,
      profile_->GetPath());
  host->Launch(this, reply_msg, extension_info_map_);
}

void NaClHostMessageFilter::OnGetReadonlyPnaclFd(
    const std::string& filename, IPC::Message* reply_msg) {
  // This posts a task to another thread, but the renderer will
  // block until the reply is sent.
  nacl_file_host::GetReadonlyPnaclFd(this, filename, reply_msg);
}

void NaClHostMessageFilter::OnNaClCreateTemporaryFile(
    IPC::Message* reply_msg) {
  nacl_file_host::CreateTemporaryFile(this, reply_msg);
}

void NaClHostMessageFilter::OnNaClErrorStatus(int render_view_id,
                                              int error_id) {
  // Currently there is only one kind of error status, for which
  // we want to show the user an infobar.
  ShowNaClInfobar(render_process_id_, render_view_id, error_id);
}

void NaClHostMessageFilter::OnOpenNaClExecutable(int render_view_id,
                                                 const GURL& file_url,
                                                 IPC::Message* reply_msg) {
  nacl_file_host::OpenNaClExecutable(this, extension_info_map_,
                                     render_view_id, file_url, reply_msg);
}
#endif
