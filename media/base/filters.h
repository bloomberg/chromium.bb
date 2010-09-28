// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

class MessageLoop;

namespace media {

class Buffer;
class Decoder;
class DemuxerStream;
class FilterHost;
class WritableBuffer;

// Identifies the type of filter implementation.  Used in conjunction with some
// template wizardry to enforce strongly typed operations.  More or less a
// knock off of MSVC's __uuidof() operator.
enum FilterType {
  FILTER_DATA_SOURCE,
  FILTER_DEMUXER,
  FILTER_AUDIO_DECODER,
  FILTER_VIDEO_DECODER,
  FILTER_AUDIO_RENDERER,
  FILTER_VIDEO_RENDERER
};

// Used for completing asynchronous methods.
typedef Callback0::Type FilterCallback;

class MediaFilter : public base::RefCountedThreadSafe<MediaFilter> {
 public:
  MediaFilter();

  // Sets the private member |host_|. This is the first method called by
  // the FilterHost after a filter is created.  The host holds a strong
  // reference to the filter.  The reference held by the host is guaranteed
  // to be released before the host object is destroyed by the pipeline.
  virtual void set_host(FilterHost* host);

  virtual FilterHost* host();

  // Sets the private member |message_loop_|, which is used by filters for
  // processing asynchronous tasks and maintaining synchronized access to
  // internal data members.  The message loop should be running and exceed the
  // lifetime of the filter.
  virtual void set_message_loop(MessageLoop* message_loop);

  virtual MessageLoop* message_loop();

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
  friend class base::RefCountedThreadSafe<MediaFilter>;
  virtual ~MediaFilter();

  FilterHost* host() const { return host_; }
  MessageLoop* message_loop() const { return message_loop_; }

 private:
  FilterHost* host_;
  MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(MediaFilter);
};


class DataSource : public MediaFilter {
 public:
  typedef Callback1<size_t>::Type ReadCallback;
  static const size_t kReadError = static_cast<size_t>(-1);

  static FilterType filter_type() {
    return FILTER_DATA_SOURCE;
  }

  static bool IsMediaFormatSupported(const MediaFormat& media_format) {
    std::string mime_type;
    return (media_format.GetAsString(MediaFormat::kMimeType, &mime_type) &&
            mime_type == mime_type::kURL);
  }

  // Initialize a DataSource for the given URL, executing the callback upon
  // completion.
  virtual void Initialize(const std::string& url, FilterCallback* callback) = 0;

  // Returns the MediaFormat for this filter.
  virtual const MediaFormat& media_format() = 0;

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


class Demuxer : public MediaFilter {
 public:
  static FilterType filter_type() {
    return FILTER_DEMUXER;
  }

  static bool IsMediaFormatSupported(const MediaFormat& media_format) {
    std::string mime_type;
    return (media_format.GetAsString(MediaFormat::kMimeType, &mime_type) &&
            mime_type == mime_type::kApplicationOctetStream);
  }

  // Initialize a Demuxer with the given DataSource, executing the callback upon
  // completion.
  virtual void Initialize(DataSource* data_source,
                          FilterCallback* callback) = 0;

  // Returns the number of streams available
  virtual size_t GetNumberOfStreams() = 0;

  // Returns the stream for the given index, NULL otherwise
  virtual scoped_refptr<DemuxerStream> GetStream(int stream_id) = 0;
};


class DemuxerStream : public base::RefCountedThreadSafe<DemuxerStream> {
 public:
  // Returns the MediaFormat for this filter.
  virtual const MediaFormat& media_format() = 0;

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

  virtual void EnableBitstreamConverter() = 0;

 protected:
  // Optional method that is implemented by filters that support extended
  // interfaces.  The filter should return a pointer to the interface
  // associated with the |interface_id| string if they support it, otherwise
  // return NULL to indicate the interface is unknown.  The derived filter
  // should NOT AddRef() the interface.  The DemuxerStream::QueryInterface()
  // public template function will assign the interface to a scoped_refptr<>.
  virtual void* QueryInterface(const char* interface_id) { return NULL; }

  friend class base::RefCountedThreadSafe<DemuxerStream>;
  virtual ~DemuxerStream();
};


class VideoDecoder : public MediaFilter {
 public:
  static FilterType filter_type() {
    return FILTER_VIDEO_DECODER;
  }

  static const char* major_mime_type() {
    return mime_type::kMajorTypeVideo;
  }

  // Initialize a VideoDecoder with the given DemuxerStream, executing the
  // callback upon completion.
  virtual void Initialize(DemuxerStream* stream, FilterCallback* callback) = 0;

  // Returns the MediaFormat for this filter.
  virtual const MediaFormat& media_format() = 0;

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

 protected:
  // A video frame is ready to be consumed. This method invoke
  // |consume_video_frame_callback_| internally.
  void VideoFrameReady(scoped_refptr<VideoFrame> frame) {
    consume_video_frame_callback_->Run(frame);
  }

 private:
  scoped_ptr<ConsumeVideoFrameCallback> consume_video_frame_callback_;
};


class AudioDecoder : public MediaFilter {
 public:
  static FilterType filter_type() {
    return FILTER_AUDIO_DECODER;
  }

  static const char* major_mime_type() {
    return mime_type::kMajorTypeAudio;
  }

  // Initialize a AudioDecoder with the given DemuxerStream, executing the
  // callback upon completion.
  virtual void Initialize(DemuxerStream* stream, FilterCallback* callback) = 0;

  // Returns the MediaFormat for this filter.
  virtual const MediaFormat& media_format() = 0;

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

 private:
  scoped_ptr<ConsumeAudioSamplesCallback> consume_audio_samples_callback_;
};


class VideoRenderer : public MediaFilter {
 public:
  static FilterType filter_type() {
    return FILTER_VIDEO_RENDERER;
  }

  static const char* major_mime_type() {
    return mime_type::kMajorTypeVideo;
  }

  // Initialize a VideoRenderer with the given VideoDecoder, executing the
  // callback upon completion.
  virtual void Initialize(VideoDecoder* decoder, FilterCallback* callback) = 0;

  // Returns true if this filter has received and processed an end-of-stream
  // buffer.
  virtual bool HasEnded() = 0;
};


class AudioRenderer : public MediaFilter {
 public:
  static FilterType filter_type() {
    return FILTER_AUDIO_RENDERER;
  }

  static const char* major_mime_type() {
    return mime_type::kMajorTypeAudio;
  }

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
