// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef MKVMUXER_HPP
#define MKVMUXER_HPP

#include "mkvmuxertypes.hpp"

// For a description of the WebM elements see
// http://www.webmproject.org/code/specs/container/.

namespace mkvmuxer {

class MkvWriter;

///////////////////////////////////////////////////////////////
// Interface used by the mkvmuxer to write out the Mkv data.
class IMkvWriter {
 public:
  // Writes out |len| bytes of |buf|. Returns 0 on success.
  virtual int32 Write(const void* buf, uint32 len) = 0;

  // Returns the offset of the output position from the beginning of the
  // output.
  virtual int64 Position() const = 0;

  // Set the current File position. Returns 0 on success.
  virtual int32 Position(int64 position) = 0;

  // Returns true if the writer is seekable.
  virtual bool Seekable() const = 0;

  // Element start notification. Called whenever an element identifier is about
  // to be written to the stream. |element_id| is the element identifier, and
  // |position| is the location in the WebM stream where the first octet of the
  // element identifier will be written.
  // Note: the |MkvId| enumeration in webmids.hpp defines element values.
  virtual void ElementStartNotify(uint64 element_id, int64 position) = 0;

 protected:
  IMkvWriter();
  virtual ~IMkvWriter();

 private:
  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(IMkvWriter);
};

// Writes out the EBML header for a WebM file. This function must be called
// before any other libwebm writing functions are called.
bool WriteEbmlHeader(IMkvWriter* writer);

///////////////////////////////////////////////////////////////
// Class to hold data the will be written to a block.
class Frame {
 public:
  Frame();
  ~Frame();

  // Copies |frame| data into |frame_|. Returns true on success.
  bool Init(const uint8* frame, uint64 length);

  const uint8* frame() const { return frame_; }
  uint64 length() const { return length_; }
  void set_track_number(uint64 track_number) { track_number_ = track_number; }
  uint64 track_number() const { return track_number_; }
  void set_timestamp(uint64 timestamp) { timestamp_ = timestamp; }
  uint64 timestamp() const { return timestamp_; }
  void set_is_key(bool key) { is_key_ = key; }
  bool is_key() const { return is_key_; }

 private:
  // Pointer to the data. Owned by this class.
  uint8* frame_;

  // Length of the data.
  uint64 length_;

  // Mkv track number the data is associated with.
  uint64 track_number_;

  // Timestamp of the data in nanoseconds.
  uint64 timestamp_;

  // Flag telling if the data should set the key flag of a block.
  bool is_key_;
};

///////////////////////////////////////////////////////////////
// Class to hold one cue point in a Cues element.
class CuePoint {
 public:
  CuePoint();
  ~CuePoint();

  // Returns the size in bytes for the entire CuePoint element.
  uint64 Size() const;

  // Output the CuePoint element to the writer. Returns true on success.
  bool Write(IMkvWriter* writer) const;

  void set_time(uint64 time) { time_ = time; }
  uint64 time() const { return time_; }
  void set_track(uint64 track) { track_ = track; }
  uint64 track() const { return track_; }
  void set_cluster_pos(uint64 cluster_pos) { cluster_pos_ = cluster_pos; }
  uint64 cluster_pos() const { return cluster_pos_; }
  void set_block_number(uint64 block_number) { block_number_ = block_number; }
  uint64 block_number() const { return block_number_; }
  void set_output_block_number(bool output_block_number) {
    output_block_number_ = output_block_number;
  }
  bool output_block_number() const { return output_block_number_; }

 private:
  // Returns the size in bytes for the payload of the CuePoint element.
  uint64 PayloadSize() const;

  // Absolute timecode according to the segment time base.
  uint64 time_;

  // The Track element associated with the CuePoint.
  uint64 track_;

  // The position of the Cluster containing the Block.
  uint64 cluster_pos_;

  // Number of the Block within the Cluster, starting from 1.
  uint64 block_number_;

  // If true the muxer will write out the block number for the cue if the
  // block number is different than the default of 1. Default is set to true.
  bool output_block_number_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(CuePoint);
};

///////////////////////////////////////////////////////////////
// Cues element.
class Cues {
 public:
  Cues();
  ~Cues();

  // Adds a cue point to the Cues element. Returns true on success.
  bool AddCue(CuePoint* cue);

  // Returns the cue point by index. Returns NULL if there is no cue point
  // match.
  const CuePoint* GetCueByIndex(int32 index) const;

