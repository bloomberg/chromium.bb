// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_RESOURCE_H_
#define MEDIA_BASE_MEDIA_RESOURCE_H_

#include "base/macros.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/media_url_params.h"
#include "url/gurl.h"

namespace media {

// Abstract class that defines how to retrieve "media resources" in
// DemuxerStream form (for most cases) or URL form (for the MediaPlayerRenderer
// case).
//
// The derived classes must return a non-null value for the getter method
// associated with their type, and return a null/empty value for other getters.
class MEDIA_EXPORT MediaResource {
 public:
  enum Type {
    STREAM,  // Indicates GetStreams() should be used
    URL,     // Indicates GetUrl() should be used
  };

  MediaResource();
  virtual ~MediaResource();

  // For Type::STREAM:
  //   Returns the first stream of the given stream type (which is not allowed
  //   to be DemuxerStream::TEXT), or NULL if that type of stream is not
  //   present.
  //   NOTE: Once a DemuxerStream pointer is returned from GetStream it is
  //   guaranteed to stay valid for as long as the Demuxer/MediaResource
  //   is alive. But make no assumption that once GetStream returned a non-null
  //   pointer for some stream type then all subsequent calls will also return
  //   non-null pointer for the same stream type. In MSE Javascript code can
  //   remove SourceBuffer from a MediaSource at any point and this will make
  //   some previously existing streams inaccessible/unavailable.
  // Other types:
  //   Should not be called.
  virtual DemuxerStream* GetStream(DemuxerStream::Type type) = 0;

  // For Type::URL:
  //   Returns the URL parameters of the media to play. Empty URLs are legal,
  //   and should be handled appropriately by the caller.
  // Other types:
  //   Should not be called.
  virtual MediaUrlParams GetMediaUrlParams() const;

  virtual MediaResource::Type GetType() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaResource);
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_RESOURCE_H_
