// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/media_metadata_parser.h"

#include <string>

#include "base/bind.h"
#include "base/memory/linked_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "chrome/utility/media_galleries/image_metadata_extractor.h"
#include "media/base/audio_video_metadata_extractor.h"
#include "media/base/data_source.h"
#include "net/base/mime_sniffer.h"

namespace MediaGalleries = extensions::api::media_galleries;

namespace metadata {

namespace {

void SetStringScopedPtr(const std::string& value,
                        scoped_ptr<std::string>* destination) {
  DCHECK(destination);
  if (!value.empty())
    destination->reset(new std::string(value));
}

void SetIntScopedPtr(int value, scoped_ptr<int>* destination) {
  DCHECK(destination);
  if (value >= 0)
    destination->reset(new int(value));
}

void SetDoubleScopedPtr(double value, scoped_ptr<double>* destination) {
  DCHECK(destination);
  if (value >= 0)
    destination->reset(new double(value));
}

void SetBoolScopedPtr(bool value, scoped_ptr<bool>* destination) {
  DCHECK(destination);
  destination->reset(new bool(value));
}

// This runs on |media_thread_|, as the underlying FFmpeg operation is
// blocking, and the utility thread must not be blocked, so the media file
// bytes can be sent from the browser process to the utility process.
void ParseAudioVideoMetadata(
    media::DataSource* source, bool get_attached_images,
    MediaMetadataParser::MediaMetadata* metadata,
    std::vector<AttachedImage>* attached_images) {
  DCHECK(source);
  DCHECK(metadata);
  media::AudioVideoMetadataExtractor extractor;

  if (!extractor.Extract(source, get_attached_images))
    return;

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

  for (media::AudioVideoMetadataExtractor::StreamInfoVector::const_iterator it =
           extractor.stream_infos().begin();
       it != extractor.stream_infos().end(); ++it) {
    linked_ptr<MediaGalleries::StreamInfo> stream_info(
        new MediaGalleries::StreamInfo);
    stream_info->type = it->type;

    for (std::map<std::string, std::string>::const_iterator tag_it =
             it->tags.begin();
         tag_it != it->tags.end(); ++tag_it) {
      stream_info->tags.additional_properties.SetString(tag_it->first,
                                                        tag_it->second);
    }

    metadata->raw_tags.push_back(stream_info);
  }

  if (get_attached_images) {
    for (std::vector<std::string>::const_iterator it =
             extractor.attached_images_bytes().begin();
         it != extractor.attached_images_bytes().end(); ++it) {
      attached_images->push_back(AttachedImage());
      attached_images->back().data = *it;
      net::SniffMimeTypeFromLocalData(it->c_str(), it->length(),
                                      &attached_images->back().type);
    }
  }
}

void FinishParseAudioVideoMetadata(
    MediaMetadataParser::MetadataCallback callback,
    MediaMetadataParser::MediaMetadata* metadata,
    std::vector<AttachedImage>* attached_images) {
  DCHECK(!callback.is_null());
  DCHECK(metadata);
  DCHECK(attached_images);

  callback.Run(*metadata, *attached_images);
}

void FinishParseImageMetadata(
    ImageMetadataExtractor* extractor, const std::string& mime_type,
    MediaMetadataParser::MetadataCallback callback, bool extract_success) {
  DCHECK(extractor);
  MediaMetadataParser::MediaMetadata metadata;
  metadata.mime_type = mime_type;

  if (!extract_success) {
    callback.Run(metadata, std::vector<AttachedImage>());
    return;
  }

  SetIntScopedPtr(extractor->height(), &metadata.height);
  SetIntScopedPtr(extractor->width(), &metadata.width);

  SetIntScopedPtr(extractor->rotation(), &metadata.rotation);

  SetDoubleScopedPtr(extractor->x_resolution(), &metadata.x_resolution);
  SetDoubleScopedPtr(extractor->y_resolution(), &metadata.y_resolution);
  SetBoolScopedPtr(extractor->flash_fired(), &metadata.flash_fired);
  SetStringScopedPtr(extractor->camera_make(), &metadata.camera_make);
  SetStringScopedPtr(extractor->camera_model(), &metadata.camera_model);
  SetDoubleScopedPtr(extractor->exposure_time_sec(),
                     &metadata.exposure_time_seconds);

  SetDoubleScopedPtr(extractor->f_number(), &metadata.f_number);
  SetDoubleScopedPtr(extractor->focal_length_mm(), &metadata.focal_length_mm);
  SetDoubleScopedPtr(extractor->iso_equivalent(), &metadata.iso_equivalent);

  callback.Run(metadata, std::vector<AttachedImage>());
}

}  // namespace

MediaMetadataParser::MediaMetadataParser(media::DataSource* source,
                                         const std::string& mime_type,
                                         bool get_attached_images)
    : source_(source),
      mime_type_(mime_type),
      get_attached_images_(get_attached_images) {
}

MediaMetadataParser::~MediaMetadataParser() {}

void MediaMetadataParser::Start(const MetadataCallback& callback) {
  if (StartsWithASCII(mime_type_, "audio/", true) ||
      StartsWithASCII(mime_type_, "video/", true)) {
    MediaMetadata* metadata = new MediaMetadata;
    metadata->mime_type = mime_type_;
    std::vector<AttachedImage>* attached_images =
        new std::vector<AttachedImage>;

    media_thread_.reset(new base::Thread("media_thread"));
    CHECK(media_thread_->Start());
    media_thread_->message_loop_proxy()->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&ParseAudioVideoMetadata, source_, get_attached_images_,
                   metadata, attached_images),
        base::Bind(&FinishParseAudioVideoMetadata, callback,
                   base::Owned(metadata), base::Owned(attached_images)));
    return;
  }

  if (StartsWithASCII(mime_type_, "image/", true)) {
    ImageMetadataExtractor* extractor = new ImageMetadataExtractor;
    extractor->Extract(
        source_,
        base::Bind(&FinishParseImageMetadata, base::Owned(extractor),
                   mime_type_, callback));
    return;
  }

  callback.Run(MediaMetadata(), std::vector<AttachedImage>());
}

}  // namespace metadata