  // Output the Cues element to the writer. Returns true on success.
  bool Write(IMkvWriter* writer) const;

  int32 cue_entries_size() const { return cue_entries_size_; }
  void set_output_block_number(bool output_block_number) {
    output_block_number_ = output_block_number;
  }
  bool output_block_number() const { return output_block_number_; }

 private:
  // Number of allocated elements in |cue_entries_|.
  int32 cue_entries_capacity_;

  // Number of CuePoints in |cue_entries_|.
  int32 cue_entries_size_;

  // CuePoint list.
  CuePoint** cue_entries_;

  // If true the muxer will write out the block number for the cue if the
  // block number is different than the default of 1. Default is set to true.
  bool output_block_number_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(Cues);
};

///////////////////////////////////////////////////////////////
// ContentEncoding element
// Elements used to describe if the track data has been encrypted or
// compressed with zlib or header stripping.
// Currently only whole frames can only be encrypted once with AES. This
// dictates that ContentEncodingOrder will be 0, ContentEncodingScope will
// be 1, ContentEncodingType will be 1, and ContentEncAlgo will be 5.
class ContentEncoding {
 public:
  ContentEncoding();
  ~ContentEncoding();

  uint64 enc_algo() const { return enc_algo_; }
  uint64 encoding_order() const { return encoding_order_; }
  uint64 encoding_scope() const { return encoding_scope_; }
  uint64 encoding_type() const { return encoding_type_; }

  // Sets the content encryption id. Copies |length| bytes from |id| to
  // |enc_key_id_|. Returns true on success.
  bool SetEncryptionID(const uint8* id, uint64 length);

  // Returns the size in bytes for the ContentEncoding element.
  uint64 Size() const;

  // Writes out the ContentEncoding element to |writer|. Returns true on
  // success.
  bool Write(IMkvWriter* writer) const;

 private:
  // Returns the size in bytes for the encoding elements.
  uint64 EncodingSize(uint64 compresion_size, uint64 encryption_size) const;

  // Returns the size in bytes for the encryption elements.
  uint64 EncryptionSize() const;

  // Track element names
  uint64 enc_algo_;
  uint8* enc_key_id_;
  uint64 encoding_order_;
  uint64 encoding_scope_;
  uint64 encoding_type_;

  // Size of the ContentEncKeyID data in bytes.
  uint64 enc_key_id_length_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(ContentEncoding);
};

///////////////////////////////////////////////////////////////
// Track element.
class Track {
 public:
  Track();
  virtual ~Track();

  // Adds a ContentEncoding element to the Track. Returns true on success.
  virtual bool AddContentEncoding();

  // Returns the ContentEncoding by index. Returns NULL if there is no
  // ContentEncoding match.
  ContentEncoding* GetContentEncodingByIndex(uint32 index) const;

  // Returns the size in bytes for the payload of the Track element.
  virtual uint64 PayloadSize() const;

  // Returns the size in bytes of the Track element.
  virtual uint64 Size() const;

  // Output the Track element to the writer. Returns true on success.
  virtual bool Write(IMkvWriter* writer) const;

  // Sets the CodecPrivate element of the Track element. Copies |length|
  // bytes from |codec_private| to |codec_private_|. Returns true on success.
  bool SetCodecPrivate(const uint8* codec_private, uint64 length);

  void set_codec_id(const char* codec_id);
  const char* codec_id() const { return codec_id_; }
  const uint8* codec_private() const { return codec_private_; }
  void set_language(const char* language);
  const char* language() const { return language_; }
  void set_name(const char* name);
  const char* name() const { return name_; }
  void set_number(uint64 number) { number_ = number; }
  uint64 number() const { return number_; }
  void set_type(uint64 type) { type_ = type; }
  uint64 type() const { return type_; }
  uint64 uid() const { return uid_; }

  uint64 codec_private_length() const { return codec_private_length_; }
  uint32 content_encoding_entries_size() const {
    return content_encoding_entries_size_;
  }

 private:
  // Returns a random number to be used for the Track UID.
  static uint64 MakeUID();

  // Track element names
  char* codec_id_;
  uint8* codec_private_;
  char* language_;
  char* name_;
  uint64 number_;
  uint64 type_;
  const uint64 uid_;

  // Size of the CodecPrivate data in bytes.
  uint64 codec_private_length_;

  // ContentEncoding element list.
  ContentEncoding** content_encoding_entries_;

  // Number of ContentEncoding elements added.
  uint32 content_encoding_entries_size_;

