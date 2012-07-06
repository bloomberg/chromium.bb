// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AudioInputDeviceManager manages the audio input devices. In particular it
// communicates with MediaStreamManager and AudioInputRendererHost on the
// browser IO thread, handles queries like enumerate/open/close from
// MediaStreamManager and start/stop from AudioInputRendererHost.
// The work for enumerate/open/close is handled asynchronously on Media Stream
// device thread, while start/stop are synchronous on the IO thread.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "media/audio/audio_device_name.h"

namespace media {
class AudioManager;
}

namespace media_stream {

class AudioInputDeviceManagerEventHandler;

class CONTENT_EXPORT AudioInputDeviceManager
    : public base::RefCountedThreadSafe<AudioInputDeviceManager>,
      public MediaStreamProvider {
 public:
  // Calling Start() with this kFakeOpenSessionId will open the default device,
  // even though Open() has not been called. This is used to be able to use the
  // AudioInputDeviceManager before MediaStream is implemented.
  static const int kFakeOpenSessionId;

  static const int kInvalidSessionId;
  static const char kInvalidDeviceId[];

  explicit AudioInputDeviceManager(media::AudioManager* audio_manager);

  // MediaStreamProvider implementation, called on IO thread.
  virtual void Register(MediaStreamProviderListener* listener,
                        base::MessageLoopProxy* device_thread_loop) OVERRIDE;
  virtual void Unregister() OVERRIDE;
  virtual void EnumerateDevices() OVERRIDE;
  virtual int Open(const StreamDeviceInfo& device) OVERRIDE;
  virtual void Close(int session_id) OVERRIDE;

  // Functions used by AudioInputRenderHost, called on IO thread.
  // Start the device referenced by the session id.
  void Start(int session_id,
             AudioInputDeviceManagerEventHandler* event_handler);
  // Stop the device referenced by the session id.
  void Stop(int session_id);

 private:
  friend class base::RefCountedThreadSafe<AudioInputDeviceManager>;
  virtual ~AudioInputDeviceManager();

  // Executed on media stream device thread.
  void EnumerateOnDeviceThread();
  void OpenOnDeviceThread(int session_id, const StreamDeviceInfo& device);
  void CloseOnDeviceThread(int session_id);

  // Executed on IO thread to call Listener.
  void DevicesEnumeratedOnIOThread(StreamDeviceInfoArray* devices);
  void OpenedOnIOThread(int session_id);
  void ClosedOnIOThread(int session_id);
  void ErrorOnIOThread(int session_id, MediaStreamProviderError error);

  bool IsOnDeviceThread() const;

  // Only accessed on Browser::IO thread.
  MediaStreamProviderListener* listener_;
  int next_capture_session_id_;
  typedef std::map<int, AudioInputDeviceManagerEventHandler*> EventHandlerMap;
  EventHandlerMap event_handlers_;

  // Only accessed from media stream device thread.
  typedef std::map<int, media::AudioDeviceName> AudioInputDeviceMap;
  AudioInputDeviceMap devices_;
  media::AudioManager* audio_manager_;

  // The message loop of media stream device thread that this object runs on.
  scoped_refptr<base::MessageLoopProxy> device_loop_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputDeviceManager);
};

}  // namespace media_stream

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_H_
