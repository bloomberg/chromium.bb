// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Filters are connected in a strongly typed manner, with downstream filters
// always reading data from upstream filters.  Upstream filters have no clue
// who is actually reading from them, and return the results via callbacks.
//
//                         DemuxerStream(Video) <- VideoDecoder <- VideoRenderer
// DataSource <- Demuxer <
//                         DemuxerStream(Audio) <- AudioDecoder <- AudioRenderer
//
// Upstream -------------------------------------------------------> Downstream
//                         <- Reads flow this way
//                    Buffer assignments flow this way ->
//
// Every filter maintains a reference to the scheduler, who maintains data
// shared between filters (i.e., reference clock value, playback state).  The
// scheduler is also responsible for scheduling filter tasks (i.e., a read on
// a VideoDecoder would result in scheduling a Decode task).  Filters can also
// use the scheduler to signal errors and shutdown playback.

#ifndef MEDIA_BASE_FILTERS_H_
#define MEDIA_BASE_FILTERS_H_

#include <limits>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_frame.h"
#include "ui/gfx/size.h"

namespace media {

class Buffer;
class Decoder;
class DemuxerStream;
class Filter;
class FilterHost;

struct PipelineStatistics;

// Used for completing asynchronous methods.
typedef base::Callback<void(PipelineStatus)> FilterStatusCB;

// These functions copy |*cb|, call Reset() on |*cb|, and then call Run()
// on the copy.  This is used in the common case where you need to clear
// a callback member variable before running the callback.
MEDIA_EXPORT void ResetAndRunCB(FilterStatusCB* cb, PipelineStatus status);
MEDIA_EXPORT void ResetAndRunCB(base::Closure* cb);

// Used for updating pipeline statistics.
typedef base::Callback<void(const PipelineStatistics&)> StatisticsCallback;

class MEDIA_EXPORT Filter : public base::RefCountedThreadSafe<Filter> {
 public:
  Filter();

  // Sets the private member |host_|. This is the first method called by
  // the FilterHost after a filter is created.  The host holds a strong
  // reference to the filter.  The reference held by the host is guaranteed
  // to be released before the host object is destroyed by the pipeline.
  virtual void set_host(FilterHost* host);

  // Clear |host_| to signal abandonment.  Must be called after set_host() and
  // before any state-changing method below.
  virtual void clear_host();

  virtual FilterHost* host();

  // The pipeline has resumed playback.  Filters can continue requesting reads.
  // Filters may implement this method if they need to respond to this call.
  // TODO(boliu): Check that callback is not NULL in subclasses.
  virtual void Play(const base::Closure& callback);

  // The pipeline has paused playback.  Filters should stop buffer exchange.
  // Filters may implement this method if they need to respond to this call.
  // TODO(boliu): Check that callback is not NULL in subclasses.
  virtual void Pause(const base::Closure& callback);

  // The pipeline has been flushed.  Filters should return buffer to owners.
  // Filters may implement this method if they need to respond to this call.
  // TODO(boliu): Check that callback is not NULL in subclasses.
  virtual void Flush(const base::Closure& callback);

  // The pipeline is being stopped either as a result of an error or because
  // the client called Stop().
  // TODO(boliu): Check that callback is not NULL in subclasses.
  virtual void Stop(const base::Closure& callback);

  // The pipeline playback rate has been changed.  Filters may implement this
  // method if they need to respond to this call.
  virtual void SetPlaybackRate(float playback_rate);

  // Carry out any actions required to seek to the given time, executing the
  // callback upon completion.
  virtual void Seek(base::TimeDelta time, const FilterStatusCB& callback);

  // This method is called from the pipeline when the audio renderer
  // is disabled. Filters can ignore the notification if they do not
  // need to react to this event.
  virtual void OnAudioRendererDisabled();

 protected:
  // Only allow scoped_refptr<> to delete filters.
  friend class base::RefCountedThreadSafe<Filter>;
  virtual ~Filter();

  FilterHost* host() const { return host_; }

 private:
  FilterHost* host_;

  DISALLOW_COPY_AND_ASSIGN(Filter);
};

class MEDIA_EXPORT VideoDecoder : public Filter {
 public:
  // Initialize a VideoDecoder with the given DemuxerStream, executing the
  // callback upon completion.
  // stats_callback is used to update global pipeline statistics.
  virtual void Initialize(DemuxerStream* stream,
                          const PipelineStatusCB& callback,
                          const StatisticsCallback& stats_callback) = 0;