  // Flag telling if the rand call was seeded.
  static bool is_seeded_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(Track);
};

///////////////////////////////////////////////////////////////
// Track that has video specific elements.
class VideoTrack : public Track {
 public:
  // Supported modes for stereo 3D.
  enum StereoMode {
    kMono = 0,
    kSideBySideLeftIsFirst  = 1,
    kTopBottomRightIsFirst  = 2,
    kTopBottomLeftIsFirst   = 3,
    kSideBySideRightIsFirst = 11
  };

  VideoTrack();
  virtual ~VideoTrack();

  // Returns the size in bytes for the payload of the Track element plus the
  // video specific elements.
  virtual uint64 PayloadSize() const;

  // Returns the size in bytes of the Track element plus the video specific
  // elements.
  virtual uint64 Size() const;

  // Output the VideoTrack element to the writer. Returns true on success.
  virtual bool Write(IMkvWriter* writer) const;

  // Sets the video's stereo mode. Returns true on success.
  bool SetStereoMode(uint64 stereo_mode);

  void set_display_height(uint64 height) { display_height_ = height; }
  uint64 display_height() const { return display_height_; }
  void set_display_width(uint64 width) { display_width_ = width; }
  uint64 display_width() const { return display_width_; }
  void set_frame_rate(double frame_rate) { frame_rate_ = frame_rate; }
  double frame_rate() const { return frame_rate_; }
  void set_height(uint64 height) { height_ = height; }
  uint64 height() const { return height_; }
  uint64 stereo_mode() { return stereo_mode_; }
  void set_width(uint64 width) { width_ = width; }
  uint64 width() const { return width_; }

 private:
  // Returns the size in bytes of the Video element.
  uint64 VideoPayloadSize() const;

  // Video track element names.
  uint64 display_height_;
  uint64 display_width_;
  double frame_rate_;
  uint64 height_;
  uint64 stereo_mode_;
  uint64 width_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(VideoTrack);
};

///////////////////////////////////////////////////////////////
// Track that has audio specific elements.
class AudioTrack : public Track {
 public:
  AudioTrack();
  virtual ~AudioTrack();

  // Returns the size in bytes for the payload of the Track element plus the
  // audio specific elements.
  virtual uint64 PayloadSize() const;

  // Returns the size in bytes of the Track element plus the audio specific
  // elements.
  virtual uint64 Size() const;

  // Output the AudioTrack element to the writer. Returns true on success.
  virtual bool Write(IMkvWriter* writer) const;

  void set_bit_depth(uint64 bit_depth) { bit_depth_ = bit_depth; }
  uint64 bit_depth() const { return bit_depth_; }
  void set_channels(uint64 channels) { channels_ = channels; }
  uint64 channels() const { return channels_; }
  void set_sample_rate(double sample_rate) { sample_rate_ = sample_rate; }
  double sample_rate() const { return sample_rate_; }

 private:
  // Audio track element names.
  uint64 bit_depth_;
  uint64 channels_;
  double sample_rate_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(AudioTrack);
};

///////////////////////////////////////////////////////////////
// Tracks element
class Tracks {
 public:
  // Audio and video type defined by the Matroska specs.
  enum {
    kVideo = 0x1,
    kAudio = 0x2
  };
  // Vorbis and VP8 coded id defined by the Matroska specs.
  static const char* const kVorbisCodecId;
  static const char* const kVp8CodecId;

  Tracks();
  ~Tracks();

  // Adds a Track element to the Tracks object. |track| will be owned and
  // deleted by the Tracks object. Returns true on success. |number| is the
  // number to use for the track. |number| must be >= 0. If |number| == 0
  // then the muxer will decide on the track number.
  bool AddTrack(Track* track, int32 number);

  // Returns the track by index. Returns NULL if there is no track match.
  const Track* GetTrackByIndex(uint32 idx) const;

  // Search the Tracks and return the track that matches |tn|. Returns NULL
  // if there is no track match.
  Track* GetTrackByNumber(uint64 track_number) const;

  // Returns true if the track number is an audio track.
  bool TrackIsAudio(uint64 track_number) const;

  // Returns true if the track number is a video track.
  bool TrackIsVideo(uint64 track_number) const;

  // Output the Tracks element to the writer. Returns true on success.
  bool Write(IMkvWriter* writer) const;

  uint32 track_entries_size() const { return track_entries_size_; }

 private:
  // Track element list.
  Track** track_entries_;

