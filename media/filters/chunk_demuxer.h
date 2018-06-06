// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_CHUNK_DEMUXER_H_
#define MEDIA_FILTERS_CHUNK_DEMUXER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/synchronization/lock.h"
#include "media/base/byte_queue.h"
#include "media/base/demuxer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_tracks.h"
#include "media/base/ranges.h"
#include "media/base/stream_parser.h"
#include "media/filters/source_buffer_parse_warnings.h"
#include "media/filters/source_buffer_range_by_dts.h"
#include "media/filters/source_buffer_range_by_pts.h"
#include "media/filters/source_buffer_state.h"
#include "media/filters/source_buffer_stream.h"

namespace media {

class MEDIA_EXPORT ChunkDemuxerStream : public DemuxerStream {
 public:
  using BufferQueue = base::circular_deque<scoped_refptr<StreamParserBuffer>>;

  enum class RangeApi { kLegacyByDts, kNewByPts };

  ChunkDemuxerStream(Type type,
                     MediaTrack::Id media_track_id,
                     RangeApi range_api);
  ~ChunkDemuxerStream() override;

  // ChunkDemuxerStream control methods.
  void StartReturningData();
  void AbortReads();
  void CompletePendingReadIfPossible();
  void Shutdown();

  // SourceBufferStream manipulation methods.
  void Seek(base::TimeDelta time);
  bool IsSeekWaitingForData() const;

  // Add buffers to this stream.  Buffers are stored in SourceBufferStreams,
  // which handle ordering and overlap resolution.
  // Returns true if buffers were successfully added.
  bool Append(const StreamParser::BufferQueue& buffers);

  // Removes buffers between |start| and |end| according to the steps
  // in the "Coded Frame Removal Algorithm" in the Media Source
  // Extensions Spec.
  // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-coded-frame-removal
  //
  // |duration| is the current duration of the presentation. It is
  // required by the computation outlined in the spec.
  void Remove(base::TimeDelta start, base::TimeDelta end,
              base::TimeDelta duration);

  // If the buffer is full, attempts to try to free up space, as specified in
  // the "Coded Frame Eviction Algorithm" in the Media Source Extensions Spec.
  // Returns false iff buffer is still full after running eviction.
  // https://w3c.github.io/media-source/#sourcebuffer-coded-frame-eviction
  bool EvictCodedFrames(base::TimeDelta media_time, size_t newDataSize);

  void OnMemoryPressure(
      DecodeTimestamp media_time,
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level,
      bool force_instant_gc);

  // Signal to the stream that duration has changed to |duration|.
  void OnSetDuration(base::TimeDelta duration);

  // Returns the range of buffered data in this stream, capped at |duration|.
  Ranges<base::TimeDelta> GetBufferedRanges(base::TimeDelta duration) const;

  // Returns the highest PTS of the buffered data.
  // Returns base::TimeDelta() if the stream has no buffered data.
  base::TimeDelta GetHighestPresentationTimestamp() const;

  // Returns the duration of the buffered data.
  // Returns base::TimeDelta() if the stream has no buffered data.
  base::TimeDelta GetBufferedDuration() const;

  // Returns the size of the buffered data in bytes.
  size_t GetBufferedSize() const;

  // Signal to the stream that buffers handed in through subsequent calls to
  // Append() belong to a coded frame group that starts at |start_dts| and
  // |start_pts|.
  void OnStartOfCodedFrameGroup(DecodeTimestamp start_dts,
                                base::TimeDelta start_pts);

  // Called when midstream config updates occur.
  // Returns true if the new config is accepted.
  // Returns false if the new config should trigger an error.
  bool UpdateAudioConfig(const AudioDecoderConfig& config, MediaLog* media_log);
  bool UpdateVideoConfig(const VideoDecoderConfig& config, MediaLog* media_log);
  void UpdateTextConfig(const TextTrackConfig& config, MediaLog* media_log);

  void MarkEndOfStream();
  void UnmarkEndOfStream();

  // DemuxerStream methods.
  void Read(const ReadCB& read_cb) override;
  Type type() const override;
  Liveness liveness() const override;
  AudioDecoderConfig audio_decoder_config() override;
  VideoDecoderConfig video_decoder_config() override;
  bool SupportsConfigChanges() override;

  bool IsEnabled() const;
  void SetEnabled(bool enabled, base::TimeDelta timestamp);

  // Returns the text track configuration.  It is an error to call this method
  // if type() != TEXT.
  TextTrackConfig text_track_config();

  // Sets the memory limit, in bytes, on the SourceBufferStream.
  void SetStreamMemoryLimit(size_t memory_limit);

