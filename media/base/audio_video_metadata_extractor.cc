// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_video_metadata_extractor.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/blocking_url_protocol.h"
#include "media/filters/ffmpeg_glue.h"

namespace media {

namespace {

void OnError(bool* succeeded) {
  *succeeded = false;
}

// Returns true if the |tag| matches |expected_key|.
bool ExtractString(AVDictionaryEntry* tag, const char* expected_key,
                   std::string* destination) {
  if (!LowerCaseEqualsASCII(std::string(tag->key), expected_key))
    return false;

  if (destination->empty())
    *destination = tag->value;

  return true;
}

// Returns true if the |tag| matches |expected_key|.
bool ExtractInt(AVDictionaryEntry* tag, const char* expected_key,
                int* destination) {
  if (!LowerCaseEqualsASCII(std::string(tag->key), expected_key))
    return false;

  int temporary = -1;
  if (*destination < 0 && base::StringToInt(tag->value, &temporary) &&
      temporary >= 0) {
    *destination = temporary;
  }

  return true;
}

}  // namespace

AudioVideoMetadataExtractor::AudioVideoMetadataExtractor()
    : extracted_(false),
      duration_(-1),
      width_(-1),
      height_(-1),
      disc_(-1),
      rotation_(-1),
      track_(-1) {
}

AudioVideoMetadataExtractor::~AudioVideoMetadataExtractor() {
}

bool AudioVideoMetadataExtractor::Extract(DataSource* source) {
  DCHECK(!extracted_);

  bool read_ok = true;
  media::BlockingUrlProtocol protocol(source, base::Bind(&OnError, &read_ok));
  media::FFmpegGlue glue(&protocol);
  AVFormatContext* format_context = glue.format_context();

  if (!glue.OpenContext())
    return false;

  if (!read_ok)
    return false;

  if (!format_context->iformat)
    return false;

  if (avformat_find_stream_info(format_context, NULL) < 0)
    return false;

  if (format_context->duration != AV_NOPTS_VALUE)
    duration_ = static_cast<double>(format_context->duration) / AV_TIME_BASE;

  ExtractDictionary(format_context->metadata);

  for (unsigned int i = 0; i < format_context->nb_streams; ++i) {
    AVStream* stream = format_context->streams[i];
    if (!stream)
      continue;

    // Ignore attached pictures for metadata extraction.
    if ((stream->disposition & AV_DISPOSITION_ATTACHED_PIC) != 0)
      continue;

    // Extract dictionary from streams also. Needed for containers that attach
    // metadata to contained streams instead the container itself, like OGG.
    ExtractDictionary(stream->metadata);

    if (!stream->codec)
      continue;

    // Extract dimensions of largest stream that's not an attached picture.
    if (stream->codec->width > 0 && stream->codec->width > width_ &&
        stream->codec->height > 0 && stream->codec->height > height_) {
      width_ = stream->codec->width;
      height_ = stream->codec->height;
    }
  }

  extracted_ = true;
  return true;
}

double AudioVideoMetadataExtractor::duration() const {
  DCHECK(extracted_);
  return duration_;
}

int AudioVideoMetadataExtractor::width() const {
  DCHECK(extracted_);
  return width_;
}

int AudioVideoMetadataExtractor::height() const {
  DCHECK(extracted_);
  return height_;
}

int AudioVideoMetadataExtractor::rotation() const {
  DCHECK(extracted_);
  return rotation_;
}

const std::string& AudioVideoMetadataExtractor::album() const {
  DCHECK(extracted_);
  return album_;
}

const std::string& AudioVideoMetadataExtractor::artist() const {
  DCHECK(extracted_);
  return artist_;
}

const std::string& AudioVideoMetadataExtractor::comment() const {
  DCHECK(extracted_);
  return comment_;
}

const std::string& AudioVideoMetadataExtractor::copyright() const {
  DCHECK(extracted_);
  return copyright_;
}

const std::string& AudioVideoMetadataExtractor::date() const {
  DCHECK(extracted_);
  return date_;
}

int AudioVideoMetadataExtractor::disc() const {
  DCHECK(extracted_);
  return disc_;
}

const std::string& AudioVideoMetadataExtractor::encoder() const {
  DCHECK(extracted_);
  return encoder_;
}

const std::string& AudioVideoMetadataExtractor::encoded_by() const {
  DCHECK(extracted_);
  return encoded_by_;
}

const std::string& AudioVideoMetadataExtractor::genre() const {
  DCHECK(extracted_);
  return genre_;
}

const std::string& AudioVideoMetadataExtractor::language() const {
  DCHECK(extracted_);
  return language_;
}

const std::string& AudioVideoMetadataExtractor::title() const {
  DCHECK(extracted_);
  return title_;
}

int AudioVideoMetadataExtractor::track() const {
  DCHECK(extracted_);
  return track_;
}

const std::map<std::string, std::string>&
AudioVideoMetadataExtractor::raw_tags() const {
  DCHECK(extracted_);
  return raw_tags_;
}

void AudioVideoMetadataExtractor::ExtractDictionary(AVDictionary* metadata) {
  if (!metadata)
    return;

  AVDictionaryEntry* tag = NULL;
  while ((tag = av_dict_get(metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
    if (raw_tags_.find(tag->key) == raw_tags_.end())
      raw_tags_[tag->key] = tag->value;

    if (ExtractInt(tag, "rotate", &rotation_)) continue;
    if (ExtractString(tag, "album", &album_)) continue;
    if (ExtractString(tag, "artist", &artist_)) continue;
    if (ExtractString(tag, "comment", &comment_)) continue;
    if (ExtractString(tag, "copyright", &copyright_)) continue;
    if (ExtractString(tag, "date", &date_)) continue;
    if (ExtractInt(tag, "disc", &disc_)) continue;
    if (ExtractString(tag, "encoder", &encoder_)) continue;
    if (ExtractString(tag, "encoded_by", &encoded_by_)) continue;
    if (ExtractString(tag, "genre", &genre_)) continue;
    if (ExtractString(tag, "language", &language_)) continue;
    if (ExtractString(tag, "title", &title_)) continue;
    if (ExtractInt(tag, "track", &track_)) continue;
  }
}

}  // namespace media
