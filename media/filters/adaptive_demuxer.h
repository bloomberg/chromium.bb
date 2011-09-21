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
#include "media/base/demuxer.h"
#include "media/base/pipeline.h"

namespace media {

class AdaptiveDemuxer;
class StreamSwitchManager;

class AdaptiveDemuxerStream : public DemuxerStream {
 public:
  typedef std::vector<scoped_refptr<DemuxerStream> > StreamVector;
  typedef std::deque<ReadCallback> ReadCBQueue;

  // Typedefs used by the stream switching code to seek a stream to a desired
  // location. The AdaptiveDemuxer constructs a SeekFunction for the stream
  // being switched to and passes this function to
  // AdaptiveDemuxerStream::ChangeCurrentStream(). For more information about
  // how the SeekFunction is implemented, look at the comments for
  // AdaptiveDemuxer::StartStreamSwitchSeek() below.
  //
  // Callback for SeekFunction. The PipelineStatus parameter indicates whether
  // the seek was successful or not. The base::TimeDelta parameter indicates the
  // time we actually seeked to. If the seek was successful this should be >=
  // than the requested time.
  typedef base::Callback<void(base::TimeDelta, PipelineStatus)> SeekFunctionCB;

  // Wraps a function that performs a seek on a specified stream. The
  // base::TimeDelta parameter indicates what time to seek to. The
  // SeekFunctionCBparameter specifies the callback we want called when the seek
  // completes.
  typedef base::Callback<void(base::TimeDelta, SeekFunctionCB)> SeekFunction;

  // Keeps references to the passed-in streams.  |streams| must be non-empty and
  // all the streams in it must agree on type.
  //
  // |initial_stream| must be a valid index into |streams| and specifies the
  // current stream on construction.
  AdaptiveDemuxerStream(StreamVector const& streams, int initial_stream);
  virtual ~AdaptiveDemuxerStream();

  // Notifies this stream that a Seek() was requested on the demuxer.
  void OnAdaptiveDemuxerSeek(base::TimeDelta seek_time);

  // Change the stream to satisfy subsequent Read() requests from.  The
  // referenced pointer must not be NULL. The SeekFunction parameter
  // provides a way to seek |streams_[index]| to a specific time.
  void ChangeCurrentStream(int index, const SeekFunction& seek_function,
                           const PipelineStatusCB& cb);

  // DemuxerStream methods.
  virtual void Read(const ReadCallback& read_callback);
  virtual Type type();
  virtual void EnableBitstreamConverter();
  virtual AVStream* GetAVStream();
  virtual const AudioDecoderConfig& audio_decoder_config();

 private:
  // Returns a pointer to the current stream.
  DemuxerStream* current_stream();

  // DEBUG_MODE-only CHECK that the data members are in a reasonable state.
  void DCheckSanity();

  void OnReadDone(Buffer* buffer);

  bool IsSwitchPending_Locked() const;
  bool CanStartSwitch_Locked() const;

  // Starts the stream switch. This method expects that no Read()s are
  // outstanding on |streams_[current_stream_index_]|.
  void StartSwitch();

  // Called by the SeekFunction when it has completed the seek on
  // |streams_[switch_index_]|.
  void OnSwitchSeekDone(base::TimeDelta seek_time, PipelineStatus status);

  StreamVector streams_;
  // Guards the members below.  Only held for simple variable reads/writes, not
  // during async operation.
  base::Lock lock_;
  int current_stream_index_;
  bool bitstream_converter_enabled_;

  // The number of outstanding Read()'s on |streams_[current_stream_index_]|.
  int pending_reads_;

  // A queue of callback objects passed to Read().
  ReadCBQueue read_cb_queue_;

  // Callback passed to ChangeCurrentStream(). This is called & reset when the
  // stream switch has completed. Callback object is null when a stream switch
  // is not in progress.
  PipelineStatusCB switch_cb_;

  // The index of the stream we are switching to. During a stream switch
  // 0 <= |switch_index_| < |streams_.size()| and
  // |streams_[switch_index]| MUST NOT be null. |switch_index_| is set to -1 if
  // a stream switch is not in progress.
  int switch_index_;

  // Function used to seek |streams_[switch_index_]| to a specific time. The
  // callback is null if a switch is not pending.
  SeekFunction switch_seek_function_;