  bool supports_partial_append_window_trimming() const {
    return partial_append_window_trimming_enabled_;
  }

  void SetLiveness(Liveness liveness);

  MediaTrack::Id media_track_id() const { return media_track_id_; }

 private:
  enum State {
    UNINITIALIZED,
    RETURNING_DATA_FOR_READS,
    RETURNING_ABORT_FOR_READS,
    SHUTDOWN,
  };

  // Assigns |state_| to |state|
  void ChangeState_Locked(State state);

  void CompletePendingReadIfPossible_Locked();

  // Specifies the type of the stream.
  Type type_;
  const RangeApi range_api_;

  Liveness liveness_;

  // Precisely one of these will be used by an instance, determined by
  // |range_api_| set in ctor. See https://crbug.com/718641.
  std::unique_ptr<SourceBufferStream<SourceBufferRangeByDts>> stream_dts_;
  std::unique_ptr<SourceBufferStream<SourceBufferRangeByPts>> stream_pts_;

  const MediaTrack::Id media_track_id_;

  mutable base::Lock lock_;
  State state_;
  ReadCB read_cb_;
  bool partial_append_window_trimming_enabled_;
  bool is_enabled_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChunkDemuxerStream);
};

// Demuxer implementation that allows chunks of media data to be passed
// from JavaScript to the media stack.
class MEDIA_EXPORT ChunkDemuxer : public Demuxer {
 public:
  enum Status {
    kOk,              // ID added w/o error.
    kNotSupported,    // Type specified is not supported.
    kReachedIdLimit,  // Reached ID limit. We can't handle any more IDs.
  };

  // |open_cb| Run when Initialize() is called to signal that the demuxer
  //   is ready to receive media data via AppendData().
  // |progress_cb| Run each time data is appended.
  // |encrypted_media_init_data_cb| Run when the demuxer determines that an
  //   encryption key is needed to decrypt the content.
  // |media_log| Used to report content and engine debug messages.
  ChunkDemuxer(const base::Closure& open_cb,
               const base::Closure& progress_cb,
               const EncryptedMediaInitDataCB& encrypted_media_init_data_cb,
               MediaLog* media_log);
  ~ChunkDemuxer() override;

  // Demuxer implementation.
  std::string GetDisplayName() const override;

  // |enable_text| Process inband text tracks in the normal way when true,
  //   otherwise ignore them.
  void Initialize(DemuxerHost* host, const PipelineStatusCB& init_cb) override;
  void Stop() override;
  void Seek(base::TimeDelta time, const PipelineStatusCB& cb) override;
  base::Time GetTimelineOffset() const override;
  std::vector<DemuxerStream*> GetAllStreams() override;
  base::TimeDelta GetStartTime() const override;
  int64_t GetMemoryUsage() const override;
  void AbortPendingReads() override;

  // ChunkDemuxer reads are abortable. StartWaitingForSeek() and
  // CancelPendingSeek() always abort pending and future reads until the
  // expected seek occurs, so that ChunkDemuxer can stay synchronized with the
  // associated JS method calls.
  void StartWaitingForSeek(base::TimeDelta seek_time) override;
  void CancelPendingSeek(base::TimeDelta seek_time) override;

  // Registers a new |id| to use for AppendData() calls. |type| indicates
  // the MIME type for the data that we intend to append for this ID.
  // kOk is returned if the demuxer has enough resources to support another ID
  //    and supports the format indicated by |type|.
  // kNotSupported is returned if |type| is not a supported format.
  // kReachedIdLimit is returned if the demuxer cannot handle another ID right
  //    now.
  Status AddId(const std::string& id,
               const std::string& type,
               const std::string& codecs);

  // Notifies a caller via |tracks_updated_cb| that the set of media tracks
  // for a given |id| has changed.
  void SetTracksWatcher(const std::string& id,
                        const MediaTracksUpdatedCB& tracks_updated_cb);

  // Notifies a caller via |parse_warning_cb| of a parse warning.
  void SetParseWarningCallback(
      const std::string& id,
      const SourceBufferParseWarningCB& parse_warning_cb);

  // Removed an ID & associated resources that were previously added with
  // AddId().
  void RemoveId(const std::string& id);

  // Gets the currently buffered ranges for the specified ID.
  Ranges<base::TimeDelta> GetBufferedRanges(const std::string& id) const;

  // Gets the highest buffered PTS for the specified |id|. If there is nothing
  // buffered, returns base::TimeDelta().
  base::TimeDelta GetHighestPresentationTimestamp(const std::string& id) const;

  void OnEnabledAudioTracksChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;

  void OnSelectedVideoTrackChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;

