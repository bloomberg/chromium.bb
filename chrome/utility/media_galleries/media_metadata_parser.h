// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_MEDIA_GALLERIES_MEDIA_METADATA_PARSER_H_
#define CHROME_UTILITY_MEDIA_GALLERIES_MEDIA_METADATA_PARSER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/common/extensions/api/media_galleries.h"
#include "chrome/common/media_galleries/metadata_types.h"

namespace base {
class Thread;
}

namespace media {
class DataSource;
}

namespace metadata {

// This class takes a MIME type and data source and parses its metadata. It
// handles audio, video, and images. It delegates its operations to FFMPEG.
// This class lives and operates on the utility thread of the utility process
// so we sandbox potentially dangerous operations on user-provided data.
class MediaMetadataParser {
 public:
  typedef extensions::api::media_galleries::MediaMetadata MediaMetadata;
  typedef base::Callback<
      void(const MediaMetadata& metadata,
           const std::vector<AttachedImage>& attached_images)>
  MetadataCallback;

  MediaMetadataParser(std::unique_ptr<media::DataSource> source,
                      const std::string& mime_type,
                      bool get_attached_images);

  ~MediaMetadataParser();

  // |callback| is called on same message loop.
  void Start(const MetadataCallback& callback);

 private:
  // Only accessed on |media_thread_| from this class.
  std::unique_ptr<media::DataSource> source_;

  const std::string mime_type_;

  bool get_attached_images_;

  // Thread that blocking media parsing operations run on while the main thread
  // handles messages from the browser process.
  // TODO(tommycli): Replace with a reference to a WorkerPool if we ever use
  // this class in batch mode.
  std::unique_ptr<base::Thread> media_thread_;

  DISALLOW_COPY_AND_ASSIGN(MediaMetadataParser);
};

}  // namespace metadata

#endif  // CHROME_UTILITY_MEDIA_GALLERIES_MEDIA_METADATA_PARSER_H_