  // Number of Track elements added.
  uint32 track_entries_size_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(Tracks);
};

///////////////////////////////////////////////////////////////
// Cluster element
//
// Notes:
//  |Init| must be called before any other method in this class.
class Cluster {
 public:
  Cluster(uint64 timecode, int64 cues_pos);
  ~Cluster();

  // |timecode| is the absolute timecode of the cluster. |cues_pos| is the
  // position for the cluster within the segment that should be written in
  // the cues element.
  bool Init(IMkvWriter* ptr_writer);

  // Adds a frame to be output in the file. The frame is written out through
  // |writer_| if successful. Returns true on success.
  // Inputs:
  //   frame: Pointer to the data
  //   length: Length of the data
  //   track_number: Track to add the data to. Value returned by Add track
  //                 functions.
  //   timestamp:    Timecode of the frame relative to the cluster timecode.
  //   is_key:       Flag telling whether or not this frame is a key frame.
  bool AddFrame(const uint8* frame,
                uint64 length,
                uint64 track_number,
                short timecode,
                bool is_key);

  // Increments the size of the cluster's data in bytes.
  void AddPayloadSize(uint64 size);

  // Closes the cluster so no more data can be written to it. Will update the
  // cluster's size if |writer_| is seekable. Returns true on success.
  bool Finalize();

  // Returns the size in bytes for the entire Cluster element.
  uint64 Size() const;

  int32 blocks_added() const { return blocks_added_; }
  uint64 payload_size() const { return payload_size_; }
  int64 position_for_cues() const { return position_for_cues_; }
  uint64 timecode() const { return timecode_; }

 private:
  // Outputs the Cluster header to |writer_|. Returns true on success.
  bool WriteClusterHeader();

  // Number of blocks added to the cluster.
  int32 blocks_added_;

  // Flag telling if the cluster has been closed.
  bool finalized_;

  // Flag telling if the cluster's header has been written.
  bool header_written_;

  // The size of the cluster elements in bytes.
  uint64 payload_size_;

  // The file position used for cue points.
  const int64 position_for_cues_;

  // The file position of the cluster's size element.
  int64 size_position_;

  // The absolute timecode of the cluster.
  const uint64 timecode_;

  // Pointer to the writer object. Not owned by this class.
  IMkvWriter* writer_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(Cluster);
};

///////////////////////////////////////////////////////////////
// SeekHead element
class SeekHead {
 public:
  SeekHead();
  ~SeekHead();

  // TODO(fgalligan): Change this to reserve a certain size. Then check how
  // big the seek entry to be added is as not every seek entry will be the
  // maximum size it could be.
  // Adds a seek entry to be written out when the element is finalized. |id|
  // must be the coded mkv element id. |pos| is the file position of the
  // element. Returns true on success.
  bool AddSeekEntry(uint32 id, uint64 pos);

  // Writes out SeekHead and SeekEntry elements. Returns true on success.
  bool Finalize(IMkvWriter* writer) const;

  // Reserves space by writing out a Void element which will be updated with
  // a SeekHead element later. Returns true on success.
  bool Write(IMkvWriter* writer);

 private:
   // We are going to put a cap on the number of Seek Entries.
  const static int32 kSeekEntryCount = 4;

  // Returns the maximum size in bytes of one seek entry.
  uint64 MaxEntrySize() const;

  // Seek entry id element list.
  uint32 seek_entry_id_[kSeekEntryCount];

  // Seek entry pos element list.
  uint64 seek_entry_pos_[kSeekEntryCount];

  // The file position of SeekHead element.
  int64 start_pos_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(SeekHead);
};

///////////////////////////////////////////////////////////////
// Segment Information element
class SegmentInfo {
 public:
  SegmentInfo();
  ~SegmentInfo();

  // Will update the duration if |duration_| is > 0.0. Returns true on success.
  bool Finalize(IMkvWriter* writer) const;

  // Sets |muxing_app_| and |writing_app_|.
  bool Init();

  // Output the Segment Information element to the writer. Returns true on
  // success.
  bool Write(IMkvWriter* writer);

  void set_duration(double duration) { duration_ = duration; }
  double duration() const { return duration_; }
  const char* muxing_app() const { return muxing_app_; }
  void set_timecode_scale(uint64 scale) { timecode_scale_ = scale; }
  uint64 timecode_scale() const { return timecode_scale_; }
  void set_writing_app(const char* app);
  const char* writing_app() const { return writing_app_; }

