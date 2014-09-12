// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_WEBMEDIAPLAYER_FACTORY_H_
#define MOJO_SERVICES_HTML_VIEWER_WEBMEDIAPLAYER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/base/audio_hardware_config.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebMediaPlayer;
class WebLocalFrame;
class WebURL;
class WebMediaPlayerClient;
}

namespace media {
class AudioManager;
class AudioRendererSink;
}

namespace mojo {

// Helper class used to create blink::WebMediaPlayer objects.
// This class stores the "global state" shared across all WebMediaPlayer
// instances.
class WebMediaPlayerFactory {
 public:
  explicit WebMediaPlayerFactory(const scoped_refptr<
      base::SingleThreadTaskRunner>& compositor_task_runner);
  ~WebMediaPlayerFactory();

  blink::WebMediaPlayer* CreateMediaPlayer(blink::WebLocalFrame* frame,
                                           const blink::WebURL& url,
                                           blink::WebMediaPlayerClient* client);

 private:
  const media::AudioHardwareConfig& GetAudioHardwareConfig();
  scoped_refptr<media::AudioRendererSink> CreateAudioRendererSink();
  scoped_refptr<base::SingleThreadTaskRunner> GetMediaThreadTaskRunner();

  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  base::Thread media_thread_;
  media::FakeAudioLogFactory fake_audio_log_factory_;
  scoped_ptr<media::AudioManager> audio_manager_;
  media::AudioHardwareConfig audio_hardware_config_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerFactory);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_HTML_VIEWER_WEBMEDIAPLAYER_FACTORY_H_
