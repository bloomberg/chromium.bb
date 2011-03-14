// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "base/ref_counted.h"
#include "base/time.h"
#include "base/scoped_ptr.h"
#include "media/base/media_format.h"
#include "media/base/video_frame.h"

namespace media {

class Buffer;
class Decoder;
class DemuxerStream;
class Filter;
class FilterHost;

struct PipelineStatistics;

// Used for completing asynchronous methods.
typedef Callback0::Type FilterCallback;

// Used for updating pipeline statistics.
typedef Callback1<const PipelineStatistics&>::Type StatisticsCallback;

class Filter : public base::RefCountedThreadSafe<Filter> {
 public:
  Filter();

  // Sets the private member |host_|. This is the first method called by
  // the FilterHost after a filter is created.  The host holds a strong
  // reference to the filter.  The reference held by the host is guaranteed
  // to be released before the host object is destroyed by the pipeline.
  virtual void set_host(FilterHost* host);

  virtual FilterHost* host();

  // The pipeline has resumed playback.  Filters can continue requesting reads.
  // Filters may implement this method if they need to respond to this call.
  // TODO(boliu): Check that callback is not NULL in subclasses.
  virtual void Play(FilterCallback* callback);

  // The pipeline has paused playback.  Filters should stop buffer exchange.
  // Filters may implement this method if they need to respond to this call.
  // TODO(boliu): Check that callback is not NULL in subclasses.
  virtual void Pause(FilterCallback* callback);

  // The pipeline has been flushed.  Filters should return buffer to owners.
  // Filters may implement this method if they need to respond to this call.
  // TODO(boliu): Check that callback is not NULL in subclasses.
  virtual void Flush(FilterCallback* callback);

  // The pipeline is being stopped either as a result of an error or because
  // the client called Stop().
  // TODO(boliu): Check that callback is not NULL in subclasses.
  virtual void Stop(FilterCallback* callback);

  // The pipeline playback rate has been changed.  Filters may implement this
  // method if they need to respond to this call.
  virtual void SetPlaybackRate(float playback_rate);

  // Carry out any actions required to seek to the given time, executing the
  // callback upon completion.
  virtual void Seek(base::TimeDelta time, FilterCallback* callback);

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

class DataSource : public Filter {
 public:
  typedef Callback1<size_t>::Type ReadCallback;
  static const size_t kReadError = static_cast<size_t>(-1);

  // Reads |size| bytes from |position| into |data|. And when the read is done
  // or failed, |read_callback| is called with the number of bytes read or
  // kReadError in case of error.
  // TODO(hclam): should change |size| to int! It makes the code so messy
  // with size_t and int all over the place..
  virtual void Read(int64 position, size_t size,
                    uint8* data, ReadCallback* read_callback) = 0;

  // Returns true and the file size, false if the file size could not be
  // retrieved.
  virtual bool GetSize(int64* size_out) = 0;

  // Returns true if we are performing streaming. In this case seeking is
  // not possible.
  virtual bool IsStreaming() = 0;
};


class Demuxer : public Filter {
 public:
  // Returns the number of streams available
  virtual size_t GetNumberOfStreams() = 0;

  // Returns the stream for the given index, NULL otherwise
  virtual scoped_refptr<DemuxerStream> GetStream(int stream_id) = 0;
};


class DemuxerStream : public base::RefCountedThreadSafe<DemuxerStream> {
 public:
  enum Type {
    UNKNOWN,
    AUDIO,
    VIDEO,
  };

  // Schedules a read.  When the |read_callback| is called, the downstream
  // filter takes ownership of the buffer by AddRef()'ing the buffer.
  //
  // TODO(scherkus): switch Read() callback to scoped_refptr<>.
  virtual void Read(Callback1<Buffer*>::Type* read_callback) = 0;

  // Given a class that supports the |Interface| and a related static method
  // interface_id(), which returns a const char*, this method returns true if
  // the class returns an interface pointer and assigns the pointer to
  // |interface_out|.  Otherwise this method returns false.
  template <class Interface>
  bool QueryInterface(Interface** interface_out) {
    void* i = QueryInterface(Interface::interface_id());
    *interface_out = reinterpret_cast<Interface*>(i);
    return (NULL != i);
  };

  // Returns the type of stream.
  virtual Type type() = 0;

  // Returns the media format of this stream.
  virtual const MediaFormat& media_format() = 0;

  virtual void EnableBitstreamConverter() = 0;

