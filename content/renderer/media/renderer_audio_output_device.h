// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDERER_AUDIO_OUTPUT_DEVICE_H_
#define CONTENT_RENDERER_MEDIA_RENDERER_AUDIO_OUTPUT_DEVICE_H_

#include "base/memory/ref_counted.h"
#include "media/audio/audio_output_device.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class AudioDeviceFactory;
class AudioMessageFilter;

// Subclass of media::AudioOutputDevice to support additional concepts within
// the content component.  Use AudioDeviceFactory to instantiate this class.
class RendererAudioOutputDevice
    : NON_EXPORTED_BASE(public media::AudioOutputDevice) {
 public:
  // Set the source of the rendered audio data to the RenderView specified by
  // |render_view_id|.  Callers should keep in mind that the RenderView which
  // caused the instantiation of RendererAudioOutputDevice might not necessarily
  // be same as the RenderView which actually renders the audio data.
  void SetSourceRenderView(int render_view_id);

  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;

 protected:
  friend class AudioDeviceFactory;

  // Creates an uninitialized RendererAudioOutputDevice.  Clients must call
  // Initialize() before using.
  RendererAudioOutputDevice(
      AudioMessageFilter* message_filter,
      const scoped_refptr<base::MessageLoopProxy>& io_loop);

 private:
  virtual ~RendererAudioOutputDevice();

  void OnStart();
  void OnStop();
  void OnSourceChange(int render_view_id);

  int source_render_view_id_;
  bool is_after_stream_created_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RendererAudioOutputDevice);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDERER_AUDIO_OUTPUT_DEVICE_H_
