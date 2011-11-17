// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AudioInputDeviceManager manages the audio input devices. In particular it
// communicates with MediaStreamManager and AudioInputRendererHost on the
// browser IO thread, handles queries like enumerate/open/close from
// MediaStreamManager and start/stop from AudioInputRendererHost.

// All the queries come from the IO thread, while the work to enumerate devices
// is done on its own thread.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_H_

#include <map>

#include "base/threading/thread.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "media/audio/audio_device_name.h"

namespace media_stream {

class AudioInputDeviceManagerEventHandler;

class CONTENT_EXPORT AudioInputDeviceManager : public MediaStreamProvider {
 public:
  // Calling Start() with this kFakeOpenSessionId will open the default device,
  // even though Open() has not been called. This is used to be able to use the
  // AudioInputDeviceManager before MediaStream is implemented.
  static const int kFakeOpenSessionId;
  static const int kInvalidSessionId;
  static const int kInvalidDevice;
  static const int kDefaultDeviceIndex;

  AudioInputDeviceManager();
  virtual ~AudioInputDeviceManager();

  // MediaStreamProvider implementation, called on IO thread.
  virtual void Register(MediaStreamProviderListener* listener) OVERRIDE;
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

  // Function used for testing to mock platform dependent device code.
  MessageLoop* message_loop();

 private:
  // Executed on audio_input_device_thread_.
  void EnumerateOnDeviceThread();
  void OpenOnDeviceThread(int session_id, const StreamDeviceInfo& device);
  void CloseOnDeviceThread(int session_id);
  void StartOnDeviceThread(int session_id);
  void StopOnDeviceThread(int session_id);

  // Executed on IO thread to call Listener.
  void DevicesEnumeratedOnIOThread(const StreamDeviceInfoArray& devices);
  void OpenedOnIOThread(int session_id);
  void ClosedOnIOThread(int session_id);
  void ErrorOnIOThread(int session_id, MediaStreamProviderError error);

  // Executed on IO thread to call the event handler.
  void StartedOnIOThread(int session_id, int index);
  void StoppedOnIOThread(int session_id);

  // Executed on audio_input_device_thread_ to make sure
  // MediaStreamProviderListener is called from IO thread.
  void SignalError(int session_id, MediaStreamProviderError error);

  // Helpers.
  bool IsOnCaptureDeviceThread() const;
  void UnregisterEventHandler(int session_id);
  bool HasEventHandler(int session_id);

  // Thread for all calls to AudioInputDeviceManager.
  base::Thread audio_input_device_thread_;

  // Only accessed on Browser::IO thread.
  MediaStreamProviderListener* listener_;
  int next_capture_session_id_;
  typedef std::map<int, AudioInputDeviceManagerEventHandler*> EventHandlerMap;
  EventHandlerMap event_handlers_;

  // Only accessed from audio_input_device_thread_.
  typedef std::map<int, media::AudioDeviceName> AudioInputDeviceMap;
  AudioInputDeviceMap devices_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputDeviceManager);
};

}  // namespace media_stream

DISABLE_RUNNABLE_METHOD_REFCOUNT(media_stream::AudioInputDeviceManager);

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_H_
