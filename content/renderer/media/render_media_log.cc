// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/render_media_log.h"

#include "base/message_loop_proxy.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread.h"

RenderMediaLog::RenderMediaLog()
    : render_loop_(base::MessageLoopProxy::current()) {
  DCHECK(RenderThread::current()) <<
      "RenderMediaLog must be constructed on the render thread";
}

void RenderMediaLog::AddEvent(media::MediaLogEvent* event) {
  scoped_ptr<media::MediaLogEvent> e(event);

  if (RenderThread::current()) {
    RenderThread::current()->Send(new ViewHostMsg_MediaLogEvent(*e));
  } else {
    render_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &RenderMediaLog::AddEvent, e.release()));
  }
}

RenderMediaLog::~RenderMediaLog() {}
