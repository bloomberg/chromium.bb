// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_scheduler_filter.h"

#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_scheduler.h"
#include "content/common/frame_messages.h"
#include "ipc/ipc_message_macros.h"
#include "ui/base/page_transition_types.h"

namespace content {

ResourceSchedulerFilter::ResourceSchedulerFilter(int child_id)
    : BrowserMessageFilter(FrameMsgStart), child_id_(child_id) {}

ResourceSchedulerFilter::~ResourceSchedulerFilter() {}

bool ResourceSchedulerFilter::OnMessageReceived(const IPC::Message& message) {
  ResourceScheduler* scheduler = ResourceDispatcherHostImpl::Get()->scheduler();
  if (!scheduler)
    return false;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(ResourceSchedulerFilter, message, scheduler)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidCommitProvisionalLoad,
                        OnDidCommitProvisionalLoad)
    IPC_MESSAGE_HANDLER(FrameHostMsg_WillInsertBody, OnWillInsertBody)
  IPC_END_MESSAGE_MAP()
  return false;
}

void ResourceSchedulerFilter::OnDidCommitProvisionalLoad(
    ResourceScheduler* scheduler,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {
  // TODO(csharrison): This isn't quite right for OOPIF, as we *do* want to
  // propagate OnNavigate to the client associated with the OOPIF's RVH. This
  // should not result in show-stopping bugs, just poorer loading performance.
  if (ui::PageTransitionIsMainFrame(params.transition) &&
      !params.was_within_same_page) {
    scheduler->OnNavigate(child_id_, params.render_view_routing_id);
  }
}

void ResourceSchedulerFilter::OnWillInsertBody(ResourceScheduler* scheduler,
                                               int render_view_routing_id) {
  scheduler->OnWillInsertBody(child_id_, render_view_routing_id);
}

}  // namespace content
