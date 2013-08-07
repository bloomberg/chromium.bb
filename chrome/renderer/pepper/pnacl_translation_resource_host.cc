// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pnacl_translation_resource_host.h"

#ifndef DISABLE_NACL
#include "components/nacl/common/nacl_host_messages.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/ppapi_globals.h"

using ppapi::TrackedCallback;
using ppapi::PpapiGlobals;

PnaclTranslationResourceHost::CacheRequestInfo::CacheRequestInfo(
    PP_Bool* hit,
    PP_FileHandle* handle,
    scoped_refptr<TrackedCallback> cb)
    : is_hit(hit), file_handle(handle), callback(cb) {}

PnaclTranslationResourceHost::CacheRequestInfo::~CacheRequestInfo() {}

PnaclTranslationResourceHost::PnaclTranslationResourceHost(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop)
    : io_message_loop_(io_message_loop), channel_(NULL) {}

PnaclTranslationResourceHost::~PnaclTranslationResourceHost() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  CleanupCacheRequests();
  CleanupEnsurePnaclRequests();
}

void PnaclTranslationResourceHost::OnFilterAdded(IPC::Channel* channel) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  channel_ = channel;
}

void PnaclTranslationResourceHost::OnFilterRemoved() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  channel_ = NULL;
}

void PnaclTranslationResourceHost::OnChannelClosing() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  channel_ = NULL;
}

bool PnaclTranslationResourceHost::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PnaclTranslationResourceHost, message)
    IPC_MESSAGE_HANDLER(NaClViewMsg_NexeTempFileReply, OnNexeTempFileReply)
    IPC_MESSAGE_HANDLER(NaClViewMsg_EnsurePnaclInstalledReply,
                        OnEnsurePnaclInstalledReply)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PnaclTranslationResourceHost::RequestNexeFd(
    int render_view_id,
    PP_Instance instance,
    const nacl::PnaclCacheInfo& cache_info,
    PP_Bool* is_hit,
    PP_FileHandle* file_handle,
    scoped_refptr<TrackedCallback> callback) {
  DCHECK(PpapiGlobals::Get()->
         GetMainThreadMessageLoop()->BelongsToCurrentThread());
  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&PnaclTranslationResourceHost::SendRequestNexeFd,
                 this,
                 render_view_id,
                 instance,
                 cache_info,
                 is_hit,
                 file_handle,
                 callback));
  return;
}

void PnaclTranslationResourceHost::SendRequestNexeFd(
    int render_view_id,
    PP_Instance instance,
    const nacl::PnaclCacheInfo& cache_info,
    PP_Bool* is_hit,
    PP_FileHandle* file_handle,
    scoped_refptr<TrackedCallback> callback) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  if (!channel_ || !channel_->Send(new NaClHostMsg_NexeTempFileRequest(
                       render_view_id, instance, cache_info))) {
    PpapiGlobals::Get()->GetMainThreadMessageLoop()
        ->PostTask(FROM_HERE,
                   base::Bind(&TrackedCallback::Run,
                              callback,
                              static_cast<int32_t>(PP_ERROR_FAILED)));
    return;
  }
  pending_cache_requests_.insert(std::make_pair(
      instance, CacheRequestInfo(is_hit, file_handle, callback)));
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
  // If the channel is closed or we have been detached, we are probably shutting
  // down, so just don't send anything.
  if (!channel_)
    return;
  DCHECK(pending_cache_requests_.count(instance) == 0);
  channel_->Send(new NaClHostMsg_ReportTranslationFinished(instance,
                                                           PP_ToBool(success)));
}

void PnaclTranslationResourceHost::OnNexeTempFileReply(
    PP_Instance instance,
    bool is_hit,
    IPC::PlatformFileForTransit file) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  CacheRequestInfoMap::iterator it = pending_cache_requests_.find(instance);
  int32_t status = PP_ERROR_FAILED;
  // Handle the expected successful case first.
  if (it != pending_cache_requests_.end() &&
      !(file == IPC::InvalidPlatformFileForTransit()) &&
      TrackedCallback::IsPending(it->second.callback)) {
    *it->second.is_hit = PP_FromBool(is_hit);
    *it->second.file_handle = IPC::PlatformFileForTransitToPlatformFile(file);
    status = PP_OK;
  }
  if (it == pending_cache_requests_.end()) {
    DLOG(ERROR) << "Could not find pending request for reply";
  } else {
    PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(&TrackedCallback::Run, it->second.callback, status));
    pending_cache_requests_.erase(it);
  }
  if (file == IPC::InvalidPlatformFileForTransit()) {
    DLOG(ERROR) << "Got invalid platformfilefortransit";
  } else if (status != PP_OK) {
    base::ClosePlatformFile(IPC::PlatformFileForTransitToPlatformFile(file));
  }
}

void PnaclTranslationResourceHost::CleanupCacheRequests() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  for (CacheRequestInfoMap::iterator it = pending_cache_requests_.begin();
       it != pending_cache_requests_.end();
       ++it) {
    it->second.callback->PostAbort();
  }
  pending_cache_requests_.clear();
}

void PnaclTranslationResourceHost::EnsurePnaclInstalled(
    PP_Instance instance,
    scoped_refptr<TrackedCallback> callback) {
  DCHECK(PpapiGlobals::Get()->
         GetMainThreadMessageLoop()->BelongsToCurrentThread());
  io_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&PnaclTranslationResourceHost::SendEnsurePnaclInstalled,
                 this, instance, callback));
  return;
}

void PnaclTranslationResourceHost::SendEnsurePnaclInstalled(
    PP_Instance instance,
    scoped_refptr<TrackedCallback> callback) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  // If a request is already in, just queue this one and wait for notification.
  // Hope that the request is not canceled.
  if (pending_ensure_pnacl_requests_.size() > 0) {
    pending_ensure_pnacl_requests_.push_back(callback);
    return;
  }
  // Otherwise, try to send the request, then queue.
  if (!channel_ || !channel_->Send(new NaClHostMsg_EnsurePnaclInstalled(
          instance))) {
    PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(&TrackedCallback::Run,
                   callback,
                   static_cast<int32_t>(PP_ERROR_FAILED)));
    return;
  }
  pending_ensure_pnacl_requests_.push_back(callback);
}

void PnaclTranslationResourceHost::OnEnsurePnaclInstalledReply(
    PP_Instance instance,
    bool success) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  // Broadcast to all listeners.
  for (EnsurePnaclInstalledList::iterator
           i = pending_ensure_pnacl_requests_.begin(),
           e = pending_ensure_pnacl_requests_.end();
       i != e; ++i) {
    if (TrackedCallback::IsPending(*i)) {
      PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
          FROM_HERE,
          base::Bind(&TrackedCallback::Run,
                     *i,
                     static_cast<int32_t>(success ? PP_OK : PP_ERROR_FAILED)));
    }
  }
  pending_ensure_pnacl_requests_.clear();
}

void PnaclTranslationResourceHost::CleanupEnsurePnaclRequests() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  for (EnsurePnaclInstalledList::iterator
           i = pending_ensure_pnacl_requests_.begin(),
           e = pending_ensure_pnacl_requests_.end();
       i != e; ++i) {
    (*i)->PostAbort();
  }
  pending_ensure_pnacl_requests_.clear();
}

#endif  // DISABLE_NACL
