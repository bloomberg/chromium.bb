// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDER_MEDIA_LOG_H_
#define CONTENT_RENDERER_MEDIA_RENDER_MEDIA_LOG_H_

#include <vector>
#include "base/time.h"
#include "media/base/media_log.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

// RenderMediaLog is an implementation of MediaLog that passes all events to the
// browser process, throttling as necessary.
class RenderMediaLog : public media::MediaLog {
 public:
  RenderMediaLog();

  // MediaLog implementation.
  virtual void AddEvent(scoped_ptr<media::MediaLogEvent> event) OVERRIDE;

 private:
  virtual ~RenderMediaLog();

  scoped_refptr<base::MessageLoopProxy> render_loop_;
  base::Time last_ipc_send_time_;
  std::vector<media::MediaLogEvent> queued_media_events_;

  DISALLOW_COPY_AND_ASSIGN(RenderMediaLog);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDER_MEDIA_LOG_H_