  // The timestamp of the last buffer returned via a Read() callback.
  base::TimeDelta last_buffer_timestamp_;

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

  // Change to a different video stream.
  void ChangeVideoStream(int video_id, const PipelineStatusCB& done_cb);

  // Get the ID of the current video stream.
  int GetCurrentVideoId() const;

  // Demuxer implementation.
  virtual void Stop(FilterCallback* callback) OVERRIDE;
  virtual void Seek(base::TimeDelta time, const PipelineStatusCB&  cb) OVERRIDE;
  virtual void OnAudioRendererDisabled() OVERRIDE;
  virtual void set_host(FilterHost* filter_host) OVERRIDE;
  virtual void SetPlaybackRate(float playback_rate) OVERRIDE;
  virtual void SetPreload(Preload preload) OVERRIDE;
  virtual scoped_refptr<DemuxerStream> GetStream(
      DemuxerStream::Type type) OVERRIDE;
  virtual base::TimeDelta GetStartTime() const OVERRIDE;

 private:
  // Returns a pointer to the currently active demuxer of the given type.
  Demuxer* current_demuxer(DemuxerStream::Type type);

  // Called when the AdaptiveDemuxerStream completes a stream switch.
  void ChangeVideoStreamDone(int new_stream_index,
                             const PipelineStatusCB& done_cb,
                             PipelineStatus status);

  // Methods for SeekFunction.
  //
  // Seeking on a stream follows the flow below.
  //
  // SeekFunction.Run() -> StartStreamSwitchSeek()
  //                              |
  //                              V
  //                      +--------------+
  //                      | Found Switch |
  //                      |  Point in    | No  (Seeking to get more index data.)
  //                      | Index Data?  |---> |demuxers_[stream_index]->Seek()|
  //                      +--------------+                    |
  //                              |Yes                        V
  //                              |                   OnIndexSeekDone()
  //                              |                           |
  //                              V                           V
  //            (Seeking stream to switch point.)  Yes  +--------------+
  //           |demuxers_[stream_index]->Seek()| <------| Found Switch |
  //                              |                     |  Point in    |
  //                              |                     | Index Data?  |
  //                              |                     +--------------+
  //                              |                           | No
  //                              V                           V
  //                        OnStreamSeekDone()         seek_cb(ERROR)
  //                              |
  //                              V
  //                           seek_cb(OK | ERROR)
  //
  // When AdaptiveDemuxer creates a SeekFunction object it points to this
  // method.
  void StartStreamSwitchSeek(
      DemuxerStream::Type type,
      int stream_index,
      base::TimeDelta seek_time,
      const AdaptiveDemuxerStream::SeekFunctionCB& seek_cb);

  // Called when the seek for index data initiated by StartStreamSwitchSeek()
  // completes.
  void OnIndexSeekDone(DemuxerStream::Type type,
                       int stream_index,
                       base::TimeDelta seek_time,
                       const AdaptiveDemuxerStream::SeekFunctionCB& seek_cb,
                       PipelineStatus status);

  // Called when the stream seek initiated by StartStreamSwitchSeek() or
  // OnIndexSeekDone() completes.
  static void OnStreamSeekDone(
      const AdaptiveDemuxerStream::SeekFunctionCB& seek_cb,
      base::TimeDelta seek_point,
      PipelineStatus status);

  DemuxerVector demuxers_;
  scoped_refptr<AdaptiveDemuxerStream> audio_stream_;
  scoped_refptr<AdaptiveDemuxerStream> video_stream_;
  // Guards the members below.  Only held for simple variable reads/writes, not
  // during async operation.
  mutable base::Lock lock_;
  int current_audio_demuxer_index_;
  int current_video_demuxer_index_;
  float playback_rate_;
  bool switch_pending_;
  scoped_refptr<StreamSwitchManager> stream_switch_manager_;

  // This is used to set the starting clock value to match the lowest
  // timestamp of all |demuxers_|.
  base::TimeDelta start_time_;

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
class MEDIA_EXPORT AdaptiveDemuxerFactory : public DemuxerFactory {
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

// See AdaptiveDemuxerFactory's class-level comment for |url|'s format.
MEDIA_EXPORT bool ParseAdaptiveUrl(
    const std::string& url, int* audio_index, int* video_index,
    std::vector<std::string>* urls);

}  // namespace media

#endif  // MEDIA_FILTERS_ADAPTIVE_DEMUXER_H_
