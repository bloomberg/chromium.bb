// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pnacl_translation_resource_host.h"

#include "components/nacl/common/nacl_host_messages.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/ppapi_globals.h"

using ppapi::PpapiGlobals;

PnaclTranslationResourceHost::PnaclTranslationResourceHost(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop)
    : io_message_loop_(io_message_loop), sender_(NULL) {}

PnaclTranslationResourceHost::~PnaclTranslationResourceHost() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  CleanupCacheRequests();
}

void PnaclTranslationResourceHost::OnFilterAdded(IPC::Sender* sender) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  sender_ = sender;
}

void PnaclTranslationResourceHost::OnFilterRemoved() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  sender_ = NULL;
}

void PnaclTranslationResourceHost::OnChannelClosing() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  sender_ = NULL;
}

bool PnaclTranslationResourceHost::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PnaclTranslationResourceHost, message)
    IPC_MESSAGE_HANDLER(NaClViewMsg_NexeTempFileReply, OnNexeTempFileReply)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PnaclTranslationResourceHost::RequestNexeFd(
    int render_view_id,
    PP_Instance instance,
    const nacl::PnaclCacheInfo& cache_info,
    RequestNexeFdCallback callback) {
  DCHECK(PpapiGlobals::Get()->
         GetMainThreadMessageLoop()->BelongsToCurrentThread());
  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&PnaclTranslationResourceHost::SendRequestNexeFd,
                 this,
                 render_view_id,
                 instance,
                 cache_info,
                 callback));
  return;
}

void PnaclTranslationResourceHost::SendRequestNexeFd(
    int render_view_id,
    PP_Instance instance,
    const nacl::PnaclCacheInfo& cache_info,
    RequestNexeFdCallback callback) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  if (!sender_ || !sender_->Send(new NaClHostMsg_NexeTempFileRequest(
          render_view_id, instance, cache_info))) {
    PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   static_cast<int32_t>(PP_ERROR_FAILED),
                   false,
                   PP_kInvalidFileHandle));
    return;
  }
  pending_cache_requests_.insert(std::make_pair(instance, callback));
}

void PnaclTranslationResourceHost::ReportTranslationFinished(
    PP_Instance instance,
    PP_Bool success) {
  DCHECK(PpapiGlobals::Get()->
         GetMainThreadMessageLoop()->BelongsToCurrentThread());
  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&PnaclTranslationResourceHost::SendReportTranslationFinished,
                 this,
                 instance,
                 success));
  return;
}

void PnaclTranslationResourceHost::SendReportTranslationFinished(
    PP_Instance instance,
    PP_Bool success) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  // If the sender is closed or we have been detached, we are probably shutting
  // down, so just don't send anything.
  if (!sender_)
    return;
  DCHECK(pending_cache_requests_.count(instance) == 0);
  sender_->Send(new NaClHostMsg_ReportTranslationFinished(instance,
                                                          PP_ToBool(success)));
}

void PnaclTranslationResourceHost::OnNexeTempFileReply(
    PP_Instance instance,
    bool is_hit,
    IPC::PlatformFileForTransit file) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  base::File base_file = IPC::PlatformFileForTransitToFile(file);
  CacheRequestInfoMap::iterator it = pending_cache_requests_.find(instance);
  int32_t status = PP_ERROR_FAILED;
  // Handle the expected successful case first.
  PP_FileHandle file_handle = PP_kInvalidFileHandle;
  if (it != pending_cache_requests_.end() && base_file.IsValid()) {
    file_handle = base_file.TakePlatformFile();
    status = PP_OK;
  }
  if (it == pending_cache_requests_.end()) {
    DLOG(ERROR) << "Could not find pending request for reply";
  } else {
    PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(it->second, status, is_hit, file_handle));
    pending_cache_requests_.erase(it);
  }
  if (!base_file.IsValid()) {
    DLOG(ERROR) << "Got invalid platformfilefortransit";
  }
}

void PnaclTranslationResourceHost::CleanupCacheRequests() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  for (CacheRequestInfoMap::iterator it = pending_cache_requests_.begin();
       it != pending_cache_requests_.end();
       ++it) {
    PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(it->second,
                   static_cast<int32_t>(PP_ERROR_ABORTED),
                   false,
                   PP_kInvalidFileHandle));
  }
  pending_cache_requests_.clear();
}
