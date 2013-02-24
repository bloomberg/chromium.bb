// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/parsers/metadata_parser_jpeg_factory.h"

#include "base/files/file_path.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/parsers/metadata_parser_jpeg.h"

MetadataParserJpegFactory::MetadataParserJpegFactory()
    : MetadataParserFactory() {
}

bool MetadataParserJpegFactory::CanParse(const base::FilePath& path,
                                         char* bytes,
                                         int bytes_size) {
#if defined(OS_WIN)
  base::FilePath::StringType ext = UTF8ToWide(std::string(".jpg"));
#elif defined(OS_POSIX)
  base::FilePath::StringType ext = ".jpg";
#endif
  return path.MatchesExtension(ext);
}

MetadataParser* MetadataParserJpegFactory::CreateParser(
    const base::FilePath& path) {
  JpegMetadataParser* parser;
  parser = new JpegMetadataParser(path);
  return parser;
}