  // Appends media data to the source buffer associated with |id|, applying
  // and possibly updating |*timestamp_offset| during coded frame processing.
  // |append_window_start| and |append_window_end| correspond to the MSE spec's
  // similarly named source buffer attributes that are used in coded frame
  // processing. Returns true on success, false if the caller needs to run the
  // append error algorithm with decode error parameter set to true.
  bool AppendData(const std::string& id,
                  const uint8_t* data,
                  size_t length,
                  base::TimeDelta append_window_start,
                  base::TimeDelta append_window_end,
                  base::TimeDelta* timestamp_offset);

  // Aborts parsing the current segment and reset the parser to a state where
  // it can accept a new segment.
  // Some pending frames can be emitted during that process. These frames are
  // applied |timestamp_offset|.
  void ResetParserState(const std::string& id,
                        base::TimeDelta append_window_start,
                        base::TimeDelta append_window_end,
                        base::TimeDelta* timestamp_offset);

  // Remove buffers between |start| and |end| for the source buffer
  // associated with |id|.
  void Remove(const std::string& id, base::TimeDelta start,
              base::TimeDelta end);

  // If the buffer is full, attempts to try to free up space, as specified in
  // the "Coded Frame Eviction Algorithm" in the Media Source Extensions Spec.
  // Returns false iff buffer is still full after running eviction.
  // https://w3c.github.io/media-source/#sourcebuffer-coded-frame-eviction
  bool EvictCodedFrames(const std::string& id,
                        base::TimeDelta currentMediaTime,
                        size_t newDataSize);

  void OnMemoryPressure(
      base::TimeDelta currentMediaTime,
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level,
      bool force_instant_gc);

  // Returns the current presentation duration.
  double GetDuration();
  double GetDuration_Locked();

  // Notifies the demuxer that the duration of the media has changed to
  // |duration|.
  void SetDuration(double duration);

  // Returns true if the source buffer associated with |id| is currently parsing
  // a media segment, or false otherwise.
  bool IsParsingMediaSegment(const std::string& id);

  // Returns the 'Generate Timestamps Flag', as described in the MSE Byte Stream
  // Format Registry, for the source buffer associated with |id|.
  bool GetGenerateTimestampsFlag(const std::string& id);

  // Set the append mode to be applied to subsequent buffers appended to the
  // source buffer associated with |id|. If |sequence_mode| is true, caller
  // is requesting "sequence" mode. Otherwise, caller is requesting "segments"
  // mode.
  void SetSequenceMode(const std::string& id, bool sequence_mode);

  // Signals the coded frame processor for the source buffer associated with
  // |id| to update its group start timestamp to be |timestamp_offset| if it is
  // in sequence append mode.
  void SetGroupStartTimestampIfInSequenceMode(const std::string& id,
                                              base::TimeDelta timestamp_offset);

  // Called to signal changes in the "end of stream"
  // state. UnmarkEndOfStream() must not be called if a matching
  // MarkEndOfStream() has not come before it.
  void MarkEndOfStream(PipelineStatus status);
  void UnmarkEndOfStream();

  void Shutdown();

  // Sets the memory limit on each stream of a specific type.
  // |memory_limit| is the maximum number of bytes each stream of type |type|
  // is allowed to hold in its buffer.
  void SetMemoryLimitsForTest(DemuxerStream::Type type, size_t memory_limit);

  // Returns the ranges representing the buffered data in the demuxer.
  // TODO(wolenetz): Remove this method once MediaSourceDelegate no longer
  // requires it for doing hack browser seeks to I-frame on Android. See
  // http://crbug.com/304234.
  Ranges<base::TimeDelta> GetBufferedRanges() const;

 private:
  enum State {
    WAITING_FOR_INIT = 0,
    INITIALIZING,
    INITIALIZED,
    ENDED,

    // Any State at or beyond PARSE_ERROR cannot be changed to a state before
    // this. See ChangeState_Locked.
    PARSE_ERROR,
    SHUTDOWN,
  };

  // Helper for vide and audio track changing.
  void FindAndEnableProperTracks(const std::vector<MediaTrack::Id>& track_ids,
                                 base::TimeDelta curr_time,
                                 DemuxerStream::Type track_type,
                                 TrackChangeCB change_completed_cb);

  void ChangeState_Locked(State new_state);

  // Reports an error and puts the demuxer in a state where it won't accept more
  // data.
  void ReportError_Locked(PipelineStatus error);

  // Returns true if any stream has seeked to a time without buffered data.
  bool IsSeekWaitingForData_Locked() const;

  // Returns true if all streams can successfully call EndOfStream,
  // false if any can not.
  bool CanEndOfStream_Locked() const;

