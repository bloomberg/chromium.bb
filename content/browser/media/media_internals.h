// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "media/audio/audio_logging.h"
#include "media/base/media_log.h"
#include "media/video/capture/video_capture_device_info.h"

namespace media {
class AudioParameters;
struct MediaLogEvent;
}

namespace content {

// This class stores information about currently active media.
class CONTENT_EXPORT MediaInternals
    : NON_EXPORTED_BASE(public media::AudioLogFactory) {
 public:
  // Called with the update string.
  typedef base::Callback<void(const base::string16&)> UpdateCallback;

  static MediaInternals* GetInstance();

  ~MediaInternals() override;

  // Called when a MediaEvent occurs.
  void OnMediaEvents(int render_process_id,
                     const std::vector<media::MediaLogEvent>& events);

  // Add/remove update callbacks (see above). Must be called on the UI thread.
  // The callbacks must also be fired on UI thread.
  void AddUpdateCallback(const UpdateCallback& callback);
  void RemoveUpdateCallback(const UpdateCallback& callback);

  // Whether there are any update callbacks available. Can be called on any
  // thread.
  bool CanUpdate();

  // Sends all audio cached data to each registered UpdateCallback.
  void SendAudioStreamData();

  // Sends all video capture capabilities cached data to each registered
  // UpdateCallback.
  void SendVideoCaptureDeviceCapabilities();

  // Called to inform of the capabilities enumerated for video devices.
  void UpdateVideoCaptureDeviceCapabilities(
      const media::VideoCaptureDeviceInfos& video_capture_device_infos);

  // AudioLogFactory implementation.  Safe to call from any thread.
  scoped_ptr<media::AudioLog> CreateAudioLog(AudioComponent component) override;

  // If possible, i.e. a WebContents exists for the given RenderFrameHostID,
  // tells an existing AudioLogEntry the WebContents title for easier
  // differentiation on the UI.
  void SetWebContentsTitleForAudioLogEntry(int component_id,
                                           int render_process_id,
                                           int render_frame_id,
                                           media::AudioLog* audio_log);

 private:
  // Inner class to handle reporting pipelinestatus to UMA
  class MediaInternalsUMAHandler;

  friend class AudioLogImpl;
  friend class MediaInternalsTest;
  friend struct base::DefaultLazyInstanceTraits<MediaInternals>;

  MediaInternals();

  // Sends |update| to each registered UpdateCallback.  Safe to call from any
  // thread, but will forward to the IO thread.
  void SendUpdate(const base::string16& update);

  // Caches |value| under |cache_key| so that future SendAudioLogUpdate() calls
  // will include the current data.  Calls JavaScript |function|(|value|) for
  // each registered UpdateCallback.
  enum AudioLogUpdateType {
    CREATE,             // Creates a new AudioLog cache entry.
    UPDATE_IF_EXISTS,   // Updates an existing AudioLog cache entry, does
                        // nothing if it doesn't exist.
    UPDATE_AND_DELETE,  // Deletes an existing AudioLog cache entry.
  };
  void SendAudioLogUpdate(AudioLogUpdateType type,
                          const std::string& cache_key,
                          const std::string& function,
                          const base::DictionaryValue* value);

  // Must only be accessed on the UI thread.
  std::vector<UpdateCallback> update_callbacks_;

  // Must only be accessed on the IO thread.
  base::ListValue video_capture_capabilities_cached_data_;

  // All variables below must be accessed under |lock_|.
  base::Lock lock_;
  bool can_update_;
  base::DictionaryValue audio_streams_cached_data_;
  int owner_ids_[AUDIO_COMPONENT_MAX];
  scoped_ptr<MediaInternalsUMAHandler> uma_handler_;

  DISALLOW_COPY_AND_ASSIGN(MediaInternals);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_H_