 protected:
  // Optional method that is implemented by filters that support extended
  // interfaces.  The filter should return a pointer to the interface
  // associated with the |interface_id| string if they support it, otherwise
  // return NULL to indicate the interface is unknown.  The derived filter
  // should NOT AddRef() the interface.  The DemuxerStream::QueryInterface()
  // public template function will assign the interface to a scoped_refptr<>.
  virtual void* QueryInterface(const char* interface_id);

  friend class base::RefCountedThreadSafe<DemuxerStream>;
  virtual ~DemuxerStream();
};


class VideoDecoder : public Filter {
 public:
  // Initialize a VideoDecoder with the given DemuxerStream, executing the
  // callback upon completion.
  // stats_callback is used to update global pipeline statistics.
  virtual void Initialize(DemuxerStream* stream, FilterCallback* callback,
                          StatisticsCallback* stats_callback) = 0;

  // |set_fill_buffer_done_callback| install permanent callback from downstream
  // filter (i.e. Renderer). The callback is used to deliver video frames at
  // runtime to downstream filter
  typedef Callback1<scoped_refptr<VideoFrame> >::Type ConsumeVideoFrameCallback;
  void set_consume_video_frame_callback(ConsumeVideoFrameCallback* callback) {
    consume_video_frame_callback_.reset(callback);
  }

  // Renderer provides an output buffer for Decoder to write to. These buffers
  // will be recycled to renderer by |fill_buffer_done_callback_|.
  // We could also pass empty pointer here to let decoder provide buffers pool.
  virtual void ProduceVideoFrame(scoped_refptr<VideoFrame> frame) = 0;

  // Indicate whether decoder provides its own output buffers
  virtual bool ProvidesBuffer() = 0;

  // Returns the media format produced by this decoder.
  virtual const MediaFormat& media_format() = 0;

 protected:
  // A video frame is ready to be consumed. This method invoke
  // |consume_video_frame_callback_| internally.
  void VideoFrameReady(scoped_refptr<VideoFrame> frame) {
    consume_video_frame_callback_->Run(frame);
  }

  VideoDecoder();
  virtual ~VideoDecoder();

 private:
  scoped_ptr<ConsumeVideoFrameCallback> consume_video_frame_callback_;
};


class AudioDecoder : public Filter {
 public:
  // Initialize a AudioDecoder with the given DemuxerStream, executing the
  // callback upon completion.
  // stats_callback is used to update global pipeline statistics.
  virtual void Initialize(DemuxerStream* stream, FilterCallback* callback,
                          StatisticsCallback* stats_callback) = 0;

  // |set_fill_buffer_done_callback| install permanent callback from downstream
  // filter (i.e. Renderer). The callback is used to deliver buffers at
  // runtime to downstream filter.
  typedef Callback1<scoped_refptr<Buffer> >::Type ConsumeAudioSamplesCallback;
  void set_consume_audio_samples_callback(
      ConsumeAudioSamplesCallback* callback) {
    consume_audio_samples_callback_.reset(callback);
  }
  ConsumeAudioSamplesCallback* consume_audio_samples_callback() {
     return consume_audio_samples_callback_.get();
   }

  // Renderer provides an output buffer for Decoder to write to. These buffers
  // will be recycled to renderer by fill_buffer_done_callback_;
  // We could also pass empty pointer here to let decoder provide buffers pool.
  virtual void ProduceAudioSamples(scoped_refptr<Buffer> buffer) = 0;

  // Returns the media format produced by this decoder.
  virtual const MediaFormat& media_format() = 0;

 protected:
  AudioDecoder();
  ~AudioDecoder();

 private:
  scoped_ptr<ConsumeAudioSamplesCallback> consume_audio_samples_callback_;
};


class VideoRenderer : public Filter {
 public:
  // Initialize a VideoRenderer with the given VideoDecoder, executing the
  // callback upon completion.
  virtual void Initialize(VideoDecoder* decoder, FilterCallback* callback,
                          StatisticsCallback* stats_callback) = 0;

  // Returns true if this filter has received and processed an end-of-stream
  // buffer.
  virtual bool HasEnded() = 0;
};


class AudioRenderer : public Filter {
 public:
  // Initialize a AudioRenderer with the given AudioDecoder, executing the
  // callback upon completion.
  virtual void Initialize(AudioDecoder* decoder, FilterCallback* callback) = 0;

  // Returns true if this filter has received and processed an end-of-stream
  // buffer.
  virtual bool HasEnded() = 0;

  // Sets the output volume.
  virtual void SetVolume(float volume) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_FILTERS_H_