 private:
  // Segment Information element names.
  // Initially set to -1 to signify that a duration has not been set and should
  // not be written out.
  double duration_;
  // Set to libwebm-%d.%d.%d.%d, major, minor, build, revision.
  char* muxing_app_;
  uint64 timecode_scale_;
  // Initially set to libwebm-%d.%d.%d.%d, major, minor, build, revision.
  char* writing_app_;

  // The file position of the duration element.
  int64 duration_pos_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(SegmentInfo);
};

///////////////////////////////////////////////////////////////
// This class represents the main segment in a WebM file. Currently only
// supports one Segment element.
//
// Notes:
//  |Init| must be called before any other method in this class.
class Segment {
 public:
  enum Mode {
    kLive = 0x1,
    kFile = 0x2
  };

  const static uint64 kDefaultMaxClusterDuration = 30000000000ULL;

  Segment();
  ~Segment();

  // Initializes |SegmentInfo| and returns result. Always returns false when
  // |ptr_writer| is NULL.
  bool Init(IMkvWriter* ptr_writer);

  // Adds an audio track to the segment. Returns the number of the track on
  // success, 0 on error.
  uint64 AddAudioTrack(int32 sample_rate, int32 channels);

  // Adds an audio track to the segment. Returns the number of the track on
  // success, 0 on error. |number| is the number to use for the audio track.
  // |number| must be >= 0. If |number| == 0 then the muxer will decide on
  // the track number.
  uint64 AddAudioTrack(int32 sample_rate, int32 channels, int32 number);

  // Adds a frame to be output in the file. Returns true on success.
  // Inputs:
  //   frame: Pointer to the data
  //   length: Length of the data
  //   track_number: Track to add the data to. Value returned by Add track
  //                 functions.
  //   timestamp:    Timestamp of the frame in nanoseconds from 0.
  //   is_key:       Flag telling whether or not this frame is a key frame.
  bool AddFrame(const uint8* frame,
                uint64 length,
                uint64 track_number,
                uint64 timestamp,
                bool is_key);

  // Adds a video track to the segment. Returns the number of the track on
  // success, 0 on error.
  uint64 AddVideoTrack(int32 width, int32 height);

  // Adds a video track to the segment. Returns the number of the track on
  // success, 0 on error. |number| is the number to use for the video track.
  // |number| must be >= 0. If |number| == 0 then the muxer will decide on
  // the track number.
  uint64 AddVideoTrack(int32 width, int32 height, int32 number);

  // Sets which track to use for the Cues element. Must have added the track
  // before calling this function. Returns true on success. |track_number| is
  // returned by the Add track functions.
  bool CuesTrack(uint64 track_number);

  // Writes out any frames that have not been written out. Finalizes the last
  // cluster. May update the size and duration of the segment. May output the
  // Cues element. May finalize the SeekHead element. Returns true on success.
  bool Finalize();

  // Returns the Cues object.
  Cues* GetCues() { return &cues_; }

  // Returns the Segment Information object.
  SegmentInfo* GetSegmentInfo() { return &segment_info_; }

  // Search the Tracks and return the track that matches |track_number|.
  // Returns NULL if there is no track match.
  Track* GetTrackByNumber(uint64 track_number) const;

  // Toggles whether to output a cues element.
  void OutputCues(bool output_cues);

  // Sets if the muxer will output files in chunks or not. |chunking| is a
  // flag telling whether or not to turn on chunking. |filename| is the base
  // filename for the chunk files. The header chunk file will be named
  // |filename|.hdr and the data chunks will be named
  // |filename|_XXXXXX.chk. Chunking implies that the muxer will be writing
  // to files so the muxer will use the default MkvWriter class to control
  // what data is written to what files. Returns true on success.
  // TODO: Should we change the IMkvWriter Interface to add Open and Close?
  // That will force the interface to be dependent on files.
  bool SetChunking(bool chunking, const char* filename);

  bool chunking() const { return chunking_; }
  uint64 cues_track() const { return cues_track_; }
  void set_max_cluster_duration(uint64 max_cluster_duration) {
    max_cluster_duration_ = max_cluster_duration;
  }
  uint64 max_cluster_duration() const { return max_cluster_duration_; }
  void set_max_cluster_size(uint64 max_cluster_size) {
    max_cluster_size_ = max_cluster_size;
  }
  uint64 max_cluster_size() const { return max_cluster_size_; }
  void set_mode(Mode mode) { mode_ = mode; }
  Mode mode() const { return mode_; }
  bool output_cues() const { return output_cues_; }
  const SegmentInfo* segment_info() const { return &segment_info_; }

