// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_RESOURCE_H_
#define MEDIA_BASE_MEDIA_RESOURCE_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/media_url_params.h"
#include "url/gurl.h"

namespace media {

// The callback that is used to notify clients about streams being enabled and
// disabled. The first parameter is the DemuxerStream whose status changed. The
// second parameter is a bool indicating whether the stream got enabled or
// disabled. The third parameter specifies the media playback position at the
// time the status change happened.
using StreamStatusChangeCB =
    base::RepeatingCallback<void(DemuxerStream*, bool, base::TimeDelta)>;

// Abstract class that defines how to retrieve "media resources" in
// DemuxerStream form (for most cases) or URL form (for the MediaPlayerRenderer
// case).
//
// The derived classes must return a non-null value for the getter method
// associated with their type, and return a null/empty value for other getters.
class MEDIA_EXPORT MediaResource {
 public:
  enum Type {
    STREAM,  // Indicates GetAllStreams() or GetFirstStream() should be used
    URL,     // Indicates GetUrl() should be used
  };

  MediaResource();
  virtual ~MediaResource();

  virtual MediaResource::Type GetType() const;

  // For Type::STREAM:
  // Returns a collection of available DemuxerStream objects.
  //   NOTE: Once a DemuxerStream pointer is returned from GetStream it is
  //   guaranteed to stay valid for as long as the Demuxer/MediaResource
  //   is alive. But make no assumption that once GetStream returned a non-null
  //   pointer for some stream type then all subsequent calls will also return
  //   non-null pointer for the same stream type. In MSE Javascript code can
  //   remove SourceBuffer from a MediaSource at any point and this will make
  //   some previously existing streams inaccessible/unavailable.
  virtual std::vector<DemuxerStream*> GetAllStreams() = 0;

  // A helper function that return the first stream of the given |type| if one
  // exists or a null pointer if there is no streams of that type.
  DemuxerStream* GetFirstStream(DemuxerStream::Type type);

  // The StreamStatusChangeCB allows clients to receive notifications about one
  // of the streams being disabled or enabled.
  virtual void SetStreamStatusChangeCB(const StreamStatusChangeCB& cb) = 0;

  // For Type::URL:
  //   Returns the URL parameters of the media to play. Empty URLs are legal,
  //   and should be handled appropriately by the caller.
  // Other types:
  //   Should not be called.
  virtual MediaUrlParams GetMediaUrlParams() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaResource);
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_RESOURCE_H_
