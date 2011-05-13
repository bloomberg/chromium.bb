// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements a Demuxer that can switch among different data sources mid-stream.
// Uses FFmpegDemuxer under the covers, so see the caveats at the top of
// ffmpeg_demuxer.h.

#ifndef MEDIA_FILTERS_ADAPTIVE_DEMUXER_H_
#define MEDIA_FILTERS_ADAPTIVE_DEMUXER_H_

#include <deque>
#include <vector>

#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "media/base/buffers.h"
#include "media/base/filter_factories.h"
#include "media/base/filters.h"
#include "media/base/pipeline.h"
#include "media/base/media_format.h"

namespace media {

class AdaptiveDemuxer;

class AdaptiveDemuxerStream : public DemuxerStream {
 public:
  typedef std::vector<scoped_refptr<DemuxerStream> > StreamVector;

  // Keeps references to the passed-in streams.  |streams| must be non-empty and
  // all the streams in it must agree on type and media_format (or be NULL).
  // |initial_stream| must be a valid index into |streams| and specifies the
  // current stream on construction.
  AdaptiveDemuxerStream(StreamVector const& streams, int initial_stream);
  virtual ~AdaptiveDemuxerStream();

  // Change the stream to satisfy subsequent Read() requests from.  The
  // referenced pointer must not be NULL.
  void ChangeCurrentStream(int index);

  // DemuxerStream methods.
  virtual void Read(const ReadCallback& read_callback);
  virtual Type type();
  virtual const MediaFormat& media_format();
  virtual void EnableBitstreamConverter();
  virtual AVStream* GetAVStream();

 private:
  // Returns a pointer to the current stream.
  DemuxerStream* current_stream();

  // DEBUG_MODE-only CHECK that the data members are in a reasonable state.
  void DCheckSanity();

  StreamVector streams_;
  // Guards the members below.  Only held for simple variable reads/writes, not
  // during async operation.
  base::Lock lock_;
  int current_stream_index_;
  bool bitstream_converter_enabled_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AdaptiveDemuxerStream);
};

class AdaptiveDemuxer : public Demuxer {
 public:
  typedef std::vector<scoped_refptr<Demuxer> > DemuxerVector;

  // |demuxers| must be non-empty, and the index arguments must be valid indexes
  // into |demuxers|, or -1 to indicate no demuxer is serving that type.
  AdaptiveDemuxer(DemuxerVector const& demuxers,
                  int initial_audio_demuxer_index,
                  int initial_video_demuxer_index);
  virtual ~AdaptiveDemuxer();

  // Change which demuxers the streams will use.
  void ChangeCurrentDemuxer(int audio_index, int video_index);

  // Filter implementation.
  virtual void Stop(FilterCallback* callback);
  virtual void Seek(base::TimeDelta time, const FilterStatusCB&  cb);
  virtual void OnAudioRendererDisabled();
  virtual void set_host(FilterHost* filter_host);
  virtual void SetPlaybackRate(float playback_rate);
  virtual void SetPreload(Preload preload);

  // Demuxer implementation.
  virtual scoped_refptr<DemuxerStream> GetStream(DemuxerStream::Type type);

 private:
  // Returns a pointer to the currently active demuxer of the given type.
  Demuxer* current_demuxer(DemuxerStream::Type type);

  DemuxerVector demuxers_;
  scoped_refptr<AdaptiveDemuxerStream> audio_stream_;
  scoped_refptr<AdaptiveDemuxerStream> video_stream_;
  // Guards the members below.  Only held for simple variable reads/writes, not
  // during async operation.
  base::Lock lock_;
  int current_audio_demuxer_index_;
  int current_video_demuxer_index_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AdaptiveDemuxer);
};

// AdaptiveDemuxerFactory wraps an underlying DemuxerFactory that knows how to
// build demuxers for a single URL, and implements a primitive (for now) version
// of multi-file manifests.  The manifest is encoded in the |url| parameter to
// Build() as:
// x-adaptive:<initial_audio_index>:<initial_video_index>:<URL>[^<URL>]* where
// <URL>'s are "real" media URLs which are passed to the underlying
// DemuxerFactory's Build() method individually.  For backward-compatibility,
// the manifest URL may also simply be a regular URL in which case an implicit
// "x-adaptive:0:0:" is prepended.
class AdaptiveDemuxerFactory : public DemuxerFactory {
 public:
  // Takes a reference to |demuxer_factory|.
  AdaptiveDemuxerFactory(DemuxerFactory* delegate_factory);
  virtual ~AdaptiveDemuxerFactory();

  // DemuxerFactory methods.
  // If any of the underlying Demuxers encounter construction errors, only the
  // first one (in manifest order) will get reported back in |cb|.
  virtual void Build(const std::string& url, BuildCallback* cb);
  virtual DemuxerFactory* Clone() const;

 private:
  scoped_ptr<DemuxerFactory> delegate_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AdaptiveDemuxerFactory);
};

}  // namespace media

#endif  // MEDIA_FILTERS_ADAPTIVE_DEMUXER_H_
