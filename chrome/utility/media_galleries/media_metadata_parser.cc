// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/media_metadata_parser.h"

#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
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

// This runs on |media_thread_|, as the underlying FFmpeg operation is
// blocking, and the utility thread must not be blocked, so the media file
// bytes can be sent from the browser process to the utility process.
scoped_ptr<MediaMetadataParser::MediaMetadata> ParseAudioVideoMetadata(
    media::DataSource* source,
    scoped_ptr<MediaMetadataParser::MediaMetadata> metadata) {
  DCHECK(source);
  media::AudioVideoMetadataExtractor extractor;

  if (!extractor.Extract(source))
    return metadata.Pass();

  if (extractor.duration() >= 0)
    metadata->duration.reset(new double(extractor.duration()));

  if (extractor.height() >= 0 && extractor.width() >= 0) {
    metadata->height.reset(new int(extractor.height()));
    metadata->width.reset(new int(extractor.width()));
  }

  SetStringScopedPtr(extractor.artist(), &metadata->artist);
  SetStringScopedPtr(extractor.album(), &metadata->album);
  SetStringScopedPtr(extractor.artist(), &metadata->artist);
  SetStringScopedPtr(extractor.comment(), &metadata->comment);
  SetStringScopedPtr(extractor.copyright(), &metadata->copyright);
  SetIntScopedPtr(extractor.disc(), &metadata->disc);
  SetStringScopedPtr(extractor.genre(), &metadata->genre);
  SetStringScopedPtr(extractor.language(), &metadata->language);
  SetIntScopedPtr(extractor.rotation(), &metadata->rotation);
  SetStringScopedPtr(extractor.title(), &metadata->title);
  SetIntScopedPtr(extractor.track(), &metadata->track);

  for (std::map<std::string, std::string>::const_iterator it =
           extractor.raw_tags().begin();
       it != extractor.raw_tags().end(); ++it) {
    metadata->raw_tags.additional_properties.SetString(it->first, it->second);
  }

  return metadata.Pass();
}

}  // namespace

MediaMetadataParser::MediaMetadataParser(media::DataSource* source,
                                         const std::string& mime_type)
    : source_(source),
      mime_type_(mime_type) {
}

MediaMetadataParser::~MediaMetadataParser() {}

void MediaMetadataParser::Start(const MetadataCallback& callback) {
  scoped_ptr<MediaMetadata> metadata(new MediaMetadata);
  metadata->mime_type = mime_type_;

  if (StartsWithASCII(mime_type_, "audio/", true) ||
      StartsWithASCII(mime_type_, "video/", true)) {
    media_thread_.reset(new base::Thread("media_thread"));
    CHECK(media_thread_->Start());
    base::PostTaskAndReplyWithResult(
        media_thread_->message_loop_proxy(),
        FROM_HERE,
        base::Bind(&ParseAudioVideoMetadata, source_, base::Passed(&metadata)),
        callback);
    return;
  }

  // TODO(tommycli): Implement for image mime types.
  callback.Run(metadata.Pass());
}

}  // namespace metadata
