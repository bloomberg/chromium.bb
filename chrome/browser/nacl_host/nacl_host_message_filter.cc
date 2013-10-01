// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_host_message_filter.h"

#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/nacl_host/nacl_browser.h"
#include "chrome/browser/nacl_host/nacl_file_host.h"
#include "chrome/browser/nacl_host/nacl_process_host.h"
#include "chrome/browser/nacl_host/pnacl_host.h"
#include "components/nacl/common/nacl_host_messages.h"
#include "extensions/common/constants.h"
#include "ipc/ipc_platform_file.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

static base::FilePath GetManifestPath(
    ExtensionInfoMap* extension_info_map, const std::string& manifest) {
  GURL manifest_url(manifest);
  const extensions::Extension* extension = extension_info_map->extensions()
      .GetExtensionOrAppByURL(manifest_url);
  if (extension != NULL &&
      manifest_url.SchemeIs(extensions::kExtensionScheme)) {
    std::string path = manifest_url.path();
    TrimString(path, "/", &path);  // Remove first slash
    return extension->path().AppendASCII(path);
  }
  return base::FilePath();
}

NaClHostMessageFilter::NaClHostMessageFilter(
    int render_process_id,
    bool is_off_the_record,
    const base::FilePath& profile_directory,
    ExtensionInfoMap* extension_info_map,
    net::URLRequestContextGetter* request_context)
    : render_process_id_(render_process_id),
      off_the_record_(is_off_the_record),
      profile_directory_(profile_directory),
      request_context_(request_context),
      extension_info_map_(extension_info_map),
      weak_ptr_factory_(this) {
}

NaClHostMessageFilter::~NaClHostMessageFilter() {
}

void NaClHostMessageFilter::OnChannelClosing() {
  PnaclHost::GetInstance()->RendererClosing(render_process_id_);
}

bool NaClHostMessageFilter::OnMessageReceived(const IPC::Message& message,
                                              bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(NaClHostMessageFilter, message, *message_was_ok)
#if !defined(DISABLE_NACL)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NaClHostMsg_LaunchNaCl, OnLaunchNaCl)
    IPC_MESSAGE_HANDLER(NaClHostMsg_EnsurePnaclInstalled,
                        OnEnsurePnaclInstalled)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NaClHostMsg_GetReadonlyPnaclFD,
                                    OnGetReadonlyPnaclFd)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NaClHostMsg_NaClCreateTemporaryFile,
                                    OnNaClCreateTemporaryFile)
    IPC_MESSAGE_HANDLER(NaClHostMsg_NexeTempFileRequest,
                        OnGetNexeFd)
    IPC_MESSAGE_HANDLER(NaClHostMsg_ReportTranslationFinished,
                        OnTranslationFinished)
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
      launch_params.enable_crash_throttling,
      off_the_record_,
      profile_directory_);
  base::FilePath manifest_url =
      GetManifestPath(extension_info_map_.get(), launch_params.manifest_url);
  host->Launch(this, reply_msg, manifest_url);
}

void NaClHostMessageFilter::ReplyEnsurePnaclInstalled(
    int instance,
    bool success) {
  Send(new NaClViewMsg_EnsurePnaclInstalledReply(instance, success));
}

void NaClHostMessageFilter::SendProgressEnsurePnaclInstalled(
    int instance,
    const nacl::PnaclInstallProgress& progress) {
    // TODO(jvoung): actually send an IPC.
}

void NaClHostMessageFilter::OnEnsurePnaclInstalled(
    int instance) {
  nacl_file_host::EnsurePnaclInstalled(
      base::Bind(&NaClHostMessageFilter::ReplyEnsurePnaclInstalled,
                 this, instance),
      base::Bind(&NaClHostMessageFilter::SendProgressEnsurePnaclInstalled,
                 this, instance));
}

void NaClHostMessageFilter::OnGetReadonlyPnaclFd(
    const std::string& filename, IPC::Message* reply_msg) {
  // This posts a task to another thread, but the renderer will
  // block until the reply is sent.
  nacl_file_host::GetReadonlyPnaclFd(this, filename, reply_msg);

  // This is the first message we receive from the renderer once it knows we
  // want to use PNaCl, so start the translation cache initialization here.
  PnaclHost::GetInstance()->Init();
}

// Return the temporary file via a reply to the
// NaClHostMsg_NaClCreateTemporaryFile sync message.
void NaClHostMessageFilter::SyncReturnTemporaryFile(
    IPC::Message* reply_msg,
    base::PlatformFile fd) {
  if (fd == base::kInvalidPlatformFileValue) {
    reply_msg->set_reply_error();
  } else {
    NaClHostMsg_NaClCreateTemporaryFile::WriteReplyParams(
        reply_msg,
        IPC::GetFileHandleForProcess(fd, PeerHandle(), true));
  }
  Send(reply_msg);
}

void NaClHostMessageFilter::OnNaClCreateTemporaryFile(
    IPC::Message* reply_msg) {
  PnaclHost::GetInstance()->CreateTemporaryFile(
      base::Bind(&NaClHostMessageFilter::SyncReturnTemporaryFile,
                 this,
                 reply_msg));
}

void NaClHostMessageFilter::AsyncReturnTemporaryFile(
    int pp_instance,
    base::PlatformFile fd,
    bool is_hit) {
  Send(new NaClViewMsg_NexeTempFileReply(
      pp_instance,
      is_hit,
      // Don't close our copy of the handle, because PnaclHost will use it
      // when the translation finishes.
      IPC::GetFileHandleForProcess(fd, PeerHandle(), false)));
}

void NaClHostMessageFilter::OnGetNexeFd(
    int render_view_id,
    int pp_instance,
    const nacl::PnaclCacheInfo& cache_info) {
  if (!cache_info.pexe_url.is_valid()) {
    LOG(ERROR) << "Bad URL received from GetNexeFd: " <<
        cache_info.pexe_url.possibly_invalid_spec();
    BadMessageReceived();
    return;
  }

  PnaclHost::GetInstance()->GetNexeFd(
      render_process_id_,
      render_view_id,
      pp_instance,
      off_the_record_,
      cache_info,
      base::Bind(&NaClHostMessageFilter::AsyncReturnTemporaryFile,
                 this,
                 pp_instance));
}

void NaClHostMessageFilter::OnTranslationFinished(int instance, bool success) {
  PnaclHost::GetInstance()->TranslationFinished(
      render_process_id_, instance, success);
}

void NaClHostMessageFilter::OnNaClErrorStatus(int render_view_id,
                                              int error_id) {
  NaClBrowser::GetDelegate()->ShowNaClInfobar(render_process_id_,
                                              render_view_id, error_id);
}

void NaClHostMessageFilter::OnOpenNaClExecutable(int render_view_id,
                                                 const GURL& file_url,
                                                 IPC::Message* reply_msg) {
  nacl_file_host::OpenNaClExecutable(this, extension_info_map_,
                                     render_view_id, file_url, reply_msg);
}
#endif
