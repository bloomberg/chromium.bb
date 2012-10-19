// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDER_MEDIA_LOG_H_
#define CONTENT_RENDERER_MEDIA_RENDER_MEDIA_LOG_H_

#include "media/base/media_log.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

// RenderMediaLog is an implementation of MediaLog that passes all events to the
// browser process.
class RenderMediaLog : public media::MediaLog {
 public:
  RenderMediaLog();

  // MediaLog implementation.
  virtual void AddEvent(scoped_ptr<media::MediaLogEvent> event) OVERRIDE;

 private:
  virtual ~RenderMediaLog();

  scoped_refptr<base::MessageLoopProxy> render_loop_;

  DISALLOW_COPY_AND_ASSIGN(RenderMediaLog);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDER_MEDIA_LOG_H_
