// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/parsers/metadata_parser_jpeg_factory.h"

#include "chrome/browser/parsers/metadata_parser_jpeg.h"
#include "base/logging.h"
#include "base/string_util.h"

MetadataParserJpegFactory::MetadataParserJpegFactory()
    : MetadataParserFactory() {
}

bool MetadataParserJpegFactory::CanParse(const FilePath& path,
                                         char* bytes,
                                         int bytes_size) {
#if defined(OS_WIN)
  FilePath::StringType ext = UTF8ToWide(std::string(".jpg"));
#elif defined(OS_POSIX)
  FilePath::StringType ext = ".jpg";
#endif
  return path.MatchesExtension(ext);
}

MetadataParser* MetadataParserJpegFactory::CreateParser(const FilePath& path) {
  JpegMetadataParser* parser;
  parser = new JpegMetadataParser(path);
  return parser;
}
