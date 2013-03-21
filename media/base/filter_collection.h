// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FILTER_COLLECTION_H_
#define MEDIA_BASE_FILTER_COLLECTION_H_

#include <list>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"

namespace media {

class AudioRenderer;
class Demuxer;
class VideoDecoder;
class VideoRenderer;

// Represents a set of uninitialized demuxer and audio/video decoders and
// renderers. Used to start a Pipeline object for media playback.
//
// TODO(scherkus): Replace FilterCollection with something sensible, see
// http://crbug.com/110800
class MEDIA_EXPORT FilterCollection {
 public:
  typedef std::list<scoped_refptr<VideoDecoder> > VideoDecoderList;

  FilterCollection();
  ~FilterCollection();

  void SetDemuxer(const scoped_refptr<Demuxer>& demuxer);
  const scoped_refptr<Demuxer>& GetDemuxer();

  void SetAudioRenderer(scoped_ptr<AudioRenderer> audio_renderer);
  scoped_ptr<AudioRenderer> GetAudioRenderer();

  void SetVideoRenderer(scoped_ptr<VideoRenderer> video_renderer);
  scoped_ptr<VideoRenderer> GetVideoRenderer();

  // Remove remaining filters.
  void Clear();

  VideoDecoderList* GetVideoDecoders();

 private:
  scoped_refptr<Demuxer> demuxer_;
  VideoDecoderList video_decoders_;
  scoped_ptr<AudioRenderer> audio_renderer_;
  scoped_ptr<VideoRenderer> video_renderer_;

  DISALLOW_COPY_AND_ASSIGN(FilterCollection);
};

}  // namespace media

#endif  // MEDIA_BASE_FILTER_COLLECTION_H_
