// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/parsers/metadata_parser_jpeg.h"

JpegMetadataParser::JpegMetadataParser(const FilePath& path)
    : FileMetadataParser(path) {}

bool JpegMetadataParser::Parse() {
  FileMetadataParser::Parse();
  properties_[MetadataParser::kPropertyType] = "jpeg";
  return true;
}
