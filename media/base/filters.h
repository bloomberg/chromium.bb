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

class AudioDecoder;
class Buffer;
class Decoder;
class DemuxerStream;
class Filter;
class FilterHost;

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
  virtual void Seek(base::TimeDelta time, const PipelineStatusCB& callback);

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
  // Status codes for read operations on VideoDecoder.
  enum DecoderStatus {
    kOk,  // Everything went as planned.
    kDecodeError,  // Decoding error happened.
    kDecryptError  // Decrypting error happened.
  };

  // Initialize a VideoDecoder with the given DemuxerStream, executing the
  // callback upon completion.
  // statistics_cb is used to update global pipeline statistics.
  virtual void Initialize(DemuxerStream* stream,
                          const PipelineStatusCB& status_cb,
                          const StatisticsCB& statistics_cb) = 0;

  // Requests a frame to be decoded. The status of the decoder and decoded frame
  // are returned via the provided callback. Only one read may be in flight at
  // any given time.
  //
  // Implementations guarantee that the callback will not be called from within
  // this method.
  //
  // If the returned status is not kOk, some error has occurred in the video
  // decoder. In this case, the returned frame should always be NULL.
  //
  // Otherwise, the video decoder is in good shape. In this case, Non-NULL
  // frames contain decoded video data or may indicate the end of the stream.
  // NULL video frames indicate an aborted read. This can happen if the
  // DemuxerStream gets flushed and doesn't have any more data to return.
  typedef base::Callback<void(DecoderStatus, scoped_refptr<VideoFrame>)> ReadCB;
  virtual void Read(const ReadCB& read_cb) = 0;

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

 private:
  // These functions will be removed later. Declare here to make sure they are
  // not called from VideoDecoder interface anymore.
  // TODO(xhwang): Remove them when VideoDecoder is not a Filter any more.
  // See bug: http://crbug.com/108340
  virtual void Play(const base::Closure& callback) OVERRIDE;
  virtual void Pause(const base::Closure& callback) OVERRIDE;
  virtual void Seek(base::TimeDelta time,
                    const PipelineStatusCB& callback) OVERRIDE;
  virtual FilterHost* host() OVERRIDE;
};

class MEDIA_EXPORT VideoRenderer : public Filter {
 public:
  // Used to update the pipeline's clock time. The parameter is the time that
  // the clock should not exceed.
  typedef base::Callback<void(base::TimeDelta)> TimeCB;

  // Initialize a VideoRenderer with the given VideoDecoder, executing the
  // callback upon completion.
  virtual void Initialize(const scoped_refptr<VideoDecoder>& decoder,
                          const PipelineStatusCB& status_cb,
                          const StatisticsCB& statistics_cb,
                          const TimeCB& time_cb) = 0;

  // Returns true if this filter has received and processed an end-of-stream
  // buffer.
  virtual bool HasEnded() = 0;
};

class MEDIA_EXPORT AudioRenderer : public Filter {
 public:
  // Used to update the pipeline's clock time. The first parameter is the
  // current time, and the second parameter is the time that the clock must not
  // exceed.
  typedef base::Callback<void(base::TimeDelta, base::TimeDelta)> TimeCB;

  // Initialize a AudioRenderer with the given AudioDecoder, executing the
  // |init_cb| upon completion. |underflow_cb| is called when the
  // renderer runs out of data to pass to the audio card during playback.
  // If the |underflow_cb| is called ResumeAfterUnderflow() must be called
  // to resume playback. Pause(), Seek(), or Stop() cancels the underflow
  // condition.
  virtual void Initialize(const scoped_refptr<AudioDecoder>& decoder,
                          const PipelineStatusCB& init_cb,
                          const base::Closure& underflow_cb,
                          const TimeCB& time_cb) = 0;

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
