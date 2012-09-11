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

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/common/media_stream_request.h"

namespace media {
class AudioManager;
}

namespace media_stream {

class AudioInputDeviceManagerEventHandler;

class CONTENT_EXPORT AudioInputDeviceManager : public MediaStreamProvider {
 public:
  // Calling Start() with this kFakeOpenSessionId will open the default device,
  // even though Open() has not been called. This is used to be able to use the
  // AudioInputDeviceManager before MediaStream is implemented.
  static const int kFakeOpenSessionId;

  explicit AudioInputDeviceManager(media::AudioManager* audio_manager);

  // MediaStreamProvider implementation, called on IO thread.
  virtual void Register(MediaStreamProviderListener* listener,
                        base::MessageLoopProxy* device_thread_loop) OVERRIDE;
  virtual void Unregister() OVERRIDE;
  virtual void EnumerateDevices() OVERRIDE;
  virtual int Open(const StreamDeviceInfo& device) OVERRIDE;
  virtual void Close(int session_id) OVERRIDE;

  // Functions used by AudioInputRenderHost, called on IO thread.
  // Starts the device referenced by the session id.
  void Start(int session_id, AudioInputDeviceManagerEventHandler* handler);
  // Stops the device referenced by the session id.
  void Stop(int session_id);

 private:
  typedef std::map<int, AudioInputDeviceManagerEventHandler*> EventHandlerMap;
  typedef std::map<int, StreamDeviceInfo> StreamDeviceMap;
  virtual ~AudioInputDeviceManager();

  // Enumerates audio input devices on media stream device thread.
  void EnumerateOnDeviceThread();
  // Opens the device on media stream device thread.
  void OpenOnDeviceThread(int session_id, const StreamDeviceInfo& device);
  // Closes the deivce on the media stream device thread.
  void CloseOnDeviceThread(int session_id);

  // Callback used by EnumerateOnDeviceThread(), called with a list of
  // enumerated devices on IO thread.
  void DevicesEnumeratedOnIOThread(StreamDeviceInfoArray* devices);
  // Callback used by OpenOnDeviceThread(), called with the session_id
  // referencing the opened device on IO thread.
  void OpenedOnIOThread(content::MediaStreamDeviceType type, int session_id);
  // Callback used by CloseOnDeviceThread(), called with the session_id
  // referencing the closed device on IO thread.
  void ClosedOnIOThread(content::MediaStreamDeviceType type, int session_id);

  // Verifies that the calling thread is media stream device thread.
  bool IsOnDeviceThread() const;

  // Only accessed on Browser::IO thread.
  MediaStreamProviderListener* listener_;
  int next_capture_session_id_;
  EventHandlerMap event_handlers_;

  // Only accessed from media stream device thread.
  StreamDeviceMap devices_;
  media::AudioManager* const audio_manager_;

  // The message loop of media stream device thread that this object runs on.
  scoped_refptr<base::MessageLoopProxy> device_loop_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputDeviceManager);
};

}  // namespace media_stream

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_H_
