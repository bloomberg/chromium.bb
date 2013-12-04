// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_MEDIA_GALLERIES_MEDIA_METADATA_PARSER_H_
#define CHROME_UTILITY_MEDIA_GALLERIES_MEDIA_METADATA_PARSER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/api/media_galleries.h"

namespace metadata {

// This interface provides bytes needed by MediaMetadataParser.
class DataReader {
 public:
  typedef base::Callback<void(const std::string& bytes)> ReadBytesCallback;

  virtual ~DataReader() {}

  // Called on the utility thread.
  virtual void ReadBytes(int64 offset, int64 length,
                         const ReadBytesCallback& callback) = 0;
};

// This class takes a MIME type and data source and parses its metadata. It
// handles audio, video, and images. It delegates its operations to FFMPEG,
// libexif, etc. This class lives and operates on the utility thread of the
// utility process, as we wish to sandbox potentially dangerous operations
// on user-provided data.
class MediaMetadataParser {
 public:
  typedef extensions::api::media_galleries::MediaMetadata MediaMetadata;
  typedef base::Callback<void(scoped_ptr<MediaMetadata>)> MetadataCallback;

  // Takes ownership of |reader|. The MIME type is always sniffed in the browser
  // process, as it's faster (and safe enough) to sniff it there. When the user
  // wants more metadata than just the MIME type, this class provides it.
  MediaMetadataParser(DataReader* reader, const std::string& mime_type);

  ~MediaMetadataParser();

  // |callback| is called on same message loop.
  void Start(const MetadataCallback& callback);

 private:
  scoped_ptr<DataReader> reader_;

  MetadataCallback callback_;

  scoped_ptr<MediaMetadata> metadata_;

  DISALLOW_COPY_AND_ASSIGN(MediaMetadataParser);
};

}  // namespace metadata

#endif  // CHROME_UTILITY_MEDIA_GALLERIES_MEDIA_METADATA_PARSER_H_
