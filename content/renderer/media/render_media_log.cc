// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/render_media_log.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread_impl.h"

namespace content {

RenderMediaLog::RenderMediaLog()
    : render_loop_(base::MessageLoopProxy::current()) {
  DCHECK(RenderThreadImpl::current()) <<
      "RenderMediaLog must be constructed on the render thread";
}

void RenderMediaLog::AddEvent(scoped_ptr<media::MediaLogEvent> event) {
  if (RenderThreadImpl::current()) {
    RenderThreadImpl::current()->Send(
        new ViewHostMsg_MediaLogEvent(*event));
  } else {
    render_loop_->PostTask(FROM_HERE, base::Bind(
        &RenderMediaLog::AddEvent, this, base::Passed(&event)));
  }
}

RenderMediaLog::~RenderMediaLog() {}

}  // namespace content