  // SourceBufferState callbacks.
  void OnSourceInitDone(const std::string& source_id,
                        const StreamParser::InitParameters& params);

  // Creates a DemuxerStream of the specified |type| for the SourceBufferState
  // with the given |source_id|.
  // Returns a pointer to a new ChunkDemuxerStream instance, which is owned by
  // ChunkDemuxer.
  ChunkDemuxerStream* CreateDemuxerStream(const std::string& source_id,
                                          DemuxerStream::Type type);

  void OnNewTextTrack(ChunkDemuxerStream* text_stream,
                      const TextTrackConfig& config);

  // Returns true if |source_id| is valid, false otherwise.
  bool IsValidId(const std::string& source_id) const;

  // Increases |duration_| to |new_duration|, if |new_duration| is higher.
  void IncreaseDurationIfNecessary(base::TimeDelta new_duration);

  // Decreases |duration_| if the buffered region is less than |duration_| when
  // EndOfStream() is called.
  void DecreaseDurationIfNecessary();

  // Sets |duration_| to |new_duration|, sets |user_specified_duration_| to -1
  // and notifies |host_|.
  void UpdateDuration(base::TimeDelta new_duration);

  // Returns the ranges representing the buffered data in the demuxer.
  Ranges<base::TimeDelta> GetBufferedRanges_Locked() const;

  // Start returning data on all DemuxerStreams.
  void StartReturningData();

  void AbortPendingReads_Locked();

  // Completes any pending reads if it is possible to do so.
  void CompletePendingReadsIfPossible();

  // Seeks all SourceBufferStreams to |seek_time|.
  void SeekAllSources(base::TimeDelta seek_time);

  // Generates and returns a unique media track id.
  static MediaTrack::Id GenerateMediaTrackId();

  // Shuts down all DemuxerStreams by calling Shutdown() on
  // all objects in |source_state_map_|.
  void ShutdownAllStreams();

  mutable base::Lock lock_;
  State state_;
  bool cancel_next_seek_;

  DemuxerHost* host_;
  base::Closure open_cb_;
  base::Closure progress_cb_;
  EncryptedMediaInitDataCB encrypted_media_init_data_cb_;

  // MediaLog for reporting messages and properties to debug content and engine.
  MediaLog* media_log_;

  PipelineStatusCB init_cb_;
  // Callback to execute upon seek completion.
  // TODO(wolenetz/acolwell): Protect against possible double-locking by first
  // releasing |lock_| before executing this callback. See
  // http://crbug.com/308226
  PipelineStatusCB seek_cb_;

  using OwnedChunkDemuxerStreamVector =
      std::vector<std::unique_ptr<ChunkDemuxerStream>>;
  OwnedChunkDemuxerStreamVector audio_streams_;
  OwnedChunkDemuxerStreamVector video_streams_;
  OwnedChunkDemuxerStreamVector text_streams_;

  // Keep track of which ids still remain uninitialized so that we transition
  // into the INITIALIZED only after all ids/SourceBuffers got init segment.
  std::set<std::string> pending_source_init_ids_;

  base::TimeDelta duration_;

  // The duration passed to the last SetDuration(). If
  // SetDuration() is never called or an AppendData() call or
  // a EndOfStream() call changes |duration_|, then this
  // variable is set to < 0 to indicate that the |duration_| represents
  // the actual duration instead of a user specified value.
  double user_specified_duration_;

  base::Time timeline_offset_;
  DemuxerStream::Liveness liveness_;

  std::map<std::string, std::unique_ptr<SourceBufferState>> source_state_map_;

  std::map<std::string, std::vector<ChunkDemuxerStream*>> id_to_streams_map_;
  // Used to hold alive the demuxer streams that were created for removed /
  // released SourceBufferState objects. Demuxer clients might still have
  // references to these streams, so we need to keep them alive. But they'll be
  // in a shut down state, so reading from them will return EOS.
  std::vector<std::unique_ptr<ChunkDemuxerStream>> removed_streams_;

  // Accumulate, by type, detected track counts across the SourceBuffers.
  int detected_audio_track_count_;
  int detected_video_track_count_;
  int detected_text_track_count_;

  // Caches whether |media::kMseBufferByPts| feature was enabled at ChunkDemuxer
  // construction time. This makes sure that all buffering for this ChunkDemuxer
  // uses the same behavior. See https://crbug.com/718641.
  const bool buffering_by_pts_;

  std::map<MediaTrack::Id, ChunkDemuxerStream*> track_id_to_demux_stream_map_;

  DISALLOW_COPY_AND_ASSIGN(ChunkDemuxer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_CHUNK_DEMUXER_H_
