// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "mkvmuxer.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <new>

#include "mkvmuxerutil.hpp"
#include "mkvwriter.hpp"
#include "webmids.hpp"

namespace mkvmuxer {

///////////////////////////////////////////////////////////////
//
// IMkvWriter Class

IMkvWriter::IMkvWriter() {
}

IMkvWriter::~IMkvWriter() {
}

bool WriteEbmlHeader(IMkvWriter* writer) {
  // Level 0
  uint64 size = EbmlElementSize(kMkvEBMLVersion, 1ULL);
  size += EbmlElementSize(kMkvEBMLReadVersion, 1ULL);
  size += EbmlElementSize(kMkvEBMLMaxIDLength, 4ULL);
  size += EbmlElementSize(kMkvEBMLMaxSizeLength, 8ULL);
  size += EbmlElementSize(kMkvDocType, "webm");
  size += EbmlElementSize(kMkvDocTypeVersion, 2ULL);
  size += EbmlElementSize(kMkvDocTypeReadVersion, 2ULL);

  if (!WriteEbmlMasterElement(writer, kMkvEBML, size))
    return false;
  if (!WriteEbmlElement(writer, kMkvEBMLVersion, 1ULL))
    return false;
  if (!WriteEbmlElement(writer, kMkvEBMLReadVersion, 1ULL))
    return false;
  if (!WriteEbmlElement(writer, kMkvEBMLMaxIDLength, 4ULL))
    return false;
  if (!WriteEbmlElement(writer, kMkvEBMLMaxSizeLength, 8ULL))
    return false;
  if (!WriteEbmlElement(writer, kMkvDocType, "webm"))
    return false;
  if (!WriteEbmlElement(writer, kMkvDocTypeVersion, 2ULL))
    return false;
  if (!WriteEbmlElement(writer, kMkvDocTypeReadVersion, 2ULL))
    return false;

  return true;
}

///////////////////////////////////////////////////////////////
//
// Frame Class

Frame::Frame()
    : frame_(NULL),
      length_(0),
      track_number_(0),
      timestamp_(0),
      is_key_(false) {
}

Frame::~Frame() {
  delete [] frame_;
}

bool Frame::Init(const uint8* frame, uint64 length) {
  uint8* const data =
      new (std::nothrow) uint8[static_cast<size_t>(length)];  // NOLINT
  if (!data)
    return false;

  delete [] frame_;
  frame_ = data;
  length_ = length;

  memcpy(frame_, frame, static_cast<size_t>(length_));
  return true;
}

///////////////////////////////////////////////////////////////
//
// CuePoint Class

CuePoint::CuePoint()
    : time_(0),
      track_(0),
      cluster_pos_(0),
      block_number_(1),
      output_block_number_(true) {
}

CuePoint::~CuePoint() {
}

bool CuePoint::Write(IMkvWriter* writer) const {
  if (!writer || track_ < 1 || cluster_pos_ < 1)
    return false;

  uint64 size = EbmlElementSize(kMkvCueClusterPosition, cluster_pos_);
  size += EbmlElementSize(kMkvCueTrack, track_);
  if (output_block_number_ && block_number_ > 1)
    size += EbmlElementSize(kMkvCueBlockNumber, block_number_);
  const uint64 track_pos_size = EbmlMasterElementSize(kMkvCueTrackPositions,
                                                      size) + size;
  const uint64 payload_size = EbmlElementSize(kMkvCueTime, time_) +
                              track_pos_size;

  if (!WriteEbmlMasterElement(writer, kMkvCuePoint, payload_size))
    return false;

  const int64 payload_position = writer->Position();
  if (payload_position < 0)
    return false;

  if (!WriteEbmlElement(writer, kMkvCueTime, time_))
    return false;

  if (!WriteEbmlMasterElement(writer, kMkvCueTrackPositions, size))
    return false;
  if (!WriteEbmlElement(writer, kMkvCueTrack, track_))
    return false;
  if (!WriteEbmlElement(writer, kMkvCueClusterPosition, cluster_pos_))
    return false;
  if (output_block_number_ && block_number_ > 1)
    if (!WriteEbmlElement(writer, kMkvCueBlockNumber, block_number_))
      return false;

  const int64 stop_position = writer->Position();
  if (stop_position < 0)
    return false;

  if (stop_position - payload_position != static_cast<int64>(payload_size))
    return false;

  return true;
}

uint64 CuePoint::PayloadSize() const {
  uint64 size = EbmlElementSize(kMkvCueClusterPosition, cluster_pos_);
  size += EbmlElementSize(kMkvCueTrack, track_);
  if (output_block_number_ && block_number_ > 1)
    size += EbmlElementSize(kMkvCueBlockNumber, block_number_);
  const uint64 track_pos_size = EbmlMasterElementSize(kMkvCueTrackPositions,
                                                      size) + size;
  const uint64 payload_size = EbmlElementSize(kMkvCueTime, time_) +
                              track_pos_size;

  return payload_size;
}

uint64 CuePoint::Size() const {
  const uint64 payload_size = PayloadSize();
  return EbmlMasterElementSize(kMkvCuePoint, payload_size) + payload_size;
}

///////////////////////////////////////////////////////////////
//
// Cues Class

Cues::Cues()
    : cue_entries_capacity_(0),
      cue_entries_size_(0),
      cue_entries_(NULL),
      output_block_number_(true) {
}

Cues::~Cues() {
  if (cue_entries_) {
    for (int32 i = 0; i < cue_entries_size_; ++i) {
      CuePoint* const cue = cue_entries_[i];
      delete cue;
    }
    delete [] cue_entries_;
  }
}

bool Cues::AddCue(CuePoint* cue) {
  if (!cue)
    return false;

  if ((cue_entries_size_ + 1) > cue_entries_capacity_) {
    // Add more CuePoints.
    const int32 new_capacity =
        (!cue_entries_capacity_) ? 2 : cue_entries_capacity_ * 2;

    if (new_capacity < 1)
      return false;

    CuePoint** const cues =
        new (std::nothrow) CuePoint*[new_capacity];  // NOLINT
    if (!cues)
      return false;

    for (int32 i = 0; i < cue_entries_size_; ++i) {
      cues[i] = cue_entries_[i];
    }

    delete [] cue_entries_;

    cue_entries_ = cues;
    cue_entries_capacity_ = new_capacity;
  }

  cue->set_output_block_number(output_block_number_);
  cue_entries_[cue_entries_size_++] = cue;
  return true;
}

const CuePoint* Cues::GetCueByIndex(int32 index) const {
  if (cue_entries_ == NULL)
    return NULL;

  if (index >= cue_entries_size_)
    return NULL;

  return cue_entries_[index];
}

bool Cues::Write(IMkvWriter* writer) const {
  if (!writer)
    return false;

  uint64 size = 0;
  for (int32 i = 0; i < cue_entries_size_; ++i) {
    const CuePoint* const cue = GetCueByIndex(i);

    if (!cue)
      return false;

    size += cue->Size();
  }

  if (!WriteEbmlMasterElement(writer, kMkvCues, size))
    return false;

  const int64 payload_position = writer->Position();
  if (payload_position < 0)
    return false;

  for (int32 i = 0; i < cue_entries_size_; ++i) {
    const CuePoint* const cue = GetCueByIndex(i);

    if (!cue->Write(writer))
      return false;
  }

  const int64 stop_position = writer->Position();
  if (stop_position < 0)
    return false;

  if (stop_position - payload_position != static_cast<int64>(size))
    return false;

  return true;
}

///////////////////////////////////////////////////////////////
//
// ContentEncAESSettings Class

ContentEncAESSettings::ContentEncAESSettings() : cipher_mode_(kCTR) {}

uint64 ContentEncAESSettings::Size() const {
  const uint64 payload = PayloadSize();
  const uint64 size =
      EbmlMasterElementSize(kMkvContentEncAESSettings, payload) + payload;
  return size;
}

bool ContentEncAESSettings::Write(IMkvWriter* writer) const {
  const uint64 payload = PayloadSize();

  if (!WriteEbmlMasterElement(writer, kMkvContentEncAESSettings, payload))
    return false;

  const int64 payload_position = writer->Position();
  if (payload_position < 0)
    return false;

  if (!WriteEbmlElement(writer, kMkvAESSettingsCipherMode, cipher_mode_))
    return false;

  const int64 stop_position = writer->Position();
  if (stop_position < 0 ||
      stop_position - payload_position != static_cast<int64>(payload))
    return false;

  return true;
}

uint64 ContentEncAESSettings::PayloadSize() const {
  uint64 size = EbmlElementSize(kMkvAESSettingsCipherMode, cipher_mode_);
  return size;
}

///////////////////////////////////////////////////////////////
//
// ContentEncoding Class

ContentEncoding::ContentEncoding()
    : enc_algo_(5),
      enc_key_id_(NULL),
      encoding_order_(0),
      encoding_scope_(1),
      encoding_type_(1),
      enc_key_id_length_(0) {
}

ContentEncoding::~ContentEncoding() {
  delete [] enc_key_id_;
}

bool ContentEncoding::SetEncryptionID(const uint8* id, uint64 length) {
  if (!id || length < 1)
    return false;

  delete [] enc_key_id_;

  enc_key_id_ =
      new (std::nothrow) uint8[static_cast<size_t>(length)];  // NOLINT
  if (!enc_key_id_)
    return false;

  memcpy(enc_key_id_, id, static_cast<size_t>(length));
  enc_key_id_length_ = length;

  return true;
}

uint64 ContentEncoding::Size() const {
  const uint64 encryption_size = EncryptionSize();
  const uint64 encoding_size = EncodingSize(0, encryption_size);
  const uint64 encodings_size = EbmlMasterElementSize(kMkvContentEncoding,
                                                      encoding_size) +
                                encoding_size;

  return encodings_size;
}

bool ContentEncoding::Write(IMkvWriter* writer) const {
  const uint64 encryption_size = EncryptionSize();
  const uint64 encoding_size = EncodingSize(0, encryption_size);
  const uint64 size = EbmlMasterElementSize(kMkvContentEncoding,
                                            encoding_size) +
                      encoding_size;

  const int64 payload_position = writer->Position();
  if (payload_position < 0)
    return false;

  if (!WriteEbmlMasterElement(writer, kMkvContentEncoding, encoding_size))
    return false;
  if (!WriteEbmlElement(writer, kMkvContentEncodingOrder, encoding_order_))
    return false;
  if (!WriteEbmlElement(writer, kMkvContentEncodingScope, encoding_scope_))
    return false;
  if (!WriteEbmlElement(writer, kMkvContentEncodingType, encoding_type_))
    return false;

  if (!WriteEbmlMasterElement(writer, kMkvContentEncryption, encryption_size))
    return false;
  if (!WriteEbmlElement(writer, kMkvContentEncAlgo, enc_algo_))
    return false;
  if (!WriteEbmlElement(writer,
                        kMkvContentEncKeyID,
                        enc_key_id_,
                        enc_key_id_length_))
    return false;

  if (!enc_aes_settings_.Write(writer))
    return false;

  const int64 stop_position = writer->Position();
  if (stop_position < 0 ||
      stop_position - payload_position != static_cast<int64>(size))
    return false;

  return true;
}

uint64 ContentEncoding::EncodingSize(uint64 compresion_size,
                                     uint64 encryption_size) const {
  // TODO(fgalligan): Add support for compression settings.
  if (compresion_size != 0)
    return 0;

  uint64 encoding_size = 0;

  if (encryption_size > 0) {
    encoding_size += EbmlMasterElementSize(kMkvContentEncryption,
                                           encryption_size) +
                     encryption_size;
  }
  encoding_size += EbmlElementSize(kMkvContentEncodingType, encoding_type_);
  encoding_size += EbmlElementSize(kMkvContentEncodingScope, encoding_scope_);
  encoding_size += EbmlElementSize(kMkvContentEncodingOrder, encoding_order_);

  return encoding_size;
}

uint64 ContentEncoding::EncryptionSize() const {
  const uint64 aes_size = enc_aes_settings_.Size();

  uint64 encryption_size = EbmlElementSize(kMkvContentEncKeyID,
                                           enc_key_id_,
                                           enc_key_id_length_);
  encryption_size += EbmlElementSize(kMkvContentEncAlgo, enc_algo_);

  return encryption_size + aes_size;
}

///////////////////////////////////////////////////////////////
//
// Track Class

Track::Track()
    : codec_id_(NULL),
      codec_private_(NULL),
      language_(NULL),
      name_(NULL),
      number_(0),
      type_(0),
      uid_(MakeUID()),
      codec_private_length_(0),
      content_encoding_entries_(NULL),
      content_encoding_entries_size_(0) {
}

Track::~Track() {
  delete [] codec_id_;
  delete [] codec_private_;
  delete [] language_;
  delete [] name_;

  if (content_encoding_entries_) {
    for (uint32 i = 0; i < content_encoding_entries_size_; ++i) {
      ContentEncoding* const encoding = content_encoding_entries_[i];
      delete encoding;
    }
    delete [] content_encoding_entries_;
  }
}

bool Track::AddContentEncoding() {
  const uint32 count = content_encoding_entries_size_ + 1;

  ContentEncoding** const content_encoding_entries =
      new (std::nothrow) ContentEncoding*[count];  // NOLINT
  if (!content_encoding_entries)
    return false;

  ContentEncoding* const content_encoding =
      new (std::nothrow) ContentEncoding();  // NOLINT
  if (!content_encoding) {
    delete [] content_encoding_entries;
    return false;
  }

  for (uint32 i = 0; i < content_encoding_entries_size_; ++i) {
    content_encoding_entries[i] = content_encoding_entries_[i];
  }

  delete [] content_encoding_entries_;

  content_encoding_entries_ = content_encoding_entries;
  content_encoding_entries_[content_encoding_entries_size_] = content_encoding;
  content_encoding_entries_size_ = count;
  return true;
}

ContentEncoding* Track::GetContentEncodingByIndex(uint32 index) const {
  if (content_encoding_entries_ == NULL)
    return NULL;

  if (index >= content_encoding_entries_size_)
    return NULL;

  return content_encoding_entries_[index];
}

uint64 Track::PayloadSize() const {
  uint64 size = EbmlElementSize(kMkvTrackNumber, number_);
  size += EbmlElementSize(kMkvTrackUID, uid_);
  size += EbmlElementSize(kMkvTrackType, type_);
  if (codec_id_)
    size += EbmlElementSize(kMkvCodecID, codec_id_);
  if (codec_private_)
    size += EbmlElementSize(kMkvCodecPrivate,
                            codec_private_,
                            codec_private_length_);
  if (language_)
    size += EbmlElementSize(kMkvLanguage, language_);
  if (name_)
    size += EbmlElementSize(kMkvName, name_);

  if (content_encoding_entries_size_ > 0) {
    uint64 content_encodings_size = 0;
    for (uint32 i = 0; i < content_encoding_entries_size_; ++i) {
      ContentEncoding* const encoding = content_encoding_entries_[i];
      content_encodings_size += encoding->Size();
    }

    size += EbmlMasterElementSize(kMkvContentEncodings,
                                  content_encodings_size) +
            content_encodings_size;
  }

  return size;
}

uint64 Track::Size() const {
  uint64 size = PayloadSize();
  size += EbmlMasterElementSize(kMkvTrackEntry, size);
  return size;
}

bool Track::Write(IMkvWriter* writer) const {
  if (!writer)
    return false;

  // |size| may be bigger than what is written out in this function because
  // derived classes may write out more data in the Track element.
  const uint64 payload_size = PayloadSize();

  if (!WriteEbmlMasterElement(writer, kMkvTrackEntry, payload_size))
    return false;

  uint64 size = EbmlElementSize(kMkvTrackNumber, number_);
  size += EbmlElementSize(kMkvTrackUID, uid_);
  size += EbmlElementSize(kMkvTrackType, type_);
  if (codec_id_)
    size += EbmlElementSize(kMkvCodecID, codec_id_);
  if (codec_private_)
    size += EbmlElementSize(kMkvCodecPrivate,
                            codec_private_,
                            codec_private_length_);
  if (language_)
    size += EbmlElementSize(kMkvLanguage, language_);
  if (name_)
    size += EbmlElementSize(kMkvName, name_);

  const int64 payload_position = writer->Position();
  if (payload_position < 0)
    return false;

  if (!WriteEbmlElement(writer, kMkvTrackNumber, number_))
    return false;
  if (!WriteEbmlElement(writer, kMkvTrackUID, uid_))
    return false;
  if (!WriteEbmlElement(writer, kMkvTrackType, type_))
    return false;
  if (codec_id_) {
    if (!WriteEbmlElement(writer, kMkvCodecID, codec_id_))
      return false;
  }
  if (codec_private_) {
    if (!WriteEbmlElement(writer,
                          kMkvCodecPrivate,
                          codec_private_,
                          codec_private_length_))
      return false;
  }
  if (language_) {
    if (!WriteEbmlElement(writer, kMkvLanguage, language_))
      return false;
  }
  if (name_) {
    if (!WriteEbmlElement(writer, kMkvName, name_))
      return false;
  }

  int64 stop_position = writer->Position();
  if (stop_position < 0 ||
      stop_position - payload_position != static_cast<int64>(size))
    return false;

  if (content_encoding_entries_size_ > 0) {
    uint64 content_encodings_size = 0;
    for (uint32 i = 0; i < content_encoding_entries_size_; ++i) {
      ContentEncoding* const encoding = content_encoding_entries_[i];
      content_encodings_size += encoding->Size();
    }

    if (!WriteEbmlMasterElement(writer,
                                kMkvContentEncodings,
                                content_encodings_size))
      return false;

    for (uint32 i = 0; i < content_encoding_entries_size_; ++i) {
      ContentEncoding* const encoding = content_encoding_entries_[i];
      if (!encoding->Write(writer))
        return false;
    }
  }

  stop_position = writer->Position();
  if (stop_position < 0)
    return false;
  return true;
}

bool Track::SetCodecPrivate(const uint8* codec_private, uint64 length) {
  if (!codec_private || length < 1)
    return false;

  delete [] codec_private_;

  codec_private_ =
      new (std::nothrow) uint8[static_cast<size_t>(length)];  // NOLINT
  if (!codec_private_)
    return false;

  memcpy(codec_private_, codec_private, static_cast<size_t>(length));
  codec_private_length_ = length;

  return true;
}

void Track::set_codec_id(const char* codec_id) {
  if (codec_id) {
    delete [] codec_id_;

    const size_t length = strlen(codec_id) + 1;
    codec_id_ = new (std::nothrow) char[length];  // NOLINT
    if (codec_id_) {
#ifdef _MSC_VER
      strcpy_s(codec_id_, length, codec_id);
#else
      strcpy(codec_id_, codec_id);
#endif
    }
  }
}

// TODO(fgalligan): Vet the language parameter.
void Track::set_language(const char* language) {
  if (language) {
    delete [] language_;

    const size_t length = strlen(language) + 1;
    language_ = new (std::nothrow) char[length];  // NOLINT
    if (language_) {
#ifdef _MSC_VER
      strcpy_s(language_, length, language);
#else
      strcpy(language_, language);
#endif
    }
  }
}

void Track::set_name(const char* name) {
  if (name) {
    delete [] name_;

    const size_t length = strlen(name) + 1;
    name_ = new (std::nothrow) char[length];  // NOLINT
    if (name_) {
#ifdef _MSC_VER
      strcpy_s(name_, length, name);
#else
      strcpy(name_, name);
#endif
    }
  }
}

bool Track::is_seeded_ = false;

uint64 Track::MakeUID() {
  if (!is_seeded_) {
    srand(static_cast<uint32>(time(NULL)));
    is_seeded_ = true;
  }

  uint64 track_uid = 0;
  for (int32 i = 0; i < 7; ++i) {  // avoid problems with 8-byte values
    track_uid <<= 8;

    const int32 nn = rand();
    const int32 n = 0xFF & (nn >> 4);  // throw away low-order bits

    track_uid |= n;
  }

  return track_uid;
}

///////////////////////////////////////////////////////////////
//
// VideoTrack Class

VideoTrack::VideoTrack()
    : display_height_(0),
      display_width_(0),
      frame_rate_(0.0),
      height_(0),
      stereo_mode_(0),
      width_(0) {
}

VideoTrack::~VideoTrack() {
}

bool VideoTrack::SetStereoMode(uint64 stereo_mode) {
  if (stereo_mode != kMono &&
      stereo_mode != kSideBySideLeftIsFirst &&
      stereo_mode != kTopBottomRightIsFirst &&
      stereo_mode != kTopBottomLeftIsFirst &&
      stereo_mode != kSideBySideRightIsFirst)
    return false;

  stereo_mode_ = stereo_mode;
  return true;
}

uint64 VideoTrack::PayloadSize() const {
  const uint64 parent_size = Track::PayloadSize();

  uint64 size = VideoPayloadSize();
  size += EbmlMasterElementSize(kMkvVideo, size);

  return parent_size + size;
}

bool VideoTrack::Write(IMkvWriter* writer) const {
  if (!Track::Write(writer))
    return false;

  const uint64 size = VideoPayloadSize();

  if (!WriteEbmlMasterElement(writer, kMkvVideo, size))
    return false;

  const int64 payload_position = writer->Position();
  if (payload_position < 0)
    return false;

  if (!WriteEbmlElement(writer, kMkvPixelWidth, width_))
    return false;
  if (!WriteEbmlElement(writer, kMkvPixelHeight, height_))
    return false;
  if (display_width_ > 0)
    if (!WriteEbmlElement(writer, kMkvDisplayWidth, display_width_))
      return false;
  if (display_height_ > 0)
    if (!WriteEbmlElement(writer, kMkvDisplayHeight, display_height_))
      return false;
  if (stereo_mode_ > kMono)
    if (!WriteEbmlElement(writer, kMkvStereoMode, stereo_mode_))
      return false;
  if (frame_rate_ > 0.0)
    if (!WriteEbmlElement(writer,
                          kMkvFrameRate,
                          static_cast<float>(frame_rate_)))
      return false;

  const int64 stop_position = writer->Position();
  if (stop_position < 0 ||
      stop_position - payload_position != static_cast<int64>(size))
    return false;

  return true;
}

uint64 VideoTrack::VideoPayloadSize() const {
  uint64 size = EbmlElementSize(kMkvPixelWidth, width_);
  size += EbmlElementSize(kMkvPixelHeight, height_);
  if (display_width_ > 0)
    size += EbmlElementSize(kMkvDisplayWidth, display_width_);
  if (display_height_ > 0)
    size += EbmlElementSize(kMkvDisplayHeight, display_height_);
  if (stereo_mode_ > kMono)
    size += EbmlElementSize(kMkvStereoMode, stereo_mode_);
  if (frame_rate_ > 0.0)
    size += EbmlElementSize(kMkvFrameRate, static_cast<float>(frame_rate_));

  return size;
}

///////////////////////////////////////////////////////////////
//
// AudioTrack Class

AudioTrack::AudioTrack()
    : bit_depth_(0),
      channels_(1),
      sample_rate_(0.0) {
}

AudioTrack::~AudioTrack() {
}

uint64 AudioTrack::PayloadSize() const {
  const uint64 parent_size = Track::PayloadSize();

  uint64 size = EbmlElementSize(kMkvSamplingFrequency,
                                static_cast<float>(sample_rate_));
  size += EbmlElementSize(kMkvChannels, channels_);
  if (bit_depth_ > 0)
    size += EbmlElementSize(kMkvBitDepth, bit_depth_);
  size += EbmlMasterElementSize(kMkvAudio, size);

  return parent_size + size;
}

bool AudioTrack::Write(IMkvWriter* writer) const {
  if (!Track::Write(writer))
    return false;

  // Calculate AudioSettings size.
  uint64 size = EbmlElementSize(kMkvSamplingFrequency,
                                static_cast<float>(sample_rate_));
  size += EbmlElementSize(kMkvChannels, channels_);
  if (bit_depth_ > 0)
    size += EbmlElementSize(kMkvBitDepth, bit_depth_);

  if (!WriteEbmlMasterElement(writer, kMkvAudio, size))
    return false;

  const int64 payload_position = writer->Position();
  if (payload_position < 0)
    return false;

  if (!WriteEbmlElement(writer,
                        kMkvSamplingFrequency,
                        static_cast<float>(sample_rate_)))
    return false;
  if (!WriteEbmlElement(writer, kMkvChannels, channels_))
    return false;
  if (bit_depth_ > 0)
    if (!WriteEbmlElement(writer, kMkvBitDepth, bit_depth_))
      return false;

  const int64 stop_position = writer->Position();
  if (stop_position < 0 ||
      stop_position - payload_position != static_cast<int64>(size))
    return false;

  return true;
}

///////////////////////////////////////////////////////////////
//
// Tracks Class

const char* const Tracks::kVp8CodecId = "V_VP8";
const char* const Tracks::kVorbisCodecId = "A_VORBIS";

Tracks::Tracks()
    : track_entries_(NULL),
      track_entries_size_(0) {
}

Tracks::~Tracks() {
  if (track_entries_) {
    for (uint32 i = 0; i < track_entries_size_; ++i) {
      Track* const track = track_entries_[i];
      delete track;
    }
    delete [] track_entries_;
  }
}

bool Tracks::AddTrack(Track* track, int32 number) {
  if (number < 0)
    return false;

  uint32 track_num = number;

  // Check to make sure a track does not already have |track_num|.
  for (uint32 i = 0; i < track_entries_size_; ++i) {
    if (track_entries_[i]->number() == track_num)
      return false;
  }

  const uint32 count = track_entries_size_ + 1;

  Track** const track_entries = new (std::nothrow) Track*[count];  // NOLINT
  if (!track_entries)
    return false;

  for (uint32 i = 0; i < track_entries_size_; ++i) {
    track_entries[i] = track_entries_[i];
  }

  delete [] track_entries_;

  // Find the lowest availible track number > 0.
  if (track_num == 0) {
    track_num = count;

    // Check to make sure a track does not already have |track_num|.
    bool exit = false;
    do {
      exit = true;
      for (uint32 i = 0; i < track_entries_size_; ++i) {
        if (track_entries[i]->number() == track_num) {
          track_num++;
          exit = false;
          break;
        }
      }
    } while (!exit);
  }
  track->set_number(track_num);

  track_entries_ = track_entries;
  track_entries_[track_entries_size_] = track;
  track_entries_size_ = count;
  return true;
}

const Track* Tracks::GetTrackByIndex(uint32 index) const {
  if (track_entries_ == NULL)
    return NULL;

  if (index >= track_entries_size_)
    return NULL;

  return track_entries_[index];
}

Track* Tracks::GetTrackByNumber(uint64 track_number) const {
  const int32 count = track_entries_size();
  for (int32 i = 0; i < count; ++i) {
    if (track_entries_[i]->number() == track_number)
      return track_entries_[i];
  }

  return NULL;
}

bool Tracks::TrackIsAudio(uint64 track_number) const {
  const Track* const track = GetTrackByNumber(track_number);

  if (track->type() == kAudio)
    return true;

  return false;
}

bool Tracks::TrackIsVideo(uint64 track_number) const {
  const Track* const track = GetTrackByNumber(track_number);

  if (track->type() == kVideo)
    return true;

  return false;
}

bool Tracks::Write(IMkvWriter* writer) const {
  uint64 size = 0;
  const int32 count = track_entries_size();
  for (int32 i = 0; i < count; ++i) {
    const Track* const track = GetTrackByIndex(i);

    if (!track)
      return false;

    size += track->Size();
  }

  if (!WriteEbmlMasterElement(writer, kMkvTracks, size))
    return false;

  const int64 payload_position = writer->Position();
  if (payload_position < 0)
    return false;

  for (int32 i = 0; i < count; ++i) {
    const Track* const track = GetTrackByIndex(i);
    if (!track->Write(writer))
      return false;
  }

  const int64 stop_position = writer->Position();
  if (stop_position < 0 ||
      stop_position - payload_position != static_cast<int64>(size))
    return false;

  return true;
}

///////////////////////////////////////////////////////////////
//
// Cluster Class

Cluster::Cluster(uint64 timecode, int64 cues_pos)
    : blocks_added_(0),
      finalized_(false),
      header_written_(false),
      payload_size_(0),
      position_for_cues_(cues_pos),
      size_position_(-1),
      timecode_(timecode),
      writer_(NULL) {
}

Cluster::~Cluster() {
}

bool Cluster::Init(IMkvWriter* ptr_writer) {
  if (!ptr_writer) {
    return false;
  }
  writer_ = ptr_writer;
  return true;
}

bool Cluster::AddFrame(const uint8* frame,
                       uint64 length,
                       uint64 track_number,
                       uint64 abs_timecode,
                       bool is_key) {
  return DoWriteBlock(frame,
                      length,
                      track_number,
                      abs_timecode,
                      is_key ? 1 : 0,
                      &WriteSimpleBlock);
}

void Cluster::AddPayloadSize(uint64 size) {
  payload_size_ += size;
}

bool Cluster::Finalize() {
  if (!writer_ || finalized_ || size_position_ == -1)
    return false;

  if (writer_->Seekable()) {
    const int64 pos = writer_->Position();

    if (writer_->Position(size_position_))
      return false;

    if (WriteUIntSize(writer_, payload_size(), 8))
      return false;

    if (writer_->Position(pos))
      return false;
  }

  finalized_ = true;

  return true;
}

uint64 Cluster::Size() const {
  const uint64 element_size =
      EbmlMasterElementSize(kMkvCluster,
                            0xFFFFFFFFFFFFFFFFULL) + payload_size_;
  return element_size;
}

bool Cluster::DoWriteBlock(
    const uint8* frame,
    uint64 length,
    uint64 track_number,
    uint64 abs_timecode,
    uint64 generic_arg,
    WriteBlock write_block) {
  if (frame == NULL || length == 0)
    return false;

  // To simplify things, we require that there be fewer than 127
  // tracks -- this allows us to serialize the track number value for
  // a stream using a single byte, per the Matroska encoding.

  if (track_number == 0 || track_number > 0x7E)
    return false;

  const int64 cluster_timecode = this->Cluster::timecode();
  const int64 rel_timecode =
      static_cast<int64>(abs_timecode) - cluster_timecode;

  if (rel_timecode < 0)
    return false;

  if (rel_timecode > std::numeric_limits<int16>::max())
    return false;

  if (write_block == NULL)
    return false;

  if (finalized_)
    return false;

  if (!header_written_)
    if (!WriteClusterHeader())
      return false;

  const uint64 element_size = (*write_block)(writer_,
                                             frame,
                                             length,
                                             track_number,
                                             rel_timecode,
                                             generic_arg);

  if (element_size == 0)
    return false;

  AddPayloadSize(element_size);
  blocks_added_++;

  return true;
}

bool Cluster::WriteClusterHeader() {
  if (finalized_)
    return false;

  if (WriteID(writer_, kMkvCluster))
    return false;

  // Save for later.
  size_position_ = writer_->Position();

  // Write "unknown" (EBML coded -1) as cluster size value. We need to write 8
  // bytes because we do not know how big our cluster will be.
  if (SerializeInt(writer_, kEbmlUnknownValue, 8))
    return false;

  if (!WriteEbmlElement(writer_, kMkvTimecode, timecode()))
    return false;
  AddPayloadSize(EbmlElementSize(kMkvTimecode, timecode()));
  header_written_ = true;

  return true;
}

///////////////////////////////////////////////////////////////
//
// SeekHead Class

SeekHead::SeekHead() : start_pos_(0ULL) {
  for (int32 i = 0; i < kSeekEntryCount; ++i) {
    seek_entry_id_[i] = 0;
    seek_entry_pos_[i] = 0;
  }
}

SeekHead::~SeekHead() {
}

bool SeekHead::Finalize(IMkvWriter* writer) const {
  if (writer->Seekable()) {
    if (start_pos_ == -1)
      return false;

    uint64 payload_size = 0;
    uint64 entry_size[kSeekEntryCount];

    for (int32 i = 0; i < kSeekEntryCount; ++i) {
      if (seek_entry_id_[i] != 0) {
        entry_size[i] = EbmlElementSize(
            kMkvSeekID,
            static_cast<uint64>(seek_entry_id_[i]));
        entry_size[i] += EbmlElementSize(kMkvSeekPosition, seek_entry_pos_[i]);

        payload_size += EbmlMasterElementSize(kMkvSeek, entry_size[i]) +
                        entry_size[i];
      }
    }

    // No SeekHead elements
    if (payload_size == 0)
      return true;

    const int64 pos = writer->Position();
    if (writer->Position(start_pos_))
      return false;

    if (!WriteEbmlMasterElement(writer, kMkvSeekHead, payload_size))
      return false;

    for (int32 i = 0; i < kSeekEntryCount; ++i) {
      if (seek_entry_id_[i] != 0) {
        if (!WriteEbmlMasterElement(writer, kMkvSeek, entry_size[i]))
          return false;

        if (!WriteEbmlElement(writer,
                              kMkvSeekID,
                              static_cast<uint64>(seek_entry_id_[i])))
          return false;

        if (!WriteEbmlElement(writer, kMkvSeekPosition, seek_entry_pos_[i]))
          return false;
      }
    }

    const uint64 total_entry_size = kSeekEntryCount * MaxEntrySize();
    const uint64 total_size =
        EbmlMasterElementSize(kMkvSeekHead,
                              total_entry_size) + total_entry_size;
    const int64 size_left = total_size - (writer->Position() - start_pos_);

    const uint64 bytes_written = WriteVoidElement(writer, size_left);
    if (!bytes_written)
      return false;

    if (writer->Position(pos))
      return false;
  }

  return true;
}

bool SeekHead::Write(IMkvWriter* writer) {
  const uint64 entry_size = kSeekEntryCount * MaxEntrySize();
  const uint64 size = EbmlMasterElementSize(kMkvSeekHead, entry_size);

  start_pos_ = writer->Position();

  const uint64 bytes_written = WriteVoidElement(writer, size + entry_size);
  if (!bytes_written)
    return false;

  return true;
}

bool SeekHead::AddSeekEntry(uint32 id, uint64 pos) {
  for (int32 i = 0; i < kSeekEntryCount; ++i) {
    if (seek_entry_id_[i] == 0) {
      seek_entry_id_[i] = id;
      seek_entry_pos_[i] = pos;
      return true;
    }
  }
  return false;
}

uint64 SeekHead::MaxEntrySize() const {
  const uint64 max_entry_payload_size =
      EbmlElementSize(kMkvSeekID, 0xffffffffULL) +
      EbmlElementSize(kMkvSeekPosition, 0xffffffffffffffffULL);
  const uint64 max_entry_size =
      EbmlMasterElementSize(kMkvSeek, max_entry_payload_size) +
      max_entry_payload_size;

  return max_entry_size;
}

///////////////////////////////////////////////////////////////
//
// SegmentInfo Class

SegmentInfo::SegmentInfo()
    : duration_(-1.0),
      muxing_app_(NULL),
      timecode_scale_(1000000ULL),
      writing_app_(NULL),
      duration_pos_(-1) {
}

SegmentInfo::~SegmentInfo() {
  delete [] muxing_app_;
  delete [] writing_app_;
}

bool SegmentInfo::Init() {
  int32 major;
  int32 minor;
  int32 build;
  int32 revision;
  GetVersion(&major, &minor, &build, &revision);
  char temp[256];
#ifdef _MSC_VER
  sprintf_s(temp,
            sizeof(temp)/sizeof(temp[0]),
            "libwebm-%d.%d.%d.%d",
            major,
            minor,
            build,
            revision);
#else
  snprintf(temp,
           sizeof(temp)/sizeof(temp[0]),
           "libwebm-%d.%d.%d.%d",
           major,
           minor,
           build,
           revision);
#endif

  const size_t app_len = strlen(temp) + 1;

  delete [] muxing_app_;

  muxing_app_ = new (std::nothrow) char[app_len];  // NOLINT
  if (!muxing_app_)
    return false;

#ifdef _MSC_VER
  strcpy_s(muxing_app_, app_len, temp);
#else
  strcpy(muxing_app_, temp);
#endif

  set_writing_app(temp);
  if (!writing_app_)
    return false;
  return true;
}

bool SegmentInfo::Finalize(IMkvWriter* writer) const {
  if (!writer)
    return false;

  if (duration_ > 0.0) {
    if (writer->Seekable()) {
      if (duration_pos_ == -1)
        return false;

      const int64 pos = writer->Position();

      if (writer->Position(duration_pos_))
        return false;

      if (!WriteEbmlElement(writer,
                            kMkvDuration,
                            static_cast<float>(duration_)))
        return false;

      if (writer->Position(pos))
        return false;
    }
  }

  return true;
}

bool SegmentInfo::Write(IMkvWriter* writer) {
  if (!writer || !muxing_app_ || !writing_app_)
    return false;

  uint64 size = EbmlElementSize(kMkvTimecodeScale, timecode_scale_);
  if (duration_ > 0.0)
    size += EbmlElementSize(kMkvDuration, static_cast<float>(duration_));
  size += EbmlElementSize(kMkvMuxingApp, muxing_app_);
  size += EbmlElementSize(kMkvWritingApp, writing_app_);

  if (!WriteEbmlMasterElement(writer, kMkvInfo, size))
    return false;

  const int64 payload_position = writer->Position();
  if (payload_position < 0)
    return false;

  if (!WriteEbmlElement(writer, kMkvTimecodeScale, timecode_scale_))
    return false;

  if (duration_ > 0.0) {
    // Save for later
    duration_pos_ = writer->Position();

    if (!WriteEbmlElement(writer, kMkvDuration, static_cast<float>(duration_)))
      return false;
  }

  if (!WriteEbmlElement(writer, kMkvMuxingApp, muxing_app_))
    return false;
  if (!WriteEbmlElement(writer, kMkvWritingApp, writing_app_))
    return false;

  const int64 stop_position = writer->Position();
  if (stop_position < 0 ||
      stop_position - payload_position != static_cast<int64>(size))
    return false;

  return true;
}

void SegmentInfo::set_writing_app(const char* app) {
  if (app) {
    delete [] writing_app_;

    const size_t length = strlen(app) + 1;
    writing_app_ = new (std::nothrow) char[length];  // NOLINT
    if (writing_app_) {
#ifdef _MSC_VER
      strcpy_s(writing_app_, length, app);
#else
      strcpy(writing_app_, app);
#endif
    }
  }
}

///////////////////////////////////////////////////////////////
//
// Segment Class

Segment::Segment()
    : chunk_count_(0),
      chunk_name_(NULL),
      chunk_writer_cluster_(NULL),
      chunk_writer_cues_(NULL),
      chunk_writer_header_(NULL),
      chunking_(false),
      chunking_base_name_(NULL),
      cluster_list_(NULL),
      cluster_list_capacity_(0),
      cluster_list_size_(0),
      cues_track_(0),
      frames_(NULL),
      frames_capacity_(0),
      frames_size_(0),
      has_video_(false),
      header_written_(false),
      last_timestamp_(0),
      max_cluster_duration_(kDefaultMaxClusterDuration),
      max_cluster_size_(0),
      mode_(kFile),
      new_cuepoint_(false),
      output_cues_(true),
      payload_pos_(0),
      size_position_(0),
      writer_cluster_(NULL),
      writer_cues_(NULL),
      writer_header_(NULL) {
}

Segment::~Segment() {
  if (cluster_list_) {
    for (int32 i = 0; i < cluster_list_size_; ++i) {
      Cluster* const cluster = cluster_list_[i];
      delete cluster;
    }
    delete [] cluster_list_;
  }

  if (frames_) {
    for (int32 i = 0; i < frames_size_; ++i) {
      Frame* const frame = frames_[i];
      delete frame;
    }
    delete [] frames_;
  }

  delete [] chunk_name_;
  delete [] chunking_base_name_;

  if (chunk_writer_cluster_) {
    chunk_writer_cluster_->Close();
    delete chunk_writer_cluster_;
  }
  if (chunk_writer_cues_) {
    chunk_writer_cues_->Close();
    delete chunk_writer_cues_;
  }
  if (chunk_writer_header_) {
    chunk_writer_header_->Close();
    delete chunk_writer_header_;
  }
}

bool Segment::Init(IMkvWriter* ptr_writer) {
  if (!ptr_writer) {
    return false;
  }
  writer_cluster_ = ptr_writer;
  writer_cues_ = ptr_writer;
  writer_header_ = ptr_writer;
  return segment_info_.Init();
}

bool Segment::Finalize() {
  if (WriteFramesAll() < 0)
    return false;

  if (mode_ == kFile) {
    if (cluster_list_size_ > 0) {
      // Update last cluster's size
      Cluster* const old_cluster = cluster_list_[cluster_list_size_-1];

      if (!old_cluster || !old_cluster->Finalize())
        return false;
    }

    if (chunking_ && chunk_writer_cluster_) {
      chunk_writer_cluster_->Close();
      chunk_count_++;
    }

    const double duration =
        static_cast<double>(last_timestamp_) / segment_info_.timecode_scale();
    segment_info_.set_duration(duration);
    if (!segment_info_.Finalize(writer_header_))
      return false;

    // TODO(fgalligan): Add support for putting the Cues at the front.
    if (output_cues_)
      if (!seek_head_.AddSeekEntry(kMkvCues, MaxOffset()))
        return false;

    if (chunking_) {
      if (!chunk_writer_cues_)
        return false;

      char* name = NULL;
      if (!UpdateChunkName("cues", &name))
        return false;

      const bool cues_open = chunk_writer_cues_->Open(name);
      delete [] name;
      if (!cues_open)
        return false;
    }

    if (output_cues_)
      if (!cues_.Write(writer_cues_))
        return false;

    if (!seek_head_.Finalize(writer_header_))
      return false;

    if (writer_header_->Seekable()) {
      if (size_position_ == -1)
        return false;

      const int64 pos = writer_header_->Position();
      const int64 segment_size = MaxOffset();

      if (segment_size < 1)
        return false;

      if (writer_header_->Position(size_position_))
        return false;

      if (WriteUIntSize(writer_header_, segment_size, 8))
        return false;

      if (writer_header_->Position(pos))
        return false;
    }

    if (chunking_) {
      // Do not close any writers until the segment size has been written,
      // otherwise the size may be off.
      if (!chunk_writer_cues_ || !chunk_writer_header_)
        return false;

      chunk_writer_cues_->Close();
      chunk_writer_header_->Close();
    }
  }

  return true;
}

uint64 Segment::AddVideoTrack(int32 width, int32 height, int32 number) {
  VideoTrack* const vid_track = new (std::nothrow) VideoTrack();  // NOLINT
  if (!vid_track)
    return 0;

  vid_track->set_type(Tracks::kVideo);
  vid_track->set_codec_id(Tracks::kVp8CodecId);
  vid_track->set_width(width);
  vid_track->set_height(height);

  tracks_.AddTrack(vid_track, number);
  has_video_ = true;

  return vid_track->number();
}

uint64 Segment::AddAudioTrack(int32 sample_rate,
                              int32 channels,
                              int32 number) {
  AudioTrack* const aud_track = new (std::nothrow) AudioTrack();  // NOLINT
  if (!aud_track)
    return 0;

  aud_track->set_type(Tracks::kAudio);
  aud_track->set_codec_id(Tracks::kVorbisCodecId);
  aud_track->set_sample_rate(sample_rate);
  aud_track->set_channels(channels);

  tracks_.AddTrack(aud_track, number);

  return aud_track->number();
}

bool Segment::AddFrame(const uint8* frame,
                       uint64 length,
                       uint64 track_number,
                       uint64 timestamp,
                       bool is_key) {
  if (!frame)
    return false;

  if (!CheckHeaderInfo())
    return false;

  // Check for non-monotonically increasing timestamps.
  if (timestamp < last_timestamp_)
    return false;

  // If the segment has a video track hold onto audio frames to make sure the
  // audio that is associated with the start time of a video key-frame is
  // muxed into the same cluster.
  if (has_video_ && tracks_.TrackIsAudio(track_number)) {
    Frame* const new_frame = new Frame();
    if (!new_frame->Init(frame, length))
      return false;
    new_frame->set_track_number(track_number);
    new_frame->set_timestamp(timestamp);
    new_frame->set_is_key(is_key);

    if (!QueueFrame(new_frame))
      return false;

    return true;
  }

  if (!DoNewClusterProcessing(track_number, timestamp, is_key))
    return false;

  if (cluster_list_size_ < 1)
    return false;

  Cluster* const cluster = cluster_list_[cluster_list_size_ - 1];
  if (!cluster)
    return false;

  const uint64 timecode_scale = segment_info_.timecode_scale();
  const uint64 abs_timecode = timestamp / timecode_scale;

  if (!cluster->AddFrame(frame,
                         length,
                         track_number,
                         abs_timecode,
                         is_key))
    return false;

  if (new_cuepoint_ && cues_track_ == track_number) {
    if (!AddCuePoint(timestamp))
      return false;
  }

  if (timestamp > last_timestamp_)
    last_timestamp_ = timestamp;

  return true;
}

void Segment::OutputCues(bool output_cues) {
  output_cues_ = output_cues;
}

bool Segment::SetChunking(bool chunking, const char* filename) {
  if (chunk_count_ > 0)
    return false;

  if (chunking) {
    if (!filename)
      return false;

    // Check if we are being set to what is already set.
    if (chunking_ && !strcmp(filename, chunking_base_name_))
      return true;

    const size_t name_length = strlen(filename) + 1;
    char* const temp = new (std::nothrow) char[name_length];  // NOLINT
    if (!temp)
      return false;

#ifdef _MSC_VER
    strcpy_s(temp, name_length, filename);
#else
    strcpy(temp, filename);
#endif

    delete [] chunking_base_name_;
    chunking_base_name_ = temp;

    if (!UpdateChunkName("chk", &chunk_name_))
      return false;

    if (!chunk_writer_cluster_) {
      chunk_writer_cluster_ = new (std::nothrow) MkvWriter();  // NOLINT
      if (!chunk_writer_cluster_)
        return false;
    }

    if (!chunk_writer_cues_) {
      chunk_writer_cues_ = new (std::nothrow) MkvWriter();  // NOLINT
      if (!chunk_writer_cues_)
        return false;
    }

    if (!chunk_writer_header_) {
      chunk_writer_header_ = new (std::nothrow) MkvWriter();  // NOLINT
      if (!chunk_writer_header_)
        return false;
    }

    if (!chunk_writer_cluster_->Open(chunk_name_))
      return false;

    const size_t header_length = strlen(filename) + strlen(".hdr") + 1;
    char* const header = new (std::nothrow) char[header_length];  // NOLINT
    if (!header)
      return false;

#ifdef _MSC_VER
    strcpy_s(header, header_length - strlen(".hdr"), chunking_base_name_);
    strcat_s(header, header_length, ".hdr");
#else
    strcpy(header, chunking_base_name_);
    strcat(header, ".hdr");
#endif
    if (!chunk_writer_header_->Open(header)) {
      delete [] header;
      return false;
    }

    writer_cluster_ = chunk_writer_cluster_;
    writer_cues_ = chunk_writer_cues_;
    writer_header_ = chunk_writer_header_;

    delete [] header;
  }

  chunking_ = chunking;

  return true;
}

bool Segment::CuesTrack(uint64 track_number) {
  const Track* const track = GetTrackByNumber(track_number);
  if (!track)
    return false;

  cues_track_ = track_number;
  return true;
}

Track* Segment::GetTrackByNumber(uint64 track_number) const {
  return tracks_.GetTrackByNumber(track_number);
}

bool Segment::WriteSegmentHeader() {
  // TODO(fgalligan): Support more than one segment.
  if (!WriteEbmlHeader(writer_header_))
    return false;

  // Write "unknown" (-1) as segment size value. If mode is kFile, Segment
  // will write over duration when the file is finalized.
  if (WriteID(writer_header_, kMkvSegment))
    return false;

  // Save for later.
  size_position_ = writer_header_->Position();

  // Write "unknown" (EBML coded -1) as segment size value. We need to write 8
  // bytes because if we are going to overwrite the segment size later we do
  // not know how big our segment will be.
  if (SerializeInt(writer_header_, kEbmlUnknownValue, 8))
    return false;

  payload_pos_ =  writer_header_->Position();

  if (mode_ == kFile && writer_header_->Seekable()) {
    // Set the duration > 0.0 so SegmentInfo will write out the duration. When
    // the muxer is done writing we will set the correct duration and have
    // SegmentInfo upadte it.
    segment_info_.set_duration(1.0);

    if (!seek_head_.Write(writer_header_))
      return false;
  }

  if (!seek_head_.AddSeekEntry(kMkvInfo, MaxOffset()))
    return false;
  if (!segment_info_.Write(writer_header_))
    return false;

  if (!seek_head_.AddSeekEntry(kMkvTracks, MaxOffset()))
    return false;
  if (!tracks_.Write(writer_header_))
    return false;

  if (chunking_ && (mode_ == kLive || !writer_header_->Seekable())) {
    if (!chunk_writer_header_)
      return false;

    chunk_writer_header_->Close();
  }

  header_written_ = true;

  return true;
}

// Here we are testing whether to create a new cluster, given a frame
// having time frame_timestamp_ns.
//
int Segment::TestFrame(uint64 track_number,
                       uint64 frame_timestamp_ns,
                       bool is_key) const {
  // If no clusters have been created yet, then create a new cluster
  // and write this frame immediately, in the new cluster.  This path
  // should only be followed once, the first time we attempt to write
  // a frame.

  if (cluster_list_size_ <= 0)
    return 1;

  // There exists at least one cluster. We must compare the frame to
  // the last cluster, in order to determine whether the frame is
  // written to the existing cluster, or that a new cluster should be
  // created.

  const uint64 timecode_scale = segment_info_.timecode_scale();
  const uint64 frame_timecode = frame_timestamp_ns / timecode_scale;

  const Cluster* const last_cluster = cluster_list_[cluster_list_size_ - 1];
  const uint64 last_cluster_timecode = last_cluster->timecode();

  // For completeness we test for the case when the frame's timecode
  // is less than the cluster's timecode.  Although in principle that
  // is allowed, this muxer doesn't actually write clusters like that,
  // so this indicates a bug somewhere in our algorithm.

  if (frame_timecode < last_cluster_timecode)  // should never happen
    return -1;  // error

  // Handle the case when the frame we are testing has a timestamp
  // equal to the cluster's timestamp.  This can happen if some
  // non-video keyframe (that is, a WebVTT cue or audio block) first
  // creates the initial cluster (at t=0), and then we test a video
  // keyframe.  We don't want to create a new cluster just yet (see
  // the predicate below, which specifies the creation of a new
  // cluster when a video keyframe is detected); instead we want to
  // force the frame to be written to the existing cluster.

  if (frame_timecode == last_cluster_timecode)
    return 0;

  // If the frame has a timestamp significantly larger than the last
  // cluster (in Matroska, cluster-relative timestamps are serialized
  // using a 16-bit signed integer), then we cannot write this frame
  // that cluster, and so we must create a new cluster.

  const int64 delta_timecode = frame_timecode - last_cluster_timecode;

  if (delta_timecode > std::numeric_limits<int16>::max())
    return 1;

  // We decide to create a new cluster when we have a video keyframe.
  // This will flush queued (audio) frames, and write the keyframe
  // immediately, in the newly-created cluster.

  if (is_key && tracks_.TrackIsVideo(track_number))
    return 1;

  // Create a new cluster if we have accumulated too many frames
  // already, where "too many" is defined as "the total time of frames
  // in the cluster exceeds a threshold".

  const uint64 delta_ns = delta_timecode * timecode_scale;

  if (max_cluster_duration_ > 0 && delta_ns >= max_cluster_duration_)
    return 1;

  // This is similar to the case above, with the difference that a new
  // cluster is created when the size of the current cluster exceeds a
  // threshold.

  const uint64 cluster_size = last_cluster->payload_size();

  if (max_cluster_size_ > 0 && cluster_size >= max_cluster_size_)
    return 1;

  // There's no need to create a new cluster, so emit this frame now.

  return 0;
}

bool Segment::MakeNewCluster(uint64 frame_timestamp_ns) {
  const int32 new_size = cluster_list_size_ + 1;

  if (new_size > cluster_list_capacity_) {
    // Add more clusters.
    const int32 new_capacity =
        (cluster_list_capacity_ <= 0) ? 1 : cluster_list_capacity_ * 2;
    Cluster** const clusters =
        new (std::nothrow) Cluster*[new_capacity];  // NOLINT
    if (!clusters)
      return false;

    for (int32 i = 0; i < cluster_list_size_; ++i) {
      clusters[i] = cluster_list_[i];
    }

    delete [] cluster_list_;

    cluster_list_ = clusters;
    cluster_list_capacity_ = new_capacity;
  }

  if (!WriteFramesLessThan(frame_timestamp_ns))
    return false;

  if (mode_ == kFile) {
    if (cluster_list_size_ > 0) {
      // Update old cluster's size
      Cluster* const old_cluster = cluster_list_[cluster_list_size_ - 1];

      if (!old_cluster || !old_cluster->Finalize())
        return false;
    }

    if (output_cues_)
      new_cuepoint_ = true;
  }

  if (chunking_ && cluster_list_size_ > 0) {
    chunk_writer_cluster_->Close();
    chunk_count_++;

    if (!UpdateChunkName("chk", &chunk_name_))
      return false;
    if (!chunk_writer_cluster_->Open(chunk_name_))
      return false;
  }

  const uint64 timecode_scale = segment_info_.timecode_scale();
  const uint64 frame_timecode = frame_timestamp_ns / timecode_scale;

  uint64 cluster_timecode = frame_timecode;

  if (frames_size_ > 0) {
    const Frame* const f = frames_[0];  // earliest queued frame
    const uint64 ns = f->timestamp();
    const uint64 tc = ns / timecode_scale;

    if (tc < cluster_timecode)
      cluster_timecode = tc;
  }

  Cluster*& cluster = cluster_list_[cluster_list_size_];
  const int64 offset = MaxOffset();
  cluster = new (std::nothrow) Cluster(cluster_timecode, offset);  // NOLINT
  if (!cluster)
    return false;

  if (!cluster->Init(writer_cluster_))
    return false;

  cluster_list_size_ = new_size;
  return true;
}

bool Segment::DoNewClusterProcessing(uint64 track_number,
                                     uint64 frame_timestamp_ns,
                                     bool is_key) {
  // Based on the characteristics of the current frame and current
  // cluster, decide whether to create a new cluster.
  const int result = TestFrame(track_number, frame_timestamp_ns, is_key);
  if (result < 0)  // error
    return false;

  // A non-zero result means create a new cluster.
  if (result > 0 && !MakeNewCluster(frame_timestamp_ns))
    return false;

  // Write queued (audio) frames.
  const int frame_count = WriteFramesAll();
  if (frame_count < 0)  // error
    return false;

  // Write the current frame to the current cluster (if TestFrame
  // returns 0) or to a newly created cluster (TestFrame returns 1).
  return true;
}

bool Segment::CheckHeaderInfo() {
  if (!header_written_) {
    if (!WriteSegmentHeader())
      return false;

    if (!seek_head_.AddSeekEntry(kMkvCluster, MaxOffset()))
      return false;

    if (output_cues_ && cues_track_ == 0) {
      // Check for a video track
      for (uint32 i = 0; i < tracks_.track_entries_size(); ++i) {
        const Track* const track = tracks_.GetTrackByIndex(i);
        if (!track)
          return false;

        if (tracks_.TrackIsVideo(track->number())) {
          cues_track_ = track->number();
          break;
        }
      }

      // Set first track found
      if (cues_track_ == 0) {
        const Track* const track = tracks_.GetTrackByIndex(0);
        if (!track)
          return false;

        cues_track_ = track->number();
      }
    }
  }
  return true;
}

bool Segment::AddCuePoint(uint64 timestamp) {
  if (cluster_list_size_  < 1)
    return false;

  const Cluster* const cluster = cluster_list_[cluster_list_size_-1];
  if (!cluster)
    return false;

  CuePoint* const cue = new (std::nothrow) CuePoint();  // NOLINT
  if (!cue)
    return false;

  cue->set_time(timestamp / segment_info_.timecode_scale());
  cue->set_block_number(cluster->blocks_added() + 1);
  cue->set_cluster_pos(cluster->position_for_cues());
  cue->set_track(cues_track_);
  if (!cues_.AddCue(cue))
    return false;

  new_cuepoint_ = false;
  return true;
}

bool Segment::UpdateChunkName(const char* ext, char** name) const {
  if (!name || !ext)
    return false;

  char ext_chk[64];
#ifdef _MSC_VER
  sprintf_s(ext_chk, sizeof(ext_chk), "_%06d.%s", chunk_count_, ext);
#else
  snprintf(ext_chk, sizeof(ext_chk), "_%06d.%s", chunk_count_, ext);
#endif

  const size_t length = strlen(chunking_base_name_) + strlen(ext_chk) + 1;
  char* const str = new (std::nothrow) char[length];  // NOLINT
  if (!str)
    return false;

#ifdef _MSC_VER
  strcpy_s(str, length-strlen(ext_chk), chunking_base_name_);
  strcat_s(str, length, ext_chk);
#else
  strcpy(str, chunking_base_name_);
  strcat(str, ext_chk);
#endif

  delete [] *name;
  *name = str;

  return true;
}

int64 Segment::MaxOffset() {
  if (!writer_header_)
    return -1;

  int64 offset = writer_header_->Position() - payload_pos_;

  if (chunking_) {
    for (int32 i = 0; i < cluster_list_size_; ++i) {
      Cluster* const cluster = cluster_list_[i];
      offset += cluster->Size();
    }

    if (writer_cues_)
      offset += writer_cues_->Position();
  }

  return offset;
}

bool Segment::QueueFrame(Frame* frame) {
  const int32 new_size = frames_size_ + 1;

  if (new_size > frames_capacity_) {
    // Add more frames.
    const int32 new_capacity = (!frames_capacity_) ? 2 : frames_capacity_ * 2;

    if (new_capacity < 1)
      return false;

    Frame** const frames = new (std::nothrow) Frame*[new_capacity];  // NOLINT
    if (!frames)
      return false;

    for (int32 i = 0; i < frames_size_; ++i) {
      frames[i] = frames_[i];
    }

    delete [] frames_;
    frames_ = frames;
    frames_capacity_ = new_capacity;
  }

  frames_[frames_size_++] = frame;

  return true;
}

int Segment::WriteFramesAll() {
  if (frames_ == NULL)
    return 0;

  if (cluster_list_size_ < 1)
    return -1;

  Cluster* const cluster = cluster_list_[cluster_list_size_-1];

  if (!cluster)
    return -1;

  const uint64 timecode_scale = segment_info_.timecode_scale();

  for (int32 i = 0; i < frames_size_; ++i) {
    Frame*& frame = frames_[i];
    const uint64 frame_timestamp = frame->timestamp();  // ns
    const uint64 frame_timecode = frame_timestamp / timecode_scale;

    if (!cluster->AddFrame(frame->frame(),
                           frame->length(),
                           frame->track_number(),
                           frame_timecode,
                           frame->is_key()))
      return -1;

    if (new_cuepoint_ && cues_track_ == frame->track_number()) {
      if (!AddCuePoint(frame_timestamp))
        return -1;
    }

    if (frame_timestamp > last_timestamp_)
      last_timestamp_ = frame_timestamp;

    delete frame;
    frame = NULL;
  }

  const int result = frames_size_;
  frames_size_ = 0;

  return result;
}

bool Segment::WriteFramesLessThan(uint64 timestamp) {
  // Check |cluster_list_size_| to see if this is the first cluster. If it is
  // the first cluster the audio frames that are less than the first video
  // timesatmp will be written in a later step.
  if (frames_size_ > 0 && cluster_list_size_ > 0) {
    if (!frames_)
      return false;

    Cluster* const cluster = cluster_list_[cluster_list_size_-1];
    if (!cluster)
      return false;

    const uint64 timecode_scale = segment_info_.timecode_scale();
    int32 shift_left = 0;

    // TODO(fgalligan): Change this to use the durations of frames instead of
    // the next frame's start time if the duration is accurate.
    for (int32 i = 1; i < frames_size_; ++i) {
      const Frame* const frame_curr = frames_[i];

      if (frame_curr->timestamp() > timestamp)
        break;

      const Frame* const frame_prev = frames_[i-1];
      const uint64 frame_timestamp = frame_prev->timestamp();
      const uint64 frame_timecode = frame_timestamp / timecode_scale;

      if (!cluster->AddFrame(frame_prev->frame(),
                             frame_prev->length(),
                             frame_prev->track_number(),
                             frame_timecode,
                             frame_prev->is_key()))
        return false;

      if (new_cuepoint_ && cues_track_ == frame_prev->track_number()) {
        if (!AddCuePoint(frame_timestamp))
          return false;
      }

      ++shift_left;
      if (frame_timestamp > last_timestamp_)
        last_timestamp_ = frame_timestamp;

      delete frame_prev;
    }

    if (shift_left > 0) {
      if (shift_left >= frames_size_)
        return false;

      const int32 new_frames_size = frames_size_ - shift_left;
      for (int32 i = 0; i < new_frames_size; ++i) {
        frames_[i] = frames_[i+shift_left];
      }

      frames_size_ = new_frames_size;
    }
  }

  return true;
}

}  // namespace mkvmuxer