 private:
  // Adds a cue point to the Cues element. |timestamp| is the time in
  // nanoseconds of the cue's time. Returns true on success.
  bool AddCuePoint(uint64 timestamp);

  // Checks if header information has been output and initialized. If not it
  // will output the Segment element and initialize the SeekHead elment and
  // Cues elements.
  bool CheckHeaderInfo();

  // Sets |name| according to how many chunks have been written. |ext| is the
  // file extension. |name| must be deleted by the calling app. Returns true
  // on success.
  bool UpdateChunkName(const char* ext, char** name) const;

  // Returns the maximum offset within the segment's payload. When chunking
  // this function is needed to determine offsets of elements within the
  // chunked files. Returns -1 on error.
  int64 MaxOffset();

  // Adds the frame to our frame array.
  bool QueueFrame(Frame* frame);

  // Output all frames that are queued. Returns true on success and if there
  // are no frames queued.
  bool WriteFramesAll();

  // Output all frames that are queued that have an end time that is less
  // then |timestamp|. Returns true on success and if there are no frames
  // queued.
  bool WriteFramesLessThan(uint64 timestamp);

  // Outputs the segment header, Segment Information element, SeekHead element,
  // and Tracks element to |writer_|.
  bool WriteSegmentHeader();

  // WebM elements
  Cues cues_;
  SeekHead seek_head_;
  SegmentInfo segment_info_;
  Tracks tracks_;

  // Number of chunks written.
  int chunk_count_;

  // Current chunk filename.
  char* chunk_name_;

  // Default MkvWriter object created by this class used for writing clusters
  // out in separate files.
  MkvWriter* chunk_writer_cluster_;

  // Default MkvWriter object created by this class used for writing Cues
  // element out to a file.
  MkvWriter* chunk_writer_cues_;

  // Default MkvWriter object created by this class used for writing the
  // Matroska header out to a file.
  MkvWriter* chunk_writer_header_;

  // Flag telling whether or not the muxer is chunking output to multiple
  // files.
  bool chunking_;

  // Base filename for the chunked files.
  char* chunking_base_name_;

  // List of clusters.
  Cluster** cluster_list_;

  // Number of cluster pointers allocated in the cluster list.
  int32 cluster_list_capacity_;

  // Number of clusters in the cluster list.
  int32 cluster_list_size_;

  // Track number that is associated with the cues element for this segment.
  uint64 cues_track_;

  // List of stored audio frames. These variables are used to store frames so
  // the muxer can follow the guideline "Audio blocks that contain the video
  // key frame's timecode should be in the same cluster as the video key frame
  // block."
  Frame** frames_;

  // Number of frame pointers allocated in the frame list.
  int32 frames_capacity_;

  // Number of frames in the frame list.
  int32 frames_size_;

  // Flag telling if a video track has been added to the segment.
  bool has_video_;

  // Flag telling if the segment's header has been written.
  bool header_written_;

  // Last timestamp in nanoseconds added to a cluster.
  uint64 last_timestamp_;

  // Maximum time in nanoseconds for a cluster duration. This variable is a
  // guideline and some clusters may have a longer duration. Default is 30
  // seconds.
  uint64 max_cluster_duration_;

  // Maximum size in bytes for a cluster. This variable is a guideline and
  // some clusters may have a larger size. Default is 0 which signifies that
  // the muxer will decide the size.
  uint64 max_cluster_size_;

  // The mode that segment is in. If set to |kLive| the writer must not
  // seek backwards.
  Mode mode_;

  // Flag telling the muxer that a new cluster should be started with the next
  // frame.
  bool new_cluster_;

  // Flag telling the muxer that a new cue point should be added.
  bool new_cuepoint_;

  // TODO(fgalligan): Should we add support for more than one Cues element?
  // Flag whether or not the muxer should output a Cues element.
  bool output_cues_;

  // The file position of the segment's payload.
  int64 payload_pos_;

  // The file position of the element's size.
  int64 size_position_;

  // Pointer to the writer objects. Not owned by this class.
  IMkvWriter* writer_cluster_;
  IMkvWriter* writer_cues_;
  IMkvWriter* writer_header_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(Segment);
};

}  //end namespace mkvmuxer

#endif //MKVMUXER_HPP
