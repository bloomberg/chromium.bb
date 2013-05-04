// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/string16.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/common/media_stream_request.h"

namespace media {
struct MediaLogEvent;
}

namespace content {

// This class stores information about currently active media.
// It's constructed on the UI thread but all of its methods are called on the IO
// thread.
class CONTENT_EXPORT MediaInternals {
 public:
  virtual ~MediaInternals();

  static MediaInternals* GetInstance();

  // The following methods are virtual for gmock.

  // Called when an audio stream is deleted.
  virtual void OnDeleteAudioStream(void* host, int stream_id);

  // Called when an audio stream is set to playing or paused.
  virtual void OnSetAudioStreamPlaying(void* host, int stream_id,
                                       bool playing);

  // Called when the status of an audio stream is set to "created", "closed", or
  // "error".
  virtual void OnSetAudioStreamStatus(void* host, int stream_id,
                                      const std::string& status);

  // Called when the volume of an audio stream is set.
  virtual void OnSetAudioStreamVolume(void* host, int stream_id,
                                      double volume);

  // Called when a MediaEvent occurs.
  virtual void OnMediaEvents(int render_process_id,
                             const std::vector<media::MediaLogEvent>& events);

  // Called with the update string.
  typedef base::Callback<void(const string16&)> UpdateCallback;

  // Add/remove update callbacks (see above).
  void AddUpdateCallback(const UpdateCallback& callback);
  void RemoveUpdateCallback(const UpdateCallback& callback);
  void SendEverything();

 private:
  friend class MockMediaInternals;
  friend class MediaInternalsTest;
  friend struct DefaultSingletonTraits<MediaInternals>;

  MediaInternals();

  // Sets |property| of an audio stream to |value| and notifies observers.
  // (host, stream_id) is a unique id for the audio stream.
  // |host| will never be dereferenced.
  void UpdateAudioStream(void* host, int stream_id,
                         const std::string& property, Value* value);

  // Removes |item| from |data_|.
  void DeleteItem(const std::string& item);

  // Sets data_.id.property = value and notifies attached UIs using update_fn.
  // id may be any depth, e.g. "video.decoders.1.2.3"
  void UpdateItem(const std::string& update_fn, const std::string& id,
                  const std::string& property, Value* value);

  // Calls javascript |function|(|value|) on each attached UI.
  void SendUpdate(const std::string& function, Value* value);

  DictionaryValue data_;

  std::vector<UpdateCallback> update_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(MediaInternals);
};

} // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_H_
