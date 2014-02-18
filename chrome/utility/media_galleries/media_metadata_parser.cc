// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/media_metadata_parser.h"

#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "media/base/audio_video_metadata_extractor.h"
#include "media/base/data_source.h"

namespace metadata {

namespace {

void SetStringScopedPtr(const std::string& value,
                        scoped_ptr<std::string>* destination) {
  if (!value.empty())
    destination->reset(new std::string(value));
}

void SetIntScopedPtr(int value, scoped_ptr<int>* destination) {
  if (value >= 0)
    destination->reset(new int(value));
}

}  // namespace

MediaMetadataParser::MediaMetadataParser(media::DataSource* source,
                                         const std::string& mime_type)
    : source_(source),
      metadata_(new MediaMetadata) {
  metadata_->mime_type = mime_type;
}

MediaMetadataParser::~MediaMetadataParser() {}

void MediaMetadataParser::Start(const MetadataCallback& callback) {
  DCHECK(callback_.is_null());
  callback_ = callback;

  if (StartsWithASCII(metadata_->mime_type, "audio/", true) ||
      StartsWithASCII(metadata_->mime_type, "video/", true)) {
    PopulateAudioVideoMetadata();
  }

  // TODO(tommycli): Implement for image mime types.
  callback_.Run(metadata_.Pass());
}

void MediaMetadataParser::PopulateAudioVideoMetadata() {
  DCHECK(source_);
  DCHECK(metadata_.get());
  media::AudioVideoMetadataExtractor extractor;

  if (!extractor.Extract(source_))
    return;

  if (extractor.duration() >= 0)
    metadata_->duration.reset(new double(extractor.duration()));

  if (extractor.height() >= 0 && extractor.width() >= 0) {
    metadata_->height.reset(new int(extractor.height()));
    metadata_->width.reset(new int(extractor.width()));
  }

  SetStringScopedPtr(extractor.artist(), &metadata_->artist);
  SetStringScopedPtr(extractor.album(), &metadata_->album);
  SetStringScopedPtr(extractor.artist(), &metadata_->artist);
  SetStringScopedPtr(extractor.comment(), &metadata_->comment);
  SetStringScopedPtr(extractor.copyright(), &metadata_->copyright);
  SetIntScopedPtr(extractor.disc(), &metadata_->disc);
  SetStringScopedPtr(extractor.genre(), &metadata_->genre);
  SetStringScopedPtr(extractor.language(), &metadata_->language);
  SetIntScopedPtr(extractor.rotation(), &metadata_->rotation);
  SetStringScopedPtr(extractor.title(), &metadata_->title);
  SetIntScopedPtr(extractor.track(), &metadata_->track);
}

}  // namespace metadata
