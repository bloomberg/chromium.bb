// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_scheduler_filter.h"

#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_scheduler.h"
#include "content/common/view_messages.h"
#include "content/public/common/page_transition_types.h"

namespace content {

ResourceSchedulerFilter::ResourceSchedulerFilter(int child_id)
    : child_id_(child_id) {
}

ResourceSchedulerFilter::~ResourceSchedulerFilter() {
}

bool ResourceSchedulerFilter::OnMessageReceived(const IPC::Message& message,
                                                bool* message_was_ok) {
  switch (message.type()) {
    case ViewHostMsg_FrameNavigate::ID: {
      PickleIterator iter(message);
      ViewHostMsg_FrameNavigate_Params params;
      if (!IPC::ParamTraits<ViewHostMsg_FrameNavigate_Params>::Read(
          &message, &iter, &params)) {
        break;
      }
      if (PageTransitionIsMainFrame(params.transition) &&
          !params.was_within_same_page) {
        ResourceDispatcherHostImpl::Get()->scheduler()->OnNavigate(
            child_id_, message.routing_id());
      }
      break;
    }

    case ViewHostMsg_WillInsertBody::ID:
      ResourceDispatcherHostImpl::Get()->scheduler()->OnWillInsertBody(
          child_id_, message.routing_id());
      break;

    default:
      break;
  }

  return false;
}

}  // namespace content
