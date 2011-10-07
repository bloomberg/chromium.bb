// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/render_media_log.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread_impl.h"

RenderMediaLog::RenderMediaLog()
    : render_loop_(base::MessageLoopProxy::current()) {
  DCHECK(RenderThreadImpl::current()) <<
      "RenderMediaLog must be constructed on the render thread";
}

void RenderMediaLog::AddEvent(media::MediaLogEvent* event) {
  scoped_ptr<media::MediaLogEvent> e(event);

  if (RenderThreadImpl::current()) {
    RenderThreadImpl::current()->Send(new ViewHostMsg_MediaLogEvent(*e));
  } else {
    render_loop_->PostTask(FROM_HERE,
        base::Bind(&RenderMediaLog::AddEvent, this, e.release()));
  }
}

RenderMediaLog::~RenderMediaLog() {}