  // Request a frame to be decoded and returned via the provided callback.
  // Only one read may be in flight at any given time.
  //
  // Implementations guarantee that the callback will not be called from within
  // this method.
  //
  // Frames will be non-NULL yet may be end of stream frames.
  typedef base::Callback<void(scoped_refptr<VideoFrame>)> ReadCB;
  virtual void Read(const ReadCB& callback) = 0;

  // Returns the natural width and height of decoded video in pixels.
  //
  // Clients should NOT rely on these values to remain constant. Instead, use
  // the width/height from decoded video frames themselves.
  //
  // TODO(scherkus): why not rely on prerolling and decoding a single frame to
  // get dimensions?
  virtual const gfx::Size& natural_size() = 0;

  // Returns true if the output format has an alpha channel. Most formats do not
  // have alpha so the default is false. Override and return true for decoders
  // that return formats with an alpha channel.
  virtual bool HasAlpha() const;

  // Prepare decoder for shutdown.  This is a HACK needed because
  // PipelineImpl::Stop() goes through a Pause/Flush/Stop dance to all its
  // filters, waiting for each state transition to complete before starting the
  // next, but WebMediaPlayerImpl::Destroy() holds the renderer loop hostage for
  // the duration.  Default implementation does nothing; derived decoders may
  // override as needed.  http://crbug.com/110228 tracks removing this.
  virtual void PrepareForShutdownHack();

 protected:
  VideoDecoder();
  virtual ~VideoDecoder();
};


class MEDIA_EXPORT AudioDecoder : public Filter {
 public:
  // Initialize a AudioDecoder with the given DemuxerStream, executing the
  // callback upon completion.
  // stats_callback is used to update global pipeline statistics.
  //
  // TODO(scherkus): switch to PipelineStatus callback.
  virtual void Initialize(DemuxerStream* stream, const base::Closure& callback,
                          const StatisticsCallback& stats_callback) = 0;

  // Request samples to be decoded and returned via the provided callback.
  // Only one read may be in flight at any given time.
  //
  // Implementations guarantee that the callback will not be called from within
  // this method.
  //
  // Sample buffers will be non-NULL yet may be end of stream buffers.
  typedef base::Callback<void(scoped_refptr<Buffer>)> ReadCB;
  virtual void Read(const ReadCB& callback) = 0;

  // Returns various information about the decoded audio format.
  virtual int bits_per_channel() = 0;
  virtual ChannelLayout channel_layout() = 0;
  virtual int samples_per_second() = 0;

 protected:
  AudioDecoder();
  virtual ~AudioDecoder();
};


class MEDIA_EXPORT VideoRenderer : public Filter {
 public:
  // Initialize a VideoRenderer with the given VideoDecoder, executing the
  // callback upon completion.
  virtual void Initialize(VideoDecoder* decoder, const base::Closure& callback,
                          const StatisticsCallback& stats_callback) = 0;

  // Returns true if this filter has received and processed an end-of-stream
  // buffer.
  virtual bool HasEnded() = 0;
};


class MEDIA_EXPORT AudioRenderer : public Filter {
 public:
  // Initialize a AudioRenderer with the given AudioDecoder, executing the
  // |init_callback| upon completion. |underflow_callback| is called when the
  // renderer runs out of data to pass to the audio card during playback.
  // If the |underflow_callback| is called ResumeAfterUnderflow() must be called
  // to resume playback. Pause(), Seek(), or Stop() cancels the underflow
  // condition.
  virtual void Initialize(AudioDecoder* decoder,
                          const base::Closure& init_callback,
                          const base::Closure& underflow_callback) = 0;

  // Returns true if this filter has received and processed an end-of-stream
  // buffer.
  virtual bool HasEnded() = 0;

  // Sets the output volume.
  virtual void SetVolume(float volume) = 0;

  // Resumes playback after underflow occurs.
  // |buffer_more_audio| is set to true if you want to increase the size of the
  // decoded audio buffer.
  virtual void ResumeAfterUnderflow(bool buffer_more_audio) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_FILTERS_H_
