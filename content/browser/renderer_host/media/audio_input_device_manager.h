// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AudioInputDeviceManager manages the audio input devices. In particular it
// communicates with MediaStreamManager and AudioInputRendererHost on the
// browser IO thread, handles queries like
// enumerate/open/close/GetOpenedDeviceInfoById from MediaStreamManager and
// GetOpenedDeviceInfoById from AudioInputRendererHost.
// The work for enumerate/open/close is handled asynchronously on Media Stream
// device thread, while GetOpenedDeviceInfoById is synchronous on the IO thread.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/common/media_stream_request.h"

namespace media {
class AudioSystem;
}

namespace content {

// Should be used on IO thread only.
class CONTENT_EXPORT AudioInputDeviceManager : public MediaStreamProvider {
 public:
  // Calling Start() with this kFakeOpenSessionId will open the default device,
  // even though Open() has not been called. This is used to be able to use the
  // AudioInputDeviceManager before MediaStream is implemented.
  // TODO(xians): Remove it when the webrtc unittest does not need it any more.
  static const int kFakeOpenSessionId;

  explicit AudioInputDeviceManager(media::AudioSystem* audio_system);

  // Gets the opened device info by |session_id|. Returns NULL if the device
  // is not opened, otherwise the opened device. Called on IO thread.
  const StreamDeviceInfo* GetOpenedDeviceInfoById(int session_id);

  // MediaStreamProvider implementation.
  void RegisterListener(MediaStreamProviderListener* listener) override;
  void UnregisterListener(MediaStreamProviderListener* listener) override;
  int Open(const MediaStreamDevice& device) override;
  void Close(int session_id) override;

#if defined(OS_CHROMEOS)
  // Registers and unregisters that a stream using keyboard mic has been opened
  // or closed. Keeps count of how many such streams are open and activates and
  // inactivates the keyboard mic accordingly. The (in)activation is done on the
  // UI thread and for the register case a callback must therefor be provided
  // which is called when activated.
  void RegisterKeyboardMicStream(base::OnceClosure callback);
  void UnregisterKeyboardMicStream();
#endif

 private:
  typedef std::vector<StreamDeviceInfo> StreamDeviceList;
  ~AudioInputDeviceManager() override;

  // Callback called on IO thread when device is opened.
  void OpenedOnIOThread(int session_id,
                        const MediaStreamDevice& device,
                        base::TimeTicks start_time,
                        const media::AudioParameters& input_params,
                        const media::AudioParameters& matched_output_params,
                        const std::string& matched_output_device_id);

  // Callback called on IO thread with the session_id referencing the closed
  // device.
  void ClosedOnIOThread(MediaStreamType type, int session_id);

  // Helper to return iterator to the device referenced by |session_id|. If no
  // device is found, it will return devices_.end().
  StreamDeviceList::iterator GetDevice(int session_id);

#if defined(OS_CHROMEOS)
  // Calls Cras audio handler and sets keyboard mic active status.
  void SetKeyboardMicStreamActiveOnUIThread(bool active);
#endif

  // Only accessed on Browser::IO thread.
  base::ObserverList<MediaStreamProviderListener> listeners_;
  int next_capture_session_id_;
  StreamDeviceList devices_;

#if defined(OS_CHROMEOS)
  // Keeps count of how many streams are using keyboard mic.
  int keyboard_mic_streams_count_;
#endif

  media::AudioSystem* const audio_system_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputDeviceManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_H_
