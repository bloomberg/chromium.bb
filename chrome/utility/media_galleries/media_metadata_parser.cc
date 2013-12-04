// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/media_metadata_parser.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace metadata {

MediaMetadataParser::MediaMetadataParser(DataReader* reader,
                                         const std::string& mime_type)
    : reader_(reader),
      metadata_(new MediaMetadata) {
  metadata_->mime_type = mime_type;
}

MediaMetadataParser::~MediaMetadataParser() {}

void MediaMetadataParser::Start(const MetadataCallback& callback) {
  DCHECK(callback_.is_null());
  callback_ = callback;

  // TODO(tommycli): Implement for various mime types.
  callback_.Run(metadata_.Pass());
}

}  // namespace metadata
