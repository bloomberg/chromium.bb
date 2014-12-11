// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBMEDIAPLAYER_PARAMS_H_
#define MEDIA_BLINK_WEBMEDIAPLAYER_PARAMS_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebContentDecryptionModule;
class WebMediaPlayerClient;
}

namespace media {

class AudioRendererSink;
class MediaLog;

// Holds parameters for constructing WebMediaPlayerImpl without having
// to plumb arguments through various abstraction layers.
class MEDIA_EXPORT WebMediaPlayerParams {
 public:
  typedef base::Callback<void(const base::Closure&)> DeferLoadCB;

  // |defer_load_cb|, |audio_renderer_sink|, and |compositor_task_runner| may be
  // null.
  WebMediaPlayerParams(
      const DeferLoadCB& defer_load_cb,
      const scoped_refptr<AudioRendererSink>& audio_renderer_sink,
      const scoped_refptr<MediaLog>& media_log,
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
      blink::WebContentDecryptionModule* initial_cdm);

  ~WebMediaPlayerParams();

  base::Callback<void(const base::Closure&)> defer_load_cb() const {
    return defer_load_cb_;
  }

  const scoped_refptr<AudioRendererSink>& audio_renderer_sink() const {
    return audio_renderer_sink_;
  }

  const scoped_refptr<MediaLog>& media_log() const {
    return media_log_;
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner() const {
    return media_task_runner_;
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner()
      const {
    return compositor_task_runner_;
  }

  blink::WebContentDecryptionModule* initial_cdm() const {
    return initial_cdm_;
  }

 private:
  base::Callback<void(const base::Closure&)> defer_load_cb_;
  scoped_refptr<AudioRendererSink> audio_renderer_sink_;
  scoped_refptr<MediaLog> media_log_;
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  blink::WebContentDecryptionModule* initial_cdm_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMediaPlayerParams);
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBMEDIAPLAYER_PARAMS_H_
